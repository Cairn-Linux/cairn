#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Moves a game from a Guardian's own Steam into the family folder,
/var/lib/cairn/steam-library, when a parent picks it for a child
(ADR-0042). The copy step then gives each child their own copy from there.

It writes in the Guardian's home, so it is meant to run as the Guardian
whose password opened the Save, and refuses to run as root. It is installed
as /usr/libexec/cairn/cairn-share-games beside kidscan.py and run with
`python3 -I -B`, so it finds kidscan's rules by its own folder. Nothing runs
it yet: cairn-family save-games will, in ADR-0042's second step.

It takes the appids of the games to move, and with none it only finishes or
undoes a move an earlier run left. It reads the Guardian's own Steam library
and each library their two lists name, except the family folder. A game
moves only if the copy step would then copy it: Steam has fully installed
it, on no beta branch, with nothing downloading, every account can read its
files, and the family folder has neither its manifest nor its folder.
Nothing moves while the Guardian's Steam is open, and only by reflink, so a
game on another drive stays where it is. Its Wine prefix, shader cache and
workshop items stay in the old library, and nothing ever moves back.

A move keeps ADR-0042's order. The game's folder is copied into a folder of
this run's own in the family's steamapps/common, and each file and folder
of the copy gives its group the owner's bits, as Steam's own installs there
do. The manifest is written in the family's steamapps under a temporary
name. Then the original manifest is set aside, the copy and the family
manifest take their names, and the original folder and the set-aside
manifest go. Ctrl-C and the other ending signals wait from the set-aside
until both names are taken. A run ended anywhere, by a kill or a power cut,
is finished or undone by the next one before it looks at any game, so a
game is never in two libraries; a run ended between the set-aside and the
renames leaves it in none until then. Two runs never overlap: each holds a
lock on the family's steamapps for the whole run, and a second run waits.

Before its first move it adds the family folder to both of the Guardian's
library lists, config/ and steamapps/libraryfolders.vdf, laid out as
Steam's own Storage step lays it out, and keeps each list as it was beside
it as libraryfolders.vdf.before-cairn. It takes the folder's contentid from
the folder's libraryfolder.vdf marker, or writes one. A list that names the
folder already is left alone.

It prints only these lines, made of fixed words and appids, so no game's
name and no path reaches the Guardian's window or the journal:

    library added           the family folder is now in the Guardian's
                            library lists
    library had             the lists named the family folder already, and
                            were left alone
    moved APPID             the game is in the family folder now, and only
                            there
    shared APPID            the game was in the family folder already, and
                            nothing changed
    not-moved APPID REASON  the game stays where it is, unchanged
    refused root            it was run as root, and did nothing
    refused appid           an appid was not a plain number, and it did
                            nothing

REASON is one word:

    steam-open         the Guardian's Steam is open
    not-found          no library of the Guardian's Steam has the game, or
                       its manifest cannot be read, names another appid,
                       or names one of Steam's own tools
    not-installed      Steam has not finished installing it, an update is
                       waiting, or its folder is missing
    downloading        Steam is downloading something for it
    installdir         its installdir is not one plain folder name
    beta-branch        it is on a beta branch a child's Steam may not have
    unreadable-files   some of its files only the Guardian can read
    exists             the family folder has its folder but no manifest
    other-drive        it cannot be copied by reflink from where it is
    no-shared          there is no family folder this account may change
    unreadable-config  a library list, or the family folder's marker,
                       could not be read or written
    failed             anything else: the copy failed, Steam changed the
                       manifest meanwhile, or the game is in two of the
                       Guardian's libraries

