#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Copies each game in the shared Steam library into this account's own
Steam library, by reflink (ADR-0033), or only the games a list names
(ADR-0041).

Provisioning runs it as each child, never as root, through the visit
helper's copy mode (`cairn-steam-visit --copy-games CHILD --all`), with
the child logged out and listed so that they cannot log in meanwhile. It is installed
as /usr/local/libexec/cairn/cairn-copy-games beside kidscan.py and run with
`python3 -I`, which keeps the child's files off Python's path, so it finds
kidscan's VDF parser by its own folder.

With `--only APPID...` it copies only those games: one child's own list.
Without it, it copies every game in the shared library, which provisioning
on the kids' laptop still relies on. An appid that is not a plain number
stops it before anything changes.

It prints only these lines, one per game or condition, made of fixed words
and appids, so nothing from the child's home reaches the Guardian's
terminal. Provisioning turns them into sentences of its own.

    copied APPID           the game is now in the child's library
    have APPID             the child has it already, and nothing changed
    skipped APPID REASON   left out; REASON is one word, such as
                           not-installed or downloading, or not-shared
                           when a listed game is not in the shared library
    unnumbered             a manifest whose file name has no appid
    no-library             the child's Steam has not run yet
    no-shared              there is no shared library
    lists-shared           the child's Steam lists the shared library
    unreadable-config      the child's Steam's library list is not a plain
                           file of a sensible size, so nothing is copied
    no-reflink             the disk cannot reflink, so nothing is copied
    refused root           it was run as root, and did nothing
    refused appid          a listed appid was not a plain number, and it
                           did nothing

It exits 0 when it did what it could; a game left out is not a failure.
"""

import argparse
import os
import pwd
import re
import shutil
import signal
import stat
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import kidscan  # noqa: E402

SHARED = Path("/var/lib/cairn/steam-library/steamapps")
OWN = Path(".local/share/Steam/steamapps")
CP = "/usr/bin/cp"

# Names only this program uses. Whatever an earlier run left under them is
# removed before anything else; nothing else in the child's library is.
COPY_PREFIX = ".cairn-copy-"
MANIFEST_PREFIX = ".cairn-manifest-"
PROBE = ".cairn-copy-probe"
LEFTOVER_IN_COMMON = re.compile(r"\.cairn-copy-([0-9]+|probe)")
LEFTOVER_IN_LIBRARY = re.compile(r"\.cairn-manifest-[0-9]+")

# A real manifest, or library list, is a few kilobytes.
MANIFEST_LIMIT = 1 << 20

# The signals that end this program. They wait while a game goes into
# place, so that a Ctrl-C cannot leave a folder without its manifest.
ENDING_SIGNALS = {signal.SIGINT, signal.SIGTERM, signal.SIGHUP}


def say(*words):
    print(*words, flush=True)


def read_manifest(path):
    with open(path, "rb") as file:
        return file.read(MANIFEST_LIMIT + 1)


def app_state(data):
    """The manifest's AppState block, or None when it cannot be read."""
    if len(data) > MANIFEST_LIMIT:
        return None
    try:
        state = kidscan.ci_get(kidscan.parse_vdf(data.decode("utf-8")), "AppState")
    except (UnicodeDecodeError, ValueError):
        return None
    return state if isinstance(state, dict) else None


def is_real_folder(path):
    return path.is_dir() and not path.is_symlink()


def decide(appid, data, shared, library):
    """What to do with one shared game: ("have",), ("skipped", reason) or
    ("copy", installdir). Everything comes from the one read of its manifest."""
    state = app_state(data)
    if state is None:
        return ("skipped", "unreadable")
    if kidscan.ci_get(state, "appid") != appid:
        return ("skipped", "appid")
    # The child's copy holds their saves: never replaced, updated or deleted.
    if os.path.lexists(library / f"appmanifest_{appid}.acf"):
        return ("have",)
    installdir = kidscan.ci_get(state, "installdir", "")
    # kidscan holds the rules the game tiles share, so the two agree.
    if not kidscan.plain_folder_name(installdir):
        return ("skipped", "installdir")
    if kidscan.on_beta_branch(state):
        return ("skipped", "beta-branch")
    # 4 is fully installed with nothing pending.
    if kidscan.ci_get(state, "StateFlags") != "4":
        return ("skipped", "not-installed")
    if os.path.lexists(shared / "downloading" / appid):
        return ("skipped", "downloading")
    if not is_real_folder(shared / "common" / installdir):
        return ("skipped", "no-folder")
    # Without a manifest Steam does not know it, but it may hold saves.
    if os.path.lexists(library / "common" / installdir):
        return ("skipped", "folder-without-manifest")
    return ("copy", installdir)


