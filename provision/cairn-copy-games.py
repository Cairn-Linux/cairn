#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Copies each game in the shared Steam library into this account's own
Steam library, by reflink (ADR-0033).

Provisioning runs it as each child, never as root, through the visit
helper's copy mode (`cairn-steam-visit --copy-games CHILD`), with the child
logged out and listed so that they cannot log in meanwhile. It is installed
as /usr/local/libexec/cairn/cairn-copy-games beside kidscan.py and run with
`python3 -I`, which keeps the child's files off Python's path, so it finds
kidscan's VDF parser by its own folder.

It prints only these lines, one per game or condition, made of fixed words
and appids, so nothing from the child's home reaches the Guardian's
terminal. Provisioning turns them into sentences of its own.

    copied APPID           the game is now in the child's library
    have APPID             the child has it already, and nothing changed
    skipped APPID REASON   left out, for one of the reasons below
    unnumbered             a manifest whose file name has no appid
    no-library             the child's Steam has not run yet
    no-shared              there is no shared library
    lists-shared           the child's Steam lists the shared library
    no-reflink             the disk cannot reflink, so nothing is copied
    refused root           it was run as root, and did nothing

It exits 0 when it did what it could; a game left out is not a failure.
"""

import argparse
import os
import pwd
import re
import shutil
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

APPID = re.compile(r"[1-9][0-9]{0,9}")
# A real manifest is a few kilobytes.
MANIFEST_LIMIT = 1 << 20


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


def plain_folder_name(name):
    return (
        isinstance(name, str)
        and name not in ("", ".", "..")
        and "/" not in name
        and "\0" not in name
        and not name.startswith(".cairn-")
    )


# The child's Steam account may not have the branch, and its Steam would
# then download the game again.
def on_beta_branch(state):
    for block in ("UserConfig", "MountedConfig"):
        config = kidscan.ci_get(state, block, {})
        branch = kidscan.ci_get(config, "BetaKey", "") if isinstance(config, dict) else None
        if branch not in ("", "public"):
            return True
    return False


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
    if not plain_folder_name(installdir):
        return ("skipped", "installdir")
    if on_beta_branch(state):
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


def lists_shared(steam, shared_root):
    """Whether the child's own Steam lists the shared library. It would then
    see each game twice, and play the shared one."""
    wanted = os.path.normpath(shared_root)
    for name in ("config/libraryfolders.vdf", "steamapps/libraryfolders.vdf"):
        try:
            text = (steam / name).read_text(encoding="utf-8", errors="replace")
        except FileNotFoundError:
            continue
        try:
            folders = kidscan.ci_get(kidscan.parse_vdf(text), "libraryfolders", {})
        except ValueError:
            if wanted in text:
                return True
            continue
        if not isinstance(folders, dict):
            continue
        for entry in folders.values():
            path = kidscan.ci_get(entry, "path") if isinstance(entry, dict) else entry
            if isinstance(path, str) and path and os.path.normpath(path) == wanted:
                return True
    return False


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


def put_manifest(data, library, appid):
    """Writes the manifest exactly as read, whole, under its own name."""
    temporary = library / f"{MANIFEST_PREFIX}{appid}"
    descriptor = os.open(temporary, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o644)
    with os.fdopen(descriptor, "wb") as file:
        file.write(data)
        file.flush()
        os.fsync(file.fileno())
    os.rename(temporary, library / f"appmanifest_{appid}.acf")


def copy_game(appid, data, manifest, installdir, shared, library):
    """The folder under a temporary name, then into place, then the
    manifest: Steam never sees a manifest for half a game. None when it is
    copied, or the reason it is not."""
    temporary = library / "common" / f"{COPY_PREFIX}{appid}"
    target = library / "common" / installdir
    try:
        if not reflink_copy(shared / "common" / installdir, temporary):
            remove(temporary)
            return "copy-failed"
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
        os.rename(temporary, target)
        try:
            put_manifest(data, library, appid)
        except OSError:
            os.rename(target, temporary)
            raise
    except OSError:
        remove(temporary)
        return "copy-failed"
    return None


def copy_games(shared, library):
    (library / "common").mkdir(mode=0o755, exist_ok=True)
    remove_leftovers(library)
    reflink = None
    for manifest in sorted(shared.glob("appmanifest_*.acf")):
        appid = manifest.name.removeprefix("appmanifest_").removesuffix(".acf")
        if not APPID.fullmatch(appid):
            say("unnumbered")
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
    args = parser.parse_args(argv)

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
    if lists_shared(library.parent, args.shared.parent):
        say("lists-shared")
        return 0
    copy_games(args.shared, library)
    return 0


if __name__ == "__main__":
    sys.exit(main())