Every appid given gets exactly one line, unless a signal such as SIGTERM
ends the run: the games not yet said then get none, and the one being moved
may have moved. It exits 0 when it went to its end, 2 after "refused", and 1
after something unexpected, with "not-moved APPID failed" for each game not
yet said.
"""

import argparse
import fcntl
import os
import pwd
import re
import secrets
import shutil
import signal
import stat
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import kidscan  # noqa: E402

SHARED = Path("/var/lib/cairn/steam-library/steamapps")
STEAM = Path(".local/share/Steam")
PROC = Path("/proc")
CP = "/usr/bin/cp"

# The Guardian's two library lists, in their Steam folder; the marker in the
# family folder that names its contentid; and the copy of a list as it was.
LISTS = ("config/libraryfolders.vdf", "steamapps/libraryfolders.vdf")
MARKER = "libraryfolder.vdf"
BACKUP = ".before-cairn"

# Names only this program uses. kidscan refuses an installdir that starts
# .cairn-, and Steam reads only appmanifest_*.acf, so neither a game nor
# Steam has one. This run's own folder is in the family's common/, the
# temporary family manifest in its steamapps/, and the set-aside manifest
# in the steamapps/ the game moves from.
SCRATCH_PREFIX = ".cairn-move-"
TEMPORARY_PREFIX = ".cairn-move-manifest-"
ASIDE_PREFIX = ".cairn-move-aside-"
LIST_NEW = ".cairn-list-new"
MARKER_NEW = ".cairn-marker-new"
TEMPORARY_NAME = re.compile(rf"\.cairn-move-manifest-({kidscan.APPID.pattern})")
ASIDE_NAME = re.compile(rf"\.cairn-move-aside-({kidscan.APPID.pattern})")

# A process's folder in /proc.
PROCESS = re.compile(r"[0-9]+")
# A library's number in a list, and a size in its "apps" map.
NUMBERED = re.compile(r"[0-9]{1,9}")
SIZE = re.compile(r"[0-9]{1,20}")
# Valve documents no format for a contentid. Cairn only copies it, so a
# minus sign does no harm.
CONTENTID = re.compile(r"-?[0-9]{1,20}")

# The signals that end this program. They wait from the set-aside until the
# game has both its names in the family folder, so that a Ctrl-C never
# leaves it in no library at all.
ENDING_SIGNALS = {signal.SIGINT, signal.SIGTERM, signal.SIGHUP}


def say(*words):
    print(*words, flush=True)


def say_game(said, word, appid, reason=None):
    """One game's line, which no game gets twice."""
    said.add(appid)
    say(word, appid, *([reason] if reason else []))


def is_real_folder(path):
    """A folder, and not a link to one."""
    try:
        return stat.S_ISDIR(os.lstat(path).st_mode)
    except OSError:
        return False


def unlink_quietly(path):
    """Whether nothing is left under the name."""
    try:
        os.unlink(path)
    except FileNotFoundError:
        return True
    except OSError:
        return False
    return True


def remove(path):
    """Whatever is under the name, never followed: a real folder with all
    it holds, anything else as the entry itself."""
    if is_real_folder(path):
        shutil.rmtree(path)
    else:
        try:
            os.unlink(path)
        except FileNotFoundError:
            pass


def sync_folder(path):
    """A folder's entries on the disk, so that a power cut cannot undo a
    rename there that came before a change somewhere else."""
    descriptor = os.open(path, os.O_RDONLY | os.O_DIRECTORY)
    try:
        os.fsync(descriptor)
    finally:
        os.close(descriptor)


def read_vdf(data):
    """Bytes as parsed VDF, or None when they are not strict UTF-8 VDF."""
    if data is None:
        return None
    try:
        return kidscan.parse_vdf(data.decode("utf-8"))
    except (UnicodeDecodeError, ValueError):
        return None


def app_state(data):
    """A manifest's AppState block, or None when it cannot be read."""
    state = kidscan.ci_get(read_vdf(data) or {}, "AppState")
    return state if isinstance(state, dict) else None


def installdir_in(path, appid):
    """The installdir of a manifest of this program's own, when it is a
    plain file that names this appid and one plain folder name, or None."""
    state = app_state(kidscan.read_plain_file(path))
    if state is None or kidscan.ci_get(state, "appid") != appid:
        return None
    installdir = kidscan.ci_get(state, "installdir")
    return installdir if kidscan.plain_folder_name(installdir) else None


