#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Fills the tools folder, /var/lib/cairn/steam-tools, with Steam's own
tools from a Guardian's Steam: its Steam Linux Runtimes and its Protons
(ADR-0046).

A child's games are copies, so the child's own Steam never installs them,
and would first meet the tools they run in at the child's first tap. On the
image, each Save and each look for new games runs this program as the
Guardian whose password opened it, never as root: cairn-family save-games
starts it as a short system job (`systemd-run --uid=LOGIN`), which systemd
stops after five minutes. The copy step then copies each tool a child lacks
from the tools folder into the child's own Steam library, with the child's
new games. It is installed as /usr/libexec/cairn/cairn-share-tools beside
kidscan.py and run with `python3 -I -B`, so it finds kidscan's rules by its
own folder.

It reads every library of the Guardian's Steam. An app there is a tool when
kidscan's rule calls it one and its folder holds a toolmanifest.vdf, and it
passes over everything else without a word. A tool goes into the tools
folder only when Steam has fully installed it, with no update waiting and
no beta branch, and every account can read every file in it but those in
its own var/. Each build of a tool is a folder of its own,
<appid>-<buildid>, holding the tool's manifest byte for byte and its
common/<installdir>/. It is made inside a folder only this run uses and
renamed into place whole, so a reader sees a whole build or none. Its var/,
where the runtime keeps each account's own working files, is never copied.
Once a build is in place, the folder's other builds of that tool go. It
writes nothing in the Guardian's home, so their Steam can stay open.

It prints only these lines, made of fixed words and appids, so no game's
name and no path reaches the Guardian's window or the journal:

    tool-shared APPID             the Guardian's build is now in the tools
                                  folder
    tool-had APPID                the tools folder had that build, or a
                                  newer one
    tool-kept APPID REASON        left out for REASON; the tools folder
                                  keeps the build it had, and children get
                                  that one
    tool-not-shared APPID REASON  left out for REASON, and the tools folder
                                  has no build of it
    no-steam                      the Guardian's Steam has no library
    refused root                  it was run as root, and did nothing
    refused folder                the tools folder is not a folder this
                                  account may change, and nothing changed
    finished                      the last line of a run that went to the
                                  end
    failed                        it stopped on something unexpected; what
                                  it was could name a file in the home, so
                                  it is not said

REASON is one word: not-installed (an update is waiting, or Steam has not
finished installing it), beta-branch, downloading, no-build (its manifest
names no build), unreadable-files (some of its files only the Guardian can
read), other-drive (it cannot be copied by reflink from where it is),
copy-failed, or changed (Steam changed it during the copy).

It exits 0 after "finished", and 1 otherwise.
"""

import argparse
import os
import pwd
import re
import shutil
import stat
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import kidscan  # noqa: E402

TOOLS = Path("/var/lib/cairn/steam-tools")
STEAM = Path(".local/share/Steam")
CP = "/usr/bin/cp"

# Where a runtime keeps each account's own working files: pressure-vessel's
# temporary folders, some of them private. Never copied.
VAR = "var"

# Names only this program makes in the tools folder, which no reader takes.
# Each run works inside a folder of its own under the first, with a name no
# one can guess, and first removes whatever an earlier run left.
SCRATCH_PREFIX = ".cairn-tool-"
GONE_PREFIX = ".cairn-tool-gone-"

# A Steam manifest's file name, by kidscan's rule for an appid. A build's
# folder is named by kidscan's TOOL_BUILD, which the copy step reads too.
MANIFEST_NAME = re.compile(rf"appmanifest_({kidscan.APPID.pattern})\.acf")


def say(*words):
    print(*words, flush=True)


def is_real_folder(path):
    """A folder, and not a link to one."""
    try:
        return stat.S_ISDIR(os.lstat(path).st_mode)
    except OSError:
        return False


def unlink(path):
    try:
        os.unlink(path)
    except OSError:
        pass


def app_state(data):
    """The manifest's AppState block, or None when it cannot be read."""
    try:
        state = kidscan.ci_get(kidscan.parse_vdf(data.decode("utf-8")), "AppState")
    except (UnicodeDecodeError, ValueError):
        return None
    return state if isinstance(state, dict) else None


def listed_libraries(path):
    """The steamapps of each library one of Steam's library lists names. A
    list that is not a plain file of a sensible size, or does not parse,
    names none."""
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


def libraries(home):
    """Each library of the Guardian's Steam, as its steamapps folder: its own
    first, then each library its two lists name, each once. A steamapps or
    a common that is a link is passed over."""
    steam = home / STEAM
    candidates = [steam / "steamapps"]
    for name in ("config/libraryfolders.vdf", "steamapps/libraryfolders.vdf"):
        candidates += listed_libraries(steam / name)
    found = []
    seen = set()
    for steamapps in candidates:
        if not is_real_folder(steamapps) or not is_real_folder(steamapps / "common"):
            continue
        where = os.path.realpath(steamapps)
        if where not in seen:
            seen.add(where)
            found.append(steamapps)
    return found


