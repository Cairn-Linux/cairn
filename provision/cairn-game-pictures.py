#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Copies the picture the Guardian's Steam keeps for each shared game into
the folder the launcher's game tiles read (ADR-0033, ADR-0035).

Steam keeps each game's library art in its own folder, as
appcache/librarycache/<appid>/header.jpg, or for some games one folder
further down, as <appid>/<hash>/header.jpg, where <hash> is 40 lowercase
hex digits. This copies that file, and only that file, for each game in
the shared library that gets a tile by kidscan's rules, into
/var/lib/cairn/game-pictures/<appid>.jpg. It downloads nothing.

Provisioning runs it as root, so that each child can read the pictures
and none can change them. It is installed as
/usr/local/libexec/cairn/cairn-game-pictures beside kidscan.py, whose app
id rule it uses, and run with `python3 -I -B`, which keeps the files in
the Guardian's home off Python's path and writes no cache beside it.

The Guardian's home is opened as given, following a link, because on
Bazzite /home is a link to /var/home. Nothing inside the home is followed:
.local, share, Steam, appcache, librarycache, <appid>, <hash> and
header.jpg are each opened from the folder before without following a
link. A link at one of the five folders down to librarycache means no
picture for any game; a link at <appid> or at header.jpg is refused; a
link at a <hash> folder is passed over. A header.jpg is read only when it
is a plain file of a sensible size that starts as a JPEG does. A
steamapps in the shared folder that is a link stops the run.

It prints only these lines, made of fixed words and appids, so nothing
read from a home reaches the Guardian's terminal. Provisioning turns them
into sentences of its own.

    copied APPID            the picture is new or changed, and now in place
    removed APPID           its game has no tile any more, so it went
    missing APPID           the Guardian's Steam has no picture for it yet
    refused APPID REASON    the picture there was left out, because it
                            is not-a-file, too-big, not-a-jpeg or
                            unreadable; one already in place stays
    refused folder          the pictures folder is a link or not a
                            folder, so nothing was touched
    finished                the last line of a run that went to the end
    failed                  something unexpected stopped the run

It exits 0 when it finished, and 1 otherwise. A game without a picture is
not a failure: its tile shows its name.
"""

import argparse
import errno
import os
import re
import stat
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import kidscan  # noqa: E402

SHARED = Path("/var/lib/cairn/steam-library")
PICTURES = Path("/var/lib/cairn/game-pictures")
# Where the Guardian's Steam keeps its library art, below their home.
CACHE = (".local", "share", "Steam", "appcache", "librarycache")
HEADER = "header.jpg"
# The folder some games' art is kept in, one below <appid>.
HASHED_FOLDER = re.compile(r"[0-9a-f]{40}")
FOLDER_NOT_LINK = os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW
# Steam's header art is a 460x215 JPEG of a few tens of kilobytes.
PICTURE_LIMIT = 1 << 20
JPEG_START = b"\xff\xd8\xff"

# Names only this program writes in the pictures folder.
TEMPORARY_PREFIX = ".cairn-"
PICTURE_NAME = re.compile(r"([0-9]{1,10})\.jpg")


def say(*words):
    print(*words, flush=True)


def shared_games(shared):
    """The appids of the shared games that get a tile, by kidscan's own
    rules, so that a picture is copied, kept or asked for only for a game
    the children can see. kidscan's warnings go to stderr, which
    provisioning does not show. A steamapps that is a link stops the run:
    every Guardian can write the shared folder, and root reading through a
    link would list another account's games. Guardians are administrators,
    so this catches a mistake; it is not a wall."""
    steamapps = shared / "steamapps"
    if steamapps.is_symlink() or not steamapps.is_dir():
        raise NotADirectoryError(steamapps)
    return sorted((app["appid"] for app in kidscan.installed_apps(steamapps)), key=int)


def open_cache(home):
    """The librarycache folder in the Guardian's home, or None when there is
    none to read. The home is opened as given, following a link; each of
    the five folders below it is opened from the one before without
    following one, so a link or a missing folder there means None. Only an
    absolute home counts: provisioning passes an empty one when it finds
    no Guardian's home, and that must not mean the current folder."""
    if not os.path.isabs(home):
        return None
    try:
        folder = os.open(home, os.O_RDONLY | os.O_DIRECTORY)
    except OSError:
        return None
    for name in CACHE:
        try:
            inner = os.open(name, FOLDER_NOT_LINK, dir_fd=folder)
        except OSError:
            return None
        finally:
            os.close(folder)
        folder = inner
    return folder