def steam_open(proc):
    """Whether a process of this account's Steam runs, by OwnSteam's rule: a
    numbered folder in /proc that this account owns, whose comm is exactly
    steam. Steam's helpers and the scripts that start it have other names.
    When /proc cannot be listed, Steam counts as open."""
    uid = os.getuid()
    try:
        entries = list(os.scandir(proc))
    except OSError:
        return True
    for entry in entries:
        try:
            if not PROCESS.fullmatch(entry.name) or entry.stat(follow_symlinks=False).st_uid != uid:
                continue
        except OSError:
            continue
        comm = kidscan.read_plain_file(os.path.join(entry.path, "comm"))
        if comm is not None and comm.strip() == b"steam":
            return True
    return False


class Family:
    """The family folder: its root, which holds the marker, its steamapps,
    and its common."""

    def __init__(self, steamapps):
        self.steamapps = steamapps
        self.root = steamapps.parent
        self.common = steamapps / "common"

    def manifest(self, appid):
        return self.steamapps / f"appmanifest_{appid}.acf"

    def temporary(self, appid):
        return self.steamapps / f"{TEMPORARY_PREFIX}{appid}"

    def usable(self):
        """Real folders, not links, and a steamapps this account may change."""
        return (is_real_folder(self.root) and is_real_folder(self.steamapps)
                and os.access(self.steamapps, os.W_OK | os.X_OK))


def listed_libraries(path):
    """The steamapps of each library one of Steam's lists names. A list that
    is not a plain file of a sensible size, or does not parse, names none."""
    text = kidscan.read_manifest(path)
    if text is None:
        return []
    try:
        folders = kidscan.ci_get(kidscan.parse_vdf(text), "libraryfolders", {})
    except ValueError:
        return []
    if not isinstance(folders, dict):
        return []
    listed = []
    for entry in folders.values():
        where = kidscan.ci_get(entry, "path") if isinstance(entry, dict) else entry
        if isinstance(where, str) and os.path.isabs(where) and "\0" not in where:
            listed.append(Path(where) / "steamapps")
    return listed


def libraries(home, family):
    """Each library of the Guardian's Steam a game can move from, as its
    steamapps folder, read as the tools step reads them: its own first, then
    each one its two lists name, each once by its real path, and never the
    family folder or anything in it. A steamapps or a common that is a link
    is passed over."""
    steam = home / STEAM
    candidates = [steam / "steamapps"]
    for name in LISTS:
        candidates += listed_libraries(steam / name)
    family_root = os.path.realpath(family.root)
    found = []
    seen = set()
    for steamapps in candidates:
        if not is_real_folder(steamapps) or not is_real_folder(steamapps / "common"):
            continue
        where = os.path.realpath(steamapps)
        if where in seen or os.path.commonpath([where, family_root]) == family_root:
            continue
        seen.add(where)
        found.append(steamapps)
    return found


class Game:
    """One game about to move: the library it is in, its manifest's bytes as
    read, its folder's name, and its size on disk for the library list."""

    def __init__(self, appid, library, data, installdir, size):
        self.appid = appid
        self.library = library
        self.data = data
        self.installdir = installdir
        self.size = size
        self.manifest = library / f"appmanifest_{appid}.acf"
        self.aside = library / f"{ASIDE_PREFIX}{appid}"
        self.folder = library / "common" / installdir


def readable_by_all(path, wanted):
    """cp copies a link as a link, and never reads where it leads."""
    details = os.lstat(path)
    return stat.S_ISLNK(details.st_mode) or details.st_mode & wanted == wanted


# The copy step reads the family folder as each child, so every account
# must be able to read every file of a game that moves there.
def all_readable(folder):
    folders_wanted = stat.S_IROTH | stat.S_IXOTH
    problems = []
    try:
        if not readable_by_all(folder, folders_wanted):
            return False
        for top, folders, files in os.walk(folder, onerror=problems.append):
            for name in folders:
                if not readable_by_all(os.path.join(top, name), folders_wanted):
                    return False
            for name in files:
                if not readable_by_all(os.path.join(top, name), stat.S_IROTH):
                    return False
    except OSError:
        return False
    return not problems


