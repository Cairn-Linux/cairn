#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Copies the picture the Guardian's Steam keeps for each shared game into
the folder the launcher's game tiles read (ADR-0033, ADR-0035).

Steam keeps each game's library art in its own folder, as
appcache/librarycache/<appid>/header.jpg, or for some games one folder
further down, as <appid>/<hash>/header.jpg, where <hash> is 40 lowercase
hex digits. Some games have the same picture named library_header.jpg
instead, in either place. In each folder header.jpg is looked for first,
and <appid> is looked in before any <hash> folder below it. This copies
the one picture it finds, and nothing else, for each game in the shared
library that gets a tile by kidscan's rules, into
/var/lib/cairn/game-pictures/<appid>.jpg. It downloads nothing. The
launcher and the Guardian's Games page show a game's picture from there by
its app id (ADR-0047).

Provisioning and cairn-family save-games run it as root, so that each
child can read the pictures and none can change them. It is installed as
/usr/local/libexec/cairn/cairn-game-pictures beside kidscan.py, whose app
id rule it uses, and run with `python3 -I -B`, which keeps the files in
the Guardian's home off Python's path and writes no cache beside it.

The Guardian's home is opened as given, following a link, because on
Bazzite /home is a link to /var/home. Nothing inside the home is followed:
.local, share, Steam, appcache, librarycache, <appid>, <hash> and the
picture are each opened from the folder before without following a link.
A link at one of the five folders down to librarycache means no picture
for any game; a link at <appid> or at the picture is refused; a link at a
<hash> folder is passed over. A picture is read only when it is a plain
file of a sensible size that starts as a JPEG does, and one that is there
and left out is never passed over for another. A steamapps in the shared
folder that is a link stops the run.

A picture also stays while its game is on any child's own list,
/var/lib/cairn/games/<login>.json, which cairn-family save-games writes
(ADR-0041), even when kidscan leaves the game out of the shared folder's
tiles: uninstalled, moved back to the Guardian's own library, on a beta,
or, as the tests assume, being updated. The child keeps that tile, so it
keeps its picture (ADR-0047). The lists only keep a picture; a picture is
copied only for a game in the shared folder, so the Guardian's Steam is
read only for those. Each list is read as the launcher reads it, but
without following a link or waiting on a FIFO: only <login>.json names, by
cairn-family's rule for a login, each a plain file of at most 4 MiB
holding the launcher's games file, version 1, of which only the entries
the launcher gives a tile count. A list that cannot be read that way keeps
nothing; only root can put such a file in that folder, so it is a mistake,
and the games on it then show their names when they leave the shared
folder. No lists folder means no lists, as on a computer set up by hand. A
lists folder that is a link, not a folder, not this program's own
account's, or open to others for writing stops the run before anything is
touched, as cairn-family refuses to write there.

It prints only these lines, made of fixed words and appids, so nothing
read from a home reaches the Guardian's terminal. Provisioning turns them
into sentences of its own, and cairn-family passes them to the journal.

    copied APPID            the picture is new or changed, and now in place
    removed APPID           its game is neither among the shared folder's
                            tiles nor on any child's list, so it went
    missing APPID           the Guardian's Steam has no picture for a
                            shared game yet
    refused APPID REASON    the picture there was left out, because it
                            is not-a-file, too-big, not-a-jpeg or
                            unreadable; one already in place stays
    refused folder          the pictures folder is a link or not a
                            folder, so nothing was touched
    refused lists           the children's lists folder is a link, not
                            a folder, not this program's own account's,
                            or writable by its group or others, so
                            nothing was touched
    finished                the last line of a run that went to the end
    failed                  something unexpected stopped the run

It exits 0 when it finished, and 1 otherwise. A game without a picture is
not a failure: its tile shows its name.
"""

import argparse
import errno
import json
import os
import re
import stat
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import kidscan  # noqa: E402

SHARED = Path("/var/lib/cairn/steam-library")
PICTURES = Path("/var/lib/cairn/game-pictures")
# Each child's own list of games (ADR-0041), and cairn-family's rule for
# the login in its name. A list of a thousand games is a few hundred
# kilobytes; cairn-family and the Games page read at most 4 MiB of one.
LISTS = Path("/var/lib/cairn/games")
LIST_NAME = re.compile(r"[a-z_][a-z0-9_-]{0,31}\.json")
LIST_LIMIT = 4 << 20
# Where the Guardian's Steam keeps its library art, below their home.
CACHE = (".local", "share", "Steam", "appcache", "librarycache")
# The names Steam gives a game's picture, in the order they are looked for.
# Most games have header.jpg; some have the same 460x215 picture only as
# library_header.jpg.
HEADERS = ("header.jpg", "library_header.jpg")
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
    """The picture in a <hash> folder below <appid>, read as read_picture
    reads one, or (None, None) when no <hash> folder has one."""
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
    """The <hash> folder holding a picture, or None. A link or a file with
    a <hash> name is passed over, never followed. Of two or more, the one
    whose picture is newest wins, then the first by name, so every run
    picks the same one."""
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
            changed = picture_changed(inner)
        finally:
            os.close(inner)
        if changed is not None:
            found.append((changed, name))
    if not found:
        return None
    newest = max(changed for changed, _ in found)
    return min(name for changed, name in found if changed == newest)


def picture_changed(folder):
    """When the picture read_picture would read in a folder last changed,
    or None when there is none."""
    for name in HEADERS:
        try:
            return os.stat(name, dir_fd=folder, follow_symlinks=False).st_mtime_ns
        except FileNotFoundError:
            continue
        except OSError:
            return None
    return None


def read_picture(folder):
    """The picture in one folder, by the first of HEADERS that is there:
    (bytes, None), or (None, None) when none is, or (None, reason) when
    the one there is left out. One that is left out is not passed over for
    the next name."""
    for name in HEADERS:
        data, refused = read_named_picture(folder, name)
        if data is not None or refused is not None:
            return data, refused
    return None, None


def read_named_picture(folder, name):
    """The picture called `name` in a folder, as read_picture answers."""
    try:
        descriptor = os.open(name, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK, dir_fd=folder)
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
    """The pictures of games that are neither shared by kidscan's rules nor
    on any child's list."""
    for name in sorted(os.listdir(pictures_fd)):
        match = PICTURE_NAME.fullmatch(name)
        if match and match.group(1) not in appids:
            os.unlink(name, dir_fd=pictures_fd)
            say("removed", match.group(1))