def read_library_list(path):
    """One of the child's library lists: its text, "" when there is none, or
    None when it is not a plain file of a sensible size. Anything in the
    child's home may have been put there to stop this program: a FIFO would
    block the open for ever, and a link to /dev/zero never ends."""
    try:
        descriptor = os.open(path, os.O_RDONLY | os.O_NONBLOCK | os.O_NOFOLLOW)
    except FileNotFoundError:
        return ""
    except OSError:
        return None
    try:
        if not stat.S_ISREG(os.fstat(descriptor).st_mode):
            return None
        data = os.read(descriptor, MANIFEST_LIMIT + 1)
    except OSError:
        return None
    finally:
        os.close(descriptor)
    if len(data) > MANIFEST_LIMIT:
        return None
    return data.decode("utf-8", errors="replace")


def lists_shared(steam, shared_root):
    """Says "lists-shared" when the child's own Steam lists the shared library,
    since it would then see each game twice and play the shared one,
    "unreadable-config" when a list is not a plain file, or None."""
    wanted = os.path.normpath(shared_root)
    for name in ("config/libraryfolders.vdf", "steamapps/libraryfolders.vdf"):
        text = read_library_list(steam / name)
        if text is None:
            return "unreadable-config"
        if not text:
            continue
        try:
            folders = kidscan.ci_get(kidscan.parse_vdf(text), "libraryfolders", {})
        except ValueError:
            if wanted in text:
                return "lists-shared"
            continue
        if not isinstance(folders, dict):
            continue
        for entry in folders.values():
            path = kidscan.ci_get(entry, "path") if isinstance(entry, dict) else entry
            if isinstance(path, str) and path and os.path.normpath(path) == wanted:
                return "lists-shared"
    return None


def remove(path):
    if path.is_dir() and not path.is_symlink():
        shutil.rmtree(path)
    else:
        path.unlink(missing_ok=True)


def remove_leftovers(library):
    for folder, leftover in ((library / "common", LEFTOVER_IN_COMMON), (library, LEFTOVER_IN_LIBRARY)):
        for entry in folder.iterdir():
            if leftover.fullmatch(entry.name):
                remove(entry)


# Each file keeps its executable bits and times, but not the shared
# folder's ACLs or setgid, which mean nothing in the child's home. What cp
# says could name a file, so it is dropped.
def reflink_copy(source, target):
    done = subprocess.run(
        [CP, "-R", "--reflink=always", "--preserve=timestamps", "--", str(source), str(target)],
        stdin=subprocess.DEVNULL,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        check=False,
    )
    return done.returncode == 0


def can_reflink(manifest, common):
    probe = common / PROBE
    works = reflink_copy(manifest, probe)
    remove(probe)
    return works


def can_read(path, wanted):
    # cp copies a link as a link, and never reads where it points.
    return os.path.islink(path) or os.access(path, wanted)


# When cp fails, the likeliest lasting reason is a file of the shared game
# that this account cannot read, and that reason gets its own word.
def all_readable(folder):
    problems = []
    for top, folders, files in os.walk(folder, onerror=problems.append):
        for name in folders:
            if not can_read(os.path.join(top, name), os.R_OK | os.X_OK):
                return False
        for name in files:
            if not can_read(os.path.join(top, name), os.R_OK):
                return False
    return not problems


def write_manifest(data, path):
    """The manifest exactly as read, whole, under a name Steam ignores."""
    descriptor = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o644)
    with os.fdopen(descriptor, "wb") as file:
        file.write(data)
        file.flush()
        os.fsync(file.fileno())


def put_in_place(folder, target, manifest, manifest_target):
    """The folder, then its manifest, one right after the other, with the
    ending signals held until both are done or the folder is back out."""
    held = signal.pthread_sigmask(signal.SIG_BLOCK, ENDING_SIGNALS)
    try:
        os.rename(folder, target)
        try:
            os.rename(manifest, manifest_target)
        except OSError:
            os.rename(target, folder)
            raise
    finally:
        signal.pthread_sigmask(signal.SIG_SETMASK, held)