def decide(appid, family, found):
    """What becomes of one game before any copy: "shared", the reason it
    stays where it is, or the Game to move. Everything comes from one read
    of its manifest, and the reasons the copy step also gives come in the
    copy step's order, so one manifest gets the same word from both."""
    # The Guardian's own copy, if they have one, is left as it is.
    if os.path.lexists(family.manifest(appid)):
        return "shared"
    holding = [steamapps for steamapps in found if os.path.lexists(steamapps / f"appmanifest_{appid}.acf")]
    if not holding:
        return "not-found"
    # Moved from one, it would stay installed in the other as well as in the
    # family folder, and Steam would pick one of them to update.
    if len(holding) > 1:
        return "failed"
    library = holding[0]
    data = kidscan.read_plain_file(library / f"appmanifest_{appid}.acf")
    state = app_state(data)
    if state is None or kidscan.ci_get(state, "appid") != appid:
        return "not-found"
    # kidscan does not count Steam's own tools as games, and only games go
    # into the family folder (ADR-0046).
    name = kidscan.ci_get(state, "name", "")
    if not isinstance(name, str) or kidscan.is_steam_tool(appid, name):
        return "not-found"
    installdir = kidscan.ci_get(state, "installdir", "")
    if not kidscan.plain_folder_name(installdir):
        return "installdir"
    if kidscan.on_beta_branch(state):
        return "beta-branch"
    # 4 is fully installed with nothing pending, as the copy step asks.
    if kidscan.ci_get(state, "StateFlags") != "4":
        return "not-installed"
    # The copy step looks in the family folder's downloading/ too.
    if os.path.lexists(library / "downloading" / appid) or os.path.lexists(family.steamapps / "downloading" / appid):
        return "downloading"
    if not is_real_folder(library / "common" / installdir):
        return "not-installed"
    if os.path.lexists(family.common / installdir):
        return "exists"
    if not all_readable(library / "common" / installdir):
        return "unreadable-files"
    size = kidscan.ci_get(state, "SizeOnDisk")
    return Game(appid, library, data, installdir, size if isinstance(size, str) and SIZE.fullmatch(size) else "0")


# Each file keeps its run bits and its times, as the copy step's copies do,
# and takes the family folder's group, setgid and default ACL. What cp says
# could name a file in the home, so it is dropped.
def reflink_copy(source, target):
    done = subprocess.run(
        [CP, "-R", "--reflink=always", "--preserve=timestamps", "--", str(source), str(target)],
        stdin=subprocess.DEVNULL,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        check=False,
    )
    return done.returncode == 0


class Scratch:
    """This run's own folder in the family's common/, which no other account
    can enter, so no one can put a link where cp writes. It is made at the
    first probe, after recovery, so recovery never takes it for something an
    earlier run left."""

    def __init__(self, family):
        self.family = family
        self.path = None
        self.reflinks_from = {}

    def folder(self):
        if self.path is None:
            self.path = Path(tempfile.mkdtemp(prefix=SCRATCH_PREFIX, dir=self.family.common))
        return self.path

    def reflinks(self, game):
        """Whether cp can reflink from the game's library into the family
        folder, found out once for each library."""
        where = os.path.realpath(game.library)
        if where not in self.reflinks_from:
            probe = self.folder() / "probe"
            self.reflinks_from[where] = reflink_copy(game.manifest, probe)
            unlink_quietly(probe)
        return self.reflinks_from[where]

    def remove(self):
        if self.path is not None:
            shutil.rmtree(self.path, ignore_errors=True)


def raise_error(error):
    raise error


def let_the_group_write(copy):
    """Gives the group the owner's bits on each file and folder of a copy, as
    Steam's own installs in the family folder have them, so another
    Guardian's Steam can update the game too. The copy is in this run's own
    folder, so nothing changes between the lstat and the chmod."""
    paths = [copy]
    for top, folders, files in os.walk(copy, onerror=raise_error):
        paths += [os.path.join(top, name) for name in folders + files]
    for path in paths:
        mode = os.lstat(path).st_mode
        if not stat.S_ISLNK(mode):
            os.chmod(path, (stat.S_IMODE(mode) & ~stat.S_IRWXG) | ((mode & stat.S_IRWXU) >> 3))