def tools_in(steamapps):
    """Each of Steam's tools in one library, by kidscan's rule for a tool, in
    appid order, as (appid, manifest bytes, AppState, installdir). Anything
    else is passed over without a word, so no game of the Guardian's is ever
    named."""
    try:
        entries = sorted(os.scandir(steamapps), key=lambda entry: entry.name)
    except OSError:
        return []
    found = []
    for entry in entries:
        match = MANIFEST_NAME.fullmatch(entry.name)
        data = kidscan.read_plain_file(entry.path) if match else None
        state = app_state(data) if data is not None else None
        if state is None or kidscan.ci_get(state, "appid") != match.group(1):
            continue
        installdir = kidscan.tool_installdir(match.group(1), state, steamapps)
        if installdir is not None:
            found.append((match.group(1), data, state, installdir))
    return sorted(found, key=lambda tool: int(tool[0]))


def build_of(state):
    build = kidscan.ci_get(state, "buildid")
    return int(build) if isinstance(build, str) and kidscan.APPID.fullmatch(build) else None


def reason_in_manifest(appid, state, steamapps):
    """The first reason a tool stays out that its manifest and its library
    give, or None."""
    # 4 is fully installed with nothing pending, as the copy step asks.
    if kidscan.ci_get(state, "StateFlags") != "4":
        return "not-installed"
    if kidscan.on_beta_branch(state):
        return "beta-branch"
    if os.path.lexists(steamapps / "downloading" / appid):
        return "downloading"
    if build_of(state) is None:
        return "no-build"
    return None


def builds_held(tools, appid):
    """The builds of one tool the tools folder holds, as numbers. Only a real
    folder counts: a link or a file under a build's name is never one."""
    held = []
    for entry in os.scandir(tools):
        match = kidscan.TOOL_BUILD.fullmatch(entry.name)
        if match and match.group(1) == appid and entry.is_dir(follow_symlinks=False):
            held.append(int(match.group(2)))
    return held


def readable_by_all(path, wanted):
    """cp copies a link as a link, and never reads where it leads."""
    details = os.lstat(path)
    return stat.S_ISLNK(details.st_mode) or details.st_mode & wanted == wanted


# The copy step reads the tools folder as each child, so every account must
# be able to read what goes into it. The tool's own var/ is never copied.
def all_readable(folder):
    folders_wanted = stat.S_IROTH | stat.S_IXOTH
    problems = []
    try:
        if not readable_by_all(folder, folders_wanted):
            return False
        for top, folders, files in os.walk(folder, onerror=problems.append):
            if top == os.fspath(folder) and VAR in folders:
                folders.remove(VAR)
            for name in folders:
                if not readable_by_all(os.path.join(top, name), folders_wanted):
                    return False
            for name in files:
                if not readable_by_all(os.path.join(top, name), stat.S_IROTH):
                    return False
    except OSError:
        return False
    return not problems


# Each file keeps its run bits and its times, as the copy step's copies do,
# but takes the tools folder's group and default ACL. What cp says could
# name a file in the home, so it is dropped.
def reflink_copy(sources, target):
    done = subprocess.run(
        [CP, "-R", "--reflink=always", "--preserve=timestamps", "--", *map(str, sources), str(target)],
        stdin=subprocess.DEVNULL,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        check=False,
    )
    return done.returncode == 0


def can_reflink(manifest, scratch):
    """Whether cp can reflink from a library into the tools folder. The probe
    goes inside this run's own folder, where no one else can put a link."""
    probe = scratch / "probe"
    works = reflink_copy([manifest], probe)
    unlink(probe)
    return works


def write_manifest(data, path):
    """The manifest exactly as read, whole, before its build has its name."""
    descriptor = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o644)
    with os.fdopen(descriptor, "wb") as file:
        file.write(data)
        file.flush()
        os.fsync(file.fileno())


def sync_folder(path):
    try:
        descriptor = os.open(path, os.O_RDONLY | os.O_DIRECTORY)
    except OSError:
        return
    try:
        os.fsync(descriptor)
    except OSError:
        pass
    finally:
        os.close(descriptor)


def make_build(tool, steamapps, scratch):
    """One build of a tool, whole, inside this run's own folder: the tool's
    folder but its var/, by reflink, then its manifest, unless Steam changed
    the manifest meanwhile. The build's folder, or the reason there is none."""
    appid, data, state, installdir = tool
    source = steamapps / "common" / installdir
    build = scratch / f"{appid}-{build_of(state)}"
    try:
        os.mkdir(build, 0o755)
        os.mkdir(build / "common", 0o755)
        os.mkdir(build / "common" / installdir, 0o755)
        entries = [source / name for name in sorted(os.listdir(source)) if name != VAR]
        if not reflink_copy(entries, build / "common" / installdir):
            return None, "copy-failed"
        if kidscan.read_plain_file(steamapps / f"appmanifest_{appid}.acf") != data:
            return None, "changed"
        write_manifest(data, build / f"appmanifest_{appid}.acf")
    except OSError:
        return None, "copy-failed"
    return build, None