def read_header(cache_fd, appid):
    """One game's picture from the Guardian's Steam: (bytes, None), or
    (None, None) when there is none, or (None, reason) when the one there
    is left out. A link, a FIFO or a huge file may have been put there to
    make this program read something else, or never finish."""
    if cache_fd is None:
        return None, None
    try:
        folder = os.open(appid, FOLDER_NOT_LINK, dir_fd=cache_fd)
    except FileNotFoundError:
        return None, None
    except NotADirectoryError:
        # A link and a plain file both land here; only the link is refused.
        try:
            is_link = stat.S_ISLNK(os.stat(appid, dir_fd=cache_fd, follow_symlinks=False).st_mode)
        except OSError:
            is_link = False
        return None, ("not-a-file" if is_link else None)
    except OSError as error:
        return None, ("not-a-file" if error.errno == errno.ELOOP else "unreadable")
    try:
        data, refused = read_picture(folder)
        # Only a picture that is not there is looked for further down; one
        # that is there and refused stays refused.
        if data is None and refused is None:
            data, refused = read_hashed_picture(folder)
        return data, refused
    finally:
        os.close(folder)


def read_hashed_picture(folder):
    """The picture in <appid>/<hash>/header.jpg, read as read_picture reads
    one, or (None, None) when no <hash> folder has one."""
    name = newest_hashed_folder(folder)
    if name is None:
        return None, None
    try:
        inner = os.open(name, FOLDER_NOT_LINK, dir_fd=folder)
    except OSError:
        # It changed since it was chosen.
        return None, None
    try:
        return read_picture(inner)
    finally:
        os.close(inner)


def newest_hashed_folder(folder):
    """The <hash> folder holding a header.jpg, or None. A link or a file
    with a <hash> name is passed over, never followed. Of two or more, the
    one whose header.jpg is newest wins, then the first by name, so every
    run picks the same one."""
    try:
        names = os.listdir(folder)
    except OSError:
        return None
    found = []
    for name in names:
        if not HASHED_FOLDER.fullmatch(name):
            continue
        try:
            inner = os.open(name, FOLDER_NOT_LINK, dir_fd=folder)
        except OSError:
            continue
        try:
            found.append((os.stat(HEADER, dir_fd=inner, follow_symlinks=False).st_mtime_ns, name))
        except OSError:
            pass
        finally:
            os.close(inner)
    if not found:
        return None
    newest = max(changed for changed, _ in found)
    return min(name for changed, name in found if changed == newest)


def read_picture(folder):
    try:
        descriptor = os.open(HEADER, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK, dir_fd=folder)
    except FileNotFoundError:
        return None, None
    except OSError as error:
        return None, ("not-a-file" if error.errno == errno.ELOOP else "unreadable")
    try:
        details = os.fstat(descriptor)
        if not stat.S_ISREG(details.st_mode):
            return None, "not-a-file"
        if details.st_size > PICTURE_LIMIT:
            return None, "too-big"
        data = read_at_most(descriptor, PICTURE_LIMIT + 1)
    except OSError:
        return None, "unreadable"
    finally:
        os.close(descriptor)
    if len(data) > PICTURE_LIMIT:
        return None, "too-big"
    if not data.startswith(JPEG_START):
        return None, "not-a-jpeg"
    return data, None


def read_at_most(descriptor, limit):
    data = b""
    while len(data) < limit:
        chunk = os.read(descriptor, limit - len(data))
        if not chunk:
            break
        data += chunk
    return data