def sync_folder(path):
    descriptor = os.open(path, os.O_RDONLY | os.O_DIRECTORY)
    try:
        os.fsync(descriptor)
    finally:
        os.close(descriptor)


def copy_game(appid, data, manifest, installdir, shared, library):
    """The folder and the manifest under temporary names, then the folder
    into place, then the manifest: Steam never sees a manifest for half a
    game. None when it is copied, or the reason it is not."""
    temporary = library / "common" / f"{COPY_PREFIX}{appid}"
    target = library / "common" / installdir
    temporary_manifest = library / f"{MANIFEST_PREFIX}{appid}"
    try:
        if not reflink_copy(shared / "common" / installdir, temporary):
            remove(temporary)
            return "copy-failed" if all_readable(shared / "common" / installdir) else "unreadable-files"
        try:
            unchanged = read_manifest(manifest) == data
        except OSError:
            unchanged = False
        if not unchanged:
            remove(temporary)
            return "changed"
        if os.path.lexists(target) or os.path.lexists(library / f"appmanifest_{appid}.acf"):
            remove(temporary)
            return "folder-without-manifest"
        write_manifest(data, temporary_manifest)
        put_in_place(temporary, target, temporary_manifest, library / f"appmanifest_{appid}.acf")
    except OSError:
        remove(temporary)
        remove(temporary_manifest)
        return "copy-failed"
    # So that a power cut cannot keep one rename and lose the other. The
    # game is in place whatever this says.
    try:
        sync_folder(library / "common")
        sync_folder(library)
    except OSError:
        pass
    return None


def manifests(shared, only):
    """Each game to look at, as (appid, manifest path), in appid order for a
    list and file name order for the whole shared library. A listed game
    whose manifest is missing still comes, so that it can be reported."""
    if only is not None:
        return [(appid, shared / f"appmanifest_{appid}.acf") for appid in sorted(only, key=int)]
    found = []
    for manifest in sorted(shared.glob("appmanifest_*.acf")):
        found.append((manifest.name.removeprefix("appmanifest_").removesuffix(".acf"), manifest))
    return found


def copy_games(shared, library, only=None):
    (library / "common").mkdir(mode=0o755, exist_ok=True)
    remove_leftovers(library)
    reflink = None
    for appid, manifest in manifests(shared, only):
        if not kidscan.APPID.fullmatch(appid):
            say("unnumbered")
            continue
        if only is not None and not os.path.lexists(manifest):
            say("skipped", appid, "not-shared")
            continue
        try:
            data = read_manifest(manifest)
        except OSError:
            say("skipped", appid, "unreadable")
            continue
        decision = decide(appid, data, shared, library)
        if decision[0] == "have":
            say("have", appid)
            continue
        if decision[0] == "skipped":
            say("skipped", appid, decision[1])
            continue
        # Without reflink each copy would be a full one, and fill the disk.
        if reflink is None:
            reflink = can_reflink(manifest, library / "common")
        if not reflink:
            say("no-reflink")
            return
        reason = copy_game(appid, data, manifest, decision[1], shared, library)
        if reason is None:
            say("copied", appid)
        else:
            say("skipped", appid, reason)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    # Only the tests move these; provisioning passes nothing.
    parser.add_argument("--shared", type=Path, default=SHARED, help=argparse.SUPPRESS)
    parser.add_argument("--library", type=Path, default=None, help=argparse.SUPPRESS)
    parser.add_argument("--only", nargs="+", metavar="APPID", help="copy only these games")
    args = parser.parse_args(argv)

    # Checked first, so that a list with one bad appid changes nothing.
    only = None
    if args.only is not None:
        if not all(kidscan.APPID.fullmatch(appid) for appid in args.only):
            say("refused", "appid")
            return 2
        only = set(args.only)

    # As root a link planted in a child's home could send a write anywhere.
    if os.geteuid() == 0 or os.getuid() == 0:
        say("refused", "root")
        return 2
    os.umask(0o022)
    # Never $HOME, which whoever starts this could set.
    library = args.library or Path(pwd.getpwuid(os.getuid()).pw_dir) / OWN
    if not library.is_dir():
        say("no-library")
        return 0
    if not args.shared.is_dir():
        say("no-shared")
        return 0
    listed = lists_shared(library.parent, args.shared.parent)
    if listed is not None:
        say(listed)
        return 0
    copy_games(args.shared, library, only)
    return 0


if __name__ == "__main__":
    sys.exit(main())