def write_new(path, data, mode=0o664, exact=False):
    """A new file holding exactly these bytes, on the disk when this
    returns. O_EXCL never writes through a link or into a file that is
    there already, and a file left half written is removed. Without
    `exact`, the umask or the folder's default ACL has the last word on the
    mode, as for Steam's own files."""
    descriptor = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, mode)
    try:
        with os.fdopen(descriptor, "wb") as file:
            if exact:
                os.fchmod(file.fileno(), mode)
            file.write(data)
            file.flush()
            os.fsync(file.fileno())
    except OSError:
        unlink_quietly(path)
        raise


def put_in_place(game, family, copy):
    """ADR-0042's steps 3 and 4, with the ending signals held: the original
    manifest set aside, then the copy and the family manifest under their
    own names. True once the game is moved. Otherwise what was done is
    undone by one rule: the set-aside manifest goes back only once the
    temporary family manifest is gone, since the next run finishes a move
    whose copy has its full name and whose temporary manifest is there.
    When that manifest cannot be removed, everything stays as a kill would
    leave it, and the next run finishes the move or undoes it."""
    target = family.common / game.installdir
    temporary = family.temporary(game.appid)
    set_aside = False
    held = signal.pthread_sigmask(signal.SIG_BLOCK, ENDING_SIGNALS)
    try:
        os.rename(game.manifest, game.aside)
        set_aside = True
        # On the disk before the next two renames, in another folder, so a
        # power cut cannot leave the game in both libraries.
        sync_folder(game.library)
        os.rename(copy, target)
        try:
            os.rename(temporary, family.manifest(game.appid))
        except OSError:
            # Back into this run's own folder, which is removed, if it can.
            try:
                os.rename(target, copy)
            except OSError:
                pass
            raise
        return True
    except OSError:
        if unlink_quietly(temporary) and set_aside:
            try:
                os.rename(game.aside, game.manifest)
            except OSError:
                pass
        return False
    finally:
        signal.pthread_sigmask(signal.SIG_SETMASK, held)


def move(game, family, scratch, proc):
    """ADR-0042's steps 1 to 4 for one game: None once it is in the family
    folder under its own names, or the reason it stays where it is, with
    nothing of the attempt left behind."""
    if steam_open(proc):
        return "steam-open"
    copy = scratch.folder() / game.appid
    temporary = family.temporary(game.appid)
    written = placing = False
    try:
        if not reflink_copy(game.folder, copy):
            return "failed"
        let_the_group_write(copy)
        # A manifest Steam changed during the copy means the copy may hold
        # parts of two versions.
        if kidscan.read_plain_file(game.manifest) != game.data:
            return "failed"
        # Another Guardian's Steam may have installed it there meanwhile.
        if os.path.lexists(family.common / game.installdir) or os.path.lexists(family.manifest(game.appid)):
            return "exists"
        write_new(temporary, game.data)
        written = True
        # The copy and its manifest are on the disk before anything in the
        # Guardian's library changes, so a power cut can never keep the
        # removal of the original and lose the copy.
        os.sync()
        if steam_open(proc):
            return "steam-open"
        placing = True
        return None if put_in_place(game, family, copy) else "failed"
    except OSError:
        return "failed"
    finally:
        # Only ever this run's own copy, still in this run's own folder.
        if is_real_folder(copy):
            shutil.rmtree(copy, ignore_errors=True)
        if written and not placing:
            unlink_quietly(temporary)


def finish(game, family):
    """ADR-0042's step 5: the original folder, then the set-aside manifest,
    once the family folder's renames are on the disk. Whatever cannot go
    now stays with the set-aside manifest, and the next run tries again."""
    try:
        sync_folder(family.common)
        sync_folder(family.steamapps)
        remove(game.folder)
        os.unlink(game.aside)
        sync_folder(game.library / "common")
        sync_folder(game.library)
    except OSError:
        pass


def names_folder(parsed, root):
    """Whether a library list names the family folder, by real path, in an
    entry's path or as an old list's bare path."""
    folders = kidscan.ci_get(parsed, "libraryfolders")
    if not isinstance(folders, dict):
        return False
    wanted = os.path.realpath(root)
    for entry in folders.values():
        where = kidscan.ci_get(entry, "path") if isinstance(entry, dict) else entry
        if isinstance(where, str) and os.path.isabs(where) and "\0" not in where:
            if os.path.realpath(where) == wanted:
                return True
    return False