def listed_games(lists):
    """The appids of the games on every child's own list, or None when the
    lists folder is a link, not a folder, or a folder someone other than
    this program's own account could write in: root in use, the test's own
    account in a test, as cairn-family requires before it writes a list.
    No folder means no lists."""
    try:
        folder = os.open(lists, FOLDER_NOT_LINK)
    except FileNotFoundError:
        return set()
    except OSError as error:
        if error.errno in (errno.ELOOP, errno.ENOTDIR):
            return None
        raise
    try:
        details = os.fstat(folder)
        if details.st_uid != os.geteuid() or details.st_mode & (stat.S_IWGRP | stat.S_IWOTH):
            return None
        listed = set()
        for name in sorted(os.listdir(folder)):
            if LIST_NAME.fullmatch(name):
                listed |= appids_on_list(folder, name)
        return listed
    finally:
        os.close(folder)


def appids_on_list(folder, name):
    """The appids of the games one list gives a tile, or none when it is
    not a plain file of at most LIST_LIMIT holding the launcher's games
    file, version 1. It is opened without following a link or waiting for
    a FIFO's writer."""
    try:
        descriptor = os.open(name, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK, dir_fd=folder)
    except OSError:
        return set()
    try:
        details = os.fstat(descriptor)
        if not stat.S_ISREG(details.st_mode) or details.st_size > LIST_LIMIT:
            return set()
        data = read_at_most(descriptor, LIST_LIMIT + 1)
    except OSError:
        return set()
    finally:
        os.close(descriptor)
    if len(data) > LIST_LIMIT:
        return set()
    try:
        document = json.loads(data)
    except (ValueError, RecursionError):
        return set()
    if not isinstance(document, dict):
        return set()
    version = document.get("version")
    entries = document.get("entries")
    if isinstance(version, bool) or version != 1 or not isinstance(entries, list):
        return set()
    return {appid for appid in map(tile_appid, entries) if appid is not None}


def tile_appid(entry):
    """The appid of an entry the launcher gives a game tile: a title, the
    category games, and exactly `steam -applaunch APPID`. None for any
    other entry (launcher/src/GamesFile.cpp)."""
    if not isinstance(entry, dict):
        return None
    title = entry.get("title")
    words = entry.get("exec")
    if not isinstance(title, str) or not title or entry.get("category") != "games":
        return None
    if not isinstance(words, list) or len(words) != 3 or words[:2] != ["steam", "-applaunch"]:
        return None
    appid = words[2]
    if not isinstance(appid, str) or not kidscan.APPID.fullmatch(appid):
        return None
    return appid


def open_pictures(pictures):
    """The pictures folder, or None when it is a link or not a folder. Root
    writes in it, so it must be the folder provisioning made."""
    try:
        if not stat.S_ISDIR(os.lstat(pictures).st_mode):
            return None
    except FileNotFoundError:
        return None
    return os.open(pictures, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)


def make_pictures(home, shared, pictures, lists):
    pictures_fd = open_pictures(pictures)
    if pictures_fd is None:
        say("refused", "folder")
        return 1
    cache_fd = None
    try:
        listed = listed_games(lists)
        if listed is None:
            say("refused", "lists")
            return 1
        remove_leftovers(pictures_fd)
        appids = shared_games(shared)
        cache_fd = open_cache(home)
        # Only a shared game's picture is copied; a listed one only stays.
        for appid in appids:
            copy_picture(cache_fd, pictures_fd, appid)
        remove_gone(pictures_fd, set(appids) | listed)
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
    parser.add_argument("--lists", type=Path, default=LISTS, help=argparse.SUPPRESS)
    args = parser.parse_args(argv)

    os.umask(0o022)
    try:
        return make_pictures(args.home, args.shared, args.pictures, args.lists)
    except Exception:
        # What went wrong could name a file in a home, so it is not said.
        say("failed")
        return 1


if __name__ == "__main__":
    sys.exit(main())