def picture_in_place(pictures_fd, appid):
    """The bytes of the picture already in place, or None when there is
    none to read."""
    try:
        descriptor = os.open(f"{appid}.jpg", os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK, dir_fd=pictures_fd)
    except OSError:
        return None
    try:
        if not stat.S_ISREG(os.fstat(descriptor).st_mode):
            return None
        return read_at_most(descriptor, PICTURE_LIMIT + 1)
    finally:
        os.close(descriptor)


def has_picture(pictures_fd, appid):
    try:
        os.stat(f"{appid}.jpg", dir_fd=pictures_fd, follow_symlinks=False)
    except FileNotFoundError:
        return False
    return True


def put_in_place(pictures_fd, appid, data):
    """A new file under a temporary name, on the disk, then renamed over
    the old one: a launcher starting meanwhile draws the old picture or
    the new one, never half of one."""
    temporary = f"{TEMPORARY_PREFIX}{appid}.jpg"
    descriptor = os.open(temporary, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o644, dir_fd=pictures_fd)
    with os.fdopen(descriptor, "wb") as file:
        file.write(data)
        file.flush()
        os.fsync(file.fileno())
    os.rename(temporary, f"{appid}.jpg", src_dir_fd=pictures_fd, dst_dir_fd=pictures_fd)
    os.fsync(pictures_fd)


def copy_picture(cache_fd, pictures_fd, appid):
    data, refused = read_header(cache_fd, appid)
    if refused is not None:
        say("refused", appid, refused)
        return
    if data is None:
        # A picture from an earlier run stays until the game goes.
        if not has_picture(pictures_fd, appid):
            say("missing", appid)
        return
    if picture_in_place(pictures_fd, appid) == data:
        return
    put_in_place(pictures_fd, appid, data)
    say("copied", appid)


def remove_leftovers(pictures_fd):
    for name in os.listdir(pictures_fd):
        if name.startswith(TEMPORARY_PREFIX):
            os.unlink(name, dir_fd=pictures_fd)


def remove_gone(pictures_fd, appids):
    """The pictures of games that no longer get a tile: gone from the
    shared folder, or left out by kidscan's rules."""
    for name in sorted(os.listdir(pictures_fd)):
        match = PICTURE_NAME.fullmatch(name)
        if match and match.group(1) not in appids:
            os.unlink(name, dir_fd=pictures_fd)
            say("removed", match.group(1))


def open_pictures(pictures):
    """The pictures folder, or None when it is a link or not a folder. Root
    writes in it, so it must be the folder provisioning made."""
    try:
        if not stat.S_ISDIR(os.lstat(pictures).st_mode):
            return None
    except FileNotFoundError:
        return None
    return os.open(pictures, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)


def make_pictures(home, shared, pictures):
    pictures_fd = open_pictures(pictures)
    if pictures_fd is None:
        say("refused", "folder")
        return 1
    cache_fd = None
    try:
        remove_leftovers(pictures_fd)
        appids = shared_games(shared)
        cache_fd = open_cache(home)
        for appid in appids:
            copy_picture(cache_fd, pictures_fd, appid)
        remove_gone(pictures_fd, set(appids))
    finally:
        os.close(pictures_fd)
        if cache_fd is not None:
            os.close(cache_fd)
    say("finished")
    return 0


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--home", required=True,
                        help="the Guardian's home, which holds .local/share/Steam/appcache/librarycache")
    # Provisioning passes its own; the tests move them.
    parser.add_argument("--shared", type=Path, default=SHARED, help=argparse.SUPPRESS)
    parser.add_argument("--pictures", type=Path, default=PICTURES, help=argparse.SUPPRESS)
    args = parser.parse_args(argv)

    os.umask(0o022)
    try:
        return make_pictures(args.home, args.shared, args.pictures)
    except Exception:
        # What went wrong could name a file in a home, so it is not said.
        say("failed")
        return 1


if __name__ == "__main__":
    sys.exit(main())