def contentids(parsed):
    """Every contentid a library list holds."""
    folders = kidscan.ci_get(parsed, "libraryfolders")
    found = set()
    for entry in folders.values() if isinstance(folders, dict) else []:
        contentid = kidscan.ci_get(entry, "contentid") if isinstance(entry, dict) else None
        if isinstance(contentid, str):
            found.add(contentid)
    return found


def entry_text(number, entry):
    """One library's entry, laid out as Steam lays out its own."""
    lines = [f'\t"{number}"', "\t{"]
    lines += [f'\t\t"{key}"\t\t"{entry[key]}"' for key in ("path", "label", "contentid", "totalsize")]
    lines += ['\t\t"apps"', "\t\t{"]
    lines += [f'\t\t\t"{appid}"\t\t"{size}"' for appid, size in entry["apps"].items()]
    lines += ["\t\t}", "\t}"]
    return ("\n".join(lines) + "\n").encode()


def with_entry(data, parsed, entry):
    """A list's bytes with the entry put in before its last closing brace,
    under the next number, so none of Steam's own text changes. None when
    the list is not one entry blocks under one libraryfolders block, among
    them "0", closed by its last brace, or when the new text would not
    parse to the old entries and exactly the new one."""
    if len(parsed) != 1:
        return None
    ((key, folders),) = parsed.items()
    if key.lower() != "libraryfolders" or not isinstance(folders, dict) or "0" not in folders:
        return None
    numbered = [name for name in folders if NUMBERED.fullmatch(name)]
    if not all(isinstance(folders[name], dict) for name in numbered):
        return None
    end = len(data.rstrip())
    if not data[:end].endswith(b"}"):
        return None
    number = str(max(int(name) for name in numbered) + 1)
    before = data[:end - 1]
    text = before + (b"" if before.endswith(b"\n") else b"\n") + entry_text(number, entry) + data[end - 1:]
    expected = read_vdf(data)
    expected[key][number] = entry
    return text if read_vdf(text) == expected else None


def read_marker(path):
    """The contentid in the family folder's marker, "" when there is no
    marker, or None when there is one this program cannot read, which it
    never writes over."""
    if not os.path.lexists(path):
        return ""
    block = kidscan.ci_get(read_vdf(kidscan.read_plain_file(path)) or {}, "libraryfolder")
    contentid = kidscan.ci_get(block, "contentid") if isinstance(block, dict) else None
    return contentid if isinstance(contentid, str) and CONTENTID.fullmatch(contentid) else None


def new_contentid(used):
    """A new random 64-bit contentid, from the operating system's secure
    source, that no library in the lists has."""
    while True:
        contentid = str(secrets.randbelow(2**63 - 1) + 1)
        if contentid not in used:
            return contentid


def put_file(path, temporary, data, mode=None):
    """A file replaced whole: the new bytes under a temporary name, on the
    disk, then renamed over the old name, so a reader sees the old file or
    the new one, never half of one. With no mode it asks for 0664, and the
    folder's default ACL decides, as for Steam's own files."""
    write_new(temporary, data, 0o664 if mode is None else mode, exact=mode is not None)
    try:
        os.replace(temporary, path)
    except OSError:
        unlink_quietly(temporary)
        raise
    sync_folder(path.parent)