def put_in_place(build, tools):
    """One rename gives the build its name. "had" when a build of that name
    came meanwhile, as rename never replaces a folder that holds anything."""
    place = tools / build.name
    try:
        os.rename(build, place)
    except OSError:
        return "had" if is_real_folder(place) else "copy-failed"
    # So that a power cut cannot lose the rename after the line was said.
    sync_folder(tools)
    return None


def set_aside(tools, path):
    """Takes one entry out of the readers' sight, then removes what it can of
    it. A folder is renamed over a new empty folder only this run made, so
    the rename can never land in a link. A build another Guardian made, which
    this account cannot empty, waits there until that Guardian's next run."""
    if not is_real_folder(path):
        unlink(path)
        return
    gone = tempfile.mkdtemp(prefix=GONE_PREFIX, dir=tools)
    try:
        os.rename(path, gone)
    except OSError:
        pass
    # The old build, or the empty folder that was to take its place.
    shutil.rmtree(gone, ignore_errors=True)


def remove_other_builds(tools, appid, kept):
    for entry in list(os.scandir(tools)):
        match = kidscan.TOOL_BUILD.fullmatch(entry.name)
        if match and match.group(1) == appid and int(match.group(2)) != kept:
            set_aside(tools, Path(entry.path))


def remove_leftovers(tools):
    """What an earlier run left under this program's own names, and anything
    under a build's name that is not a folder, which could otherwise stand
    in the way of a build for ever."""
    for entry in list(os.scandir(tools)):
        if entry.name.startswith(SCRATCH_PREFIX) or kidscan.TOOL_BUILD.fullmatch(entry.name):
            if not entry.is_dir(follow_symlinks=False):
                unlink(entry.path)
            elif entry.name.startswith(SCRATCH_PREFIX):
                shutil.rmtree(entry.path, ignore_errors=True)


class Library:
    """One library of the Guardian's Steam as this run reads it, with
    whether cp can reflink from it, found out at its first copy."""

    def __init__(self, steamapps):
        self.steamapps = steamapps
        self.reflinks = None


def share_one(tool, library, tools, scratch):
    """One tool: its line, as words."""
    appid, _, state, installdir = tool
    reason = reason_in_manifest(appid, state, library.steamapps)
    if reason is None:
        held = builds_held(tools, appid)
        # A build only goes forward: never one older than the folder's.
        if any(build >= build_of(state) for build in held):
            remove_other_builds(tools, appid, max(held))
            return ["tool-had", appid]
        if not all_readable(library.steamapps / "common" / installdir):
            reason = "unreadable-files"
        else:
            if library.reflinks is None:
                library.reflinks = can_reflink(library.steamapps / f"appmanifest_{appid}.acf", scratch)
            reason = "other-drive" if not library.reflinks else None
    if reason is None:
        build, reason = make_build(tool, library.steamapps, scratch)
        if build is not None:
            reason = put_in_place(build, tools)
        if reason is None or reason == "had":
            remove_other_builds(tools, appid, max(builds_held(tools, appid), default=build_of(state)))
            return ["tool-shared" if reason is None else "tool-had", appid]
    return ["tool-kept" if builds_held(tools, appid) else "tool-not-shared", appid, reason]


def share_tools(home, tools):
    # A grown-up who is not in cairn-guardian may not change the folder.
    if not is_real_folder(tools) or not os.access(tools, os.W_OK | os.X_OK):
        say("refused", "folder")
        return 1
    remove_leftovers(tools)
    found = [Library(steamapps) for steamapps in libraries(home)]
    if not found:
        say("no-steam")
    else:
        scratch = Path(tempfile.mkdtemp(prefix=SCRATCH_PREFIX, dir=tools))
        try:
            seen = set()
            for library in found:
                for tool in tools_in(library.steamapps):
                    # The first library that has a tool gives it.
                    if tool[0] not in seen:
                        seen.add(tool[0])
                        say(*share_one(tool, library, tools, scratch))
        finally:
            shutil.rmtree(scratch, ignore_errors=True)
    say("finished")
    return 0


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    # Only the tests move these; cairn-family passes nothing.
    parser.add_argument("--home", type=Path, default=None, help=argparse.SUPPRESS)
    parser.add_argument("--tools", type=Path, default=TOOLS, help=argparse.SUPPRESS)
    args = parser.parse_args(argv)

    # As root, a link in a home could send a read or a copy anywhere.
    if os.geteuid() == 0 or os.getuid() == 0:
        say("refused", "root")
        return 1
    os.umask(0o022)
    try:
        # Never $HOME, which whoever starts this could set.
        home = args.home or Path(pwd.getpwuid(os.getuid()).pw_dir)
        return share_tools(home, args.tools)
    except Exception:  # noqa: BLE001 - its words could name a file in the home
        say("failed")
        return 1


if __name__ == "__main__":
    sys.exit(main())