def add_family_folder(steam, family, games):
    """Adds the family folder to the Guardian's library lists, once, as one
    entry whose "apps" map holds the games about to move: "added", "had"
    when every list names it already, or None when a list or the marker
    cannot be read or written. Both lists are read and checked, and both new
    texts made and checked, before anything is written. Then the marker
    goes first, then config/, then steamapps/, each list with its copy as it
    was, so a run ended between them leaves lists the next run completes
    with the same contentid."""
    lists = []
    for name in LISTS:
        path = steam / name
        if os.path.lexists(path):
            data = kidscan.read_plain_file(path)
            parsed = read_vdf(data)
            if parsed is None:
                return None
            lists.append((path, data, parsed))
    if not lists:
        return None
    wanted = [(path, data, parsed) for path, data, parsed in lists if not names_folder(parsed, family.root)]
    if not wanted:
        return "had"
    contentid = read_marker(family.root / MARKER)
    if contentid is None:
        return None
    marker = None
    if not contentid:
        contentid = new_contentid(set().union(*(contentids(parsed) for _, _, parsed in lists)))
        marker = f'"libraryfolder"\n{{\n\t"contentid"\t\t"{contentid}"\n\t"label"\t\t""\n}}\n'.encode()
    entry = {"path": os.path.abspath(family.root), "label": "", "contentid": contentid, "totalsize": "0",
             "apps": {game.appid: game.size for game in games}}
    changes = []
    for path, data, parsed in wanted:
        text = with_entry(data, parsed, entry)
        if text is None:
            return None
        changes.append((path, data, text))
    try:
        if marker is not None:
            put_file(family.root / MARKER, family.root / MARKER_NEW, marker)
        for path, old, new in changes:
            mode = stat.S_IMODE(os.lstat(path).st_mode)
            put_file(path.with_name(path.name + BACKUP), path.parent / LIST_NEW, old, mode)
            put_file(path, path.parent / LIST_NEW, new, mode)
    except OSError:
        return None
    return "added"


def recover_family_side(family, found):
    """Each temporary family manifest an earlier run left, its own or
    another Guardian's: a move whose copy has its full name is finished,
    and anything else goes."""
    for entry in sorted(os.scandir(family.steamapps), key=lambda entry: entry.name):
        match = TEMPORARY_NAME.fullmatch(entry.name)
        if not match:
            continue
        appid = match.group(1)
        temporary = Path(entry.path)
        installdir = installdir_in(temporary, appid)
        copy = family.common / installdir if installdir else None
        if copy is None or os.path.lexists(family.manifest(appid)) or not is_real_folder(copy):
            # Not one of this program's, the family folder has the game, or
            # the move never reached step 4.
            unlink_quietly(temporary)
        elif any(os.path.lexists(steamapps / f"appmanifest_{appid}.acf") for steamapps in found):
            # Steam installed the game again meanwhile, so finishing would
            # leave it in two libraries.
            if unlink_quietly(temporary):
                shutil.rmtree(copy, ignore_errors=True)
        else:
            try:
                os.rename(temporary, family.manifest(appid))
            except OSError:
                pass


def recover_library_side(family, steamapps):
    """Each set-aside manifest an earlier run left in one of this Guardian's
    libraries. One this program cannot read is left as it is."""
    try:
        entries = sorted(os.scandir(steamapps), key=lambda entry: entry.name)
    except OSError:
        return
    for entry in entries:
        match = ASIDE_NAME.fullmatch(entry.name)
        if not match:
            continue
        appid = match.group(1)
        aside = Path(entry.path)
        installdir = installdir_in(aside, appid)
        if installdir is None:
            continue
        manifest = steamapps / f"appmanifest_{appid}.acf"
        if os.path.lexists(family.manifest(appid)):
            # The original goes only once the family folder has the whole
            # game, and never when Steam installed it here again meanwhile.
            if not is_real_folder(family.common / installdir):
                continue
            try:
                if not os.path.lexists(manifest):
                    remove(steamapps / "common" / installdir)
                os.unlink(aside)
            except OSError:
                continue
        elif os.path.lexists(family.temporary(appid)):
            continue
        elif os.path.lexists(manifest):
            unlink_quietly(aside)
        else:
            try:
                os.rename(aside, manifest)
            except OSError:
                pass
    try:
        sync_folder(steamapps / "common")
        sync_folder(steamapps)
    except OSError:
        pass


def recover(family, found, steam):
    """Finishes or undoes whatever an earlier run left, by ADR-0042's rule,
    before any game is looked at, then removes this program's leftovers. It
    says nothing."""
    recover_family_side(family, found)
    # The family folder's renames are on the disk before any original goes.
    try:
        sync_folder(family.common)
        sync_folder(family.steamapps)
    except OSError:
        return
    for steamapps in found:
        recover_library_side(family, steamapps)
    for entry in list(os.scandir(family.common)):
        if entry.name.startswith(SCRATCH_PREFIX):
            if entry.is_dir(follow_symlinks=False):
                # Another Guardian's own folder waits for their next run.
                shutil.rmtree(entry.path, ignore_errors=True)
            else:
                unlink_quietly(entry.path)
    for path in (steam / "config" / LIST_NEW, steam / "steamapps" / LIST_NEW, family.root / MARKER_NEW):
        unlink_quietly(path)


def move_games(games, home, family, scratch, proc, said):
    """The family folder into the lists, once, then each game in turn."""
    if steam_open(proc):
        for game in games:
            say_game(said, "not-moved", game.appid, "steam-open")
        return
    listed = add_family_folder(home / STEAM, family, games)
    if listed is None:
        for game in games:
            say_game(said, "not-moved", game.appid, "unreadable-config")
        return
    say("library", listed)
    for game in games:
        reason = move(game, family, scratch, proc)
        if reason is not None:
            say_game(said, "not-moved", game.appid, reason)
            continue
        say_game(said, "moved", game.appid)
        finish(game, family)


def share_games(appids, home, family, proc, said):
    """The run, under the lock that keeps a second run waiting."""
    if not family.usable():
        for appid in appids:
            say_game(said, "not-moved", appid, "no-shared")
        return 0
    lock = os.open(family.steamapps, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    try:
        fcntl.flock(lock, fcntl.LOCK_EX)
        # Nothing is written while the Guardian's Steam is open, since Steam
        # writes its library list itself.
        if steam_open(proc):
            for appid in appids:
                if os.path.lexists(family.manifest(appid)):
                    say_game(said, "shared", appid)
                else:
                    say_game(said, "not-moved", appid, "steam-open")
            return 0
        # As Steam's first install there would.
        try:
            os.mkdir(family.common, 0o775)
        except OSError:
            pass
        if not is_real_folder(family.common) or not os.access(family.common, os.W_OK | os.X_OK):
            for appid in appids:
                say_game(said, "not-moved", appid, "no-shared")
            return 0
        found = libraries(home, family)
        recover(family, found, home / STEAM)
        scratch = Scratch(family)
        try:
            games = []
            for appid in appids:
                decision = decide(appid, family, found)
                if isinstance(decision, Game):
                    if scratch.reflinks(decision):
                        games.append(decision)
                    else:
                        say_game(said, "not-moved", appid, "other-drive")
                elif decision == "shared":
                    say_game(said, "shared", appid)
                else:
                    say_game(said, "not-moved", appid, decision)
            if games:
                move_games(games, home, family, scratch, proc, said)
        finally:
            scratch.remove()
    finally:
        os.close(lock)
    return 0


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("appids", nargs="*", metavar="APPID", help="the games to move")
    # Only the tests move these.
    parser.add_argument("--home", type=Path, default=None, help=argparse.SUPPRESS)
    parser.add_argument("--shared", type=Path, default=SHARED, help=argparse.SUPPRESS)
    parser.add_argument("--proc", type=Path, default=PROC, help=argparse.SUPPRESS)
    args = parser.parse_args(argv)

    # Checked first, so that one bad appid changes nothing, not even what an
    # earlier run left.
    if not all(kidscan.APPID.fullmatch(appid) for appid in args.appids):
        say("refused", "appid")
        return 2
    # As root, a link planted in a home could send a write anywhere.
    if os.geteuid() == 0 or os.getuid() == 0:
        say("refused", "root")
        return 2
    os.umask(0o022)
    appids = sorted(set(args.appids), key=int)
    said = set()
    try:
        # Never $HOME, which whoever starts this could set.
        home = args.home or Path(pwd.getpwuid(os.getuid()).pw_dir)
        return share_games(appids, home, Family(args.shared), args.proc, said)
    except Exception:  # noqa: BLE001 - its words could name a file in the home
        for appid in appids:
            if appid not in said:
                say("not-moved", appid, "failed")
        return 1


if __name__ == "__main__":
    sys.exit(main())
