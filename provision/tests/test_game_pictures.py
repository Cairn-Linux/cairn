#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Runs cairn-game-pictures.py, the copy of each shared game's picture from
the Guardian's Steam (ADR-0033, ADR-0035), on folders made for each test.

Standard library only. The program is installed into a temporary folder
beside kidscan.py, as provisioning installs it, and run there with
`python3 -I`. It always gets --home, --shared and --pictures, so it never
reads this account's own Steam. The pictures are a few bytes that start as
a JPEG does, made here; no game's art is ever part of the tests.
"""

import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

HERE = pathlib.Path(__file__).resolve().parent
PROGRAM = HERE.parent / "cairn-game-pictures.py"
KIDSCAN = HERE.parents[1] / "tools" / "kidscan" / "kidscan.py"

# Every line the program may print (its docstring).
GRAMMAR = re.compile(
    r"(copied|removed|missing) [0-9]{1,10}"
    r"|refused [0-9]{1,10} (not-a-file|too-big|not-a-jpeg|unreadable)"
    r"|refused folder|finished|failed"
)

LIMIT = 1 << 20
# Steam names the folder some games' art is kept in by a hash like these.
HASH = "162664aa5da85f418105350c5d67ca565f6c3713"
OTHER_HASH = "0a1b2c3d4e5f60718293a4b5c6d7e8f901234567"
# The name some games' picture has instead of header.jpg.
LIBRARY_HEADER = "library_header.jpg"


def stand_in(text="a picture"):
    """Bytes that start as a JPEG does, and are nothing else."""
    return b"\xff\xd8\xff\xe0" + text.encode()


def needs_not_root(test):
    return unittest.skipIf(os.geteuid() == 0, "root can read every file")(test)


def refusals(root):
    """Each way a picture found in Steam's cache is left out: the reason the
    program gives, and how to make one at a path. The link points at a
    picture made in `root`."""
    elsewhere = root / "elsewhere.jpg"
    elsewhere.write_bytes(stand_in("someone else's"))
    return {
        "link": ("not-a-file", lambda path: path.symlink_to(elsewhere)),
        "fifo": ("not-a-file", os.mkfifo),
        "folder": ("not-a-file", lambda path: path.mkdir()),
        "too big": ("too-big", lambda path: path.write_bytes(stand_in().ljust(LIMIT + 1, b"x"))),
        "not a jpeg": ("not-a-jpeg", lambda path: path.write_bytes(b"\x89PNG\r\n\x1a\n")),
    }


def manifest_text(appid, state_flags="4"):
    return (f'"AppState"\n{{\n\t"appid"\t\t"{appid}"\n\t"name"\t\t"Game {appid}"\n'
            f'\t"StateFlags"\t\t"{state_flags}"\n\t"installdir"\t\t"Game {appid}"\n}}\n')


class Machine:
    """A shared library, a Guardian's Steam with its picture cache, the
    pictures folder provisioning makes, and the program installed beside
    kidscan.py, all under one folder."""

    def __init__(self, root):
        self.root = pathlib.Path(root)
        self.libexec = self.root / "libexec"
        self.libexec.mkdir()
        shutil.copy(PROGRAM, self.libexec / "cairn-game-pictures")
        shutil.copy(KIDSCAN, self.libexec / "kidscan.py")
        self.shared = self.root / "var" / "steam-library"
        (self.shared / "steamapps").mkdir(parents=True)
        self.pictures = self.root / "var" / "game-pictures"
        self.pictures.mkdir()
        self.home = self.root / "home" / "gina"
        self.cache = self.home / ".local" / "share" / "Steam" / "appcache" / "librarycache"
        self.cache.mkdir(parents=True)

    def add_game(self, appid, state_flags="4"):
        """A game in the shared library, installed as kidscan expects one
        that gets a tile, unless state_flags says otherwise."""
        (self.shared / "steamapps" / f"appmanifest_{appid}.acf").write_text(
            manifest_text(appid, state_flags))
        (self.shared / "steamapps" / "common" / f"Game {appid}").mkdir(parents=True, exist_ok=True)

    def remove_game(self, appid):
        (self.shared / "steamapps" / f"appmanifest_{appid}.acf").unlink()

    def header(self, appid, hashed=None, name="header.jpg"):
        """Where Steam keeps the picture: <appid>/header.jpg, or with
        `hashed`, <appid>/<hashed>/header.jpg, or by another `name`."""
        folder = self.cache / str(appid)
        return (folder / hashed if hashed else folder) / name

    def add_header(self, appid, data=None, hashed=None, name="header.jpg"):
        path = self.header(appid, hashed, name)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(stand_in() if data is None else data)
        return path

    def picture(self, appid):
        return self.pictures / f"{appid}.jpg"

    def run(self, home=None, cwd=None):
        # A run that blocks fails the test rather than hanging it.
        done = subprocess.run(
            [sys.executable, "-I", str(self.libexec / "cairn-game-pictures"),
             "--home", str(self.home) if home is None else home,
             "--shared", str(self.shared), "--pictures", str(self.pictures)],
            cwd=cwd,
            stdin=subprocess.DEVNULL,
            capture_output=True,
            text=True,
            check=False,
            timeout=60,
        )
        self.code = done.returncode
        self.stderr = done.stderr
        return done.stdout.splitlines()


class PicturesTestCase(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.machine = Machine(self.directory.name)

    def tearDown(self):
        self.directory.cleanup()

    def assertOnlyGrammar(self, lines):
        for line in lines:
            self.assertRegex(line, f"^(?:{GRAMMAR.pattern})$")
            self.assertNotIn("/", line)

    def run_finished(self, **how):
        lines = self.machine.run(**how)
        self.assertEqual(self.machine.code, 0, self.machine.stderr)
        self.assertOnlyGrammar(lines)
        self.assertEqual(lines[-1:], ["finished"])
        return lines[:-1]

    def pictures(self):
        return sorted(path.name for path in self.machine.pictures.iterdir())

    def set_time(self, path, seconds):
        os.utime(path, ns=(seconds * 10**9, seconds * 10**9))


class CopyTest(PicturesTestCase):
    def test_a_picture_is_copied_whole_and_readable_by_everyone(self):
        self.machine.add_game(294690)
        self.machine.add_header(294690, stand_in("Pajama Sam"))
        self.assertEqual(self.run_finished(), ["copied 294690"])
        picture = self.machine.picture(294690)
        self.assertEqual(picture.read_bytes(), stand_in("Pajama Sam"))
        self.assertEqual(oct(picture.stat().st_mode & 0o777), oct(0o644))
        self.assertEqual(self.pictures(), ["294690.jpg"])

    def test_an_unchanged_picture_is_left_as_it_is(self):
        self.machine.add_game(100)
        self.machine.add_header(100)
        self.assertEqual(self.run_finished(), ["copied 100"])
        before = os.stat(self.machine.picture(100))
        self.assertEqual(self.run_finished(), [])
        after = os.stat(self.machine.picture(100))
        self.assertEqual((before.st_ino, before.st_mtime_ns), (after.st_ino, after.st_mtime_ns))

    def test_a_changed_picture_is_copied_again(self):
        self.machine.add_game(100)
        self.machine.add_header(100, stand_in("old"))
        self.run_finished()
        self.machine.add_header(100, stand_in("new"))
        self.assertEqual(self.run_finished(), ["copied 100"])
        self.assertEqual(self.machine.picture(100).read_bytes(), stand_in("new"))

    def test_games_go_in_number_order(self):
        for appid in (100, 20, 3):
            self.machine.add_game(appid)
            self.machine.add_header(appid)
        self.assertEqual(self.run_finished(), ["copied 3", "copied 20", "copied 100"])

    def test_a_picture_of_exactly_the_limit_is_copied(self):
        self.machine.add_game(100)
        self.machine.add_header(100, stand_in().ljust(LIMIT, b"x"))
        self.assertEqual(self.run_finished(), ["copied 100"])


class MissingTest(PicturesTestCase):
    def test_no_picture_yet(self):
        self.machine.add_game(100)
        self.assertEqual(self.run_finished(), ["missing 100"])
        self.assertEqual(self.pictures(), [])

    def test_a_game_folder_without_its_picture(self):
        self.machine.add_game(100)
        (self.machine.cache / "100").mkdir()
        self.assertEqual(self.run_finished(), ["missing 100"])

    def test_a_plain_file_where_the_game_folder_belongs(self):
        self.machine.add_game(100)
        (self.machine.cache / "100").write_bytes(stand_in())
        self.assertEqual(self.run_finished(), ["missing 100"])

    def test_no_cache_at_all(self):
        self.machine.add_game(100)
        shutil.rmtree(self.machine.cache)
        self.assertEqual(self.run_finished(), ["missing 100"])

    def test_a_home_that_is_not_an_absolute_path_is_none(self):
        # Provisioning passes "" when the Guardian has no home; that is not
        # the folder this happens to run in.
        self.machine.add_game(100)
        self.machine.add_header(100)
        for home in ("", "gina", "."):
            with self.subTest(home=home):
                self.assertEqual(self.run_finished(home=home, cwd=self.machine.home), ["missing 100"])
                self.assertEqual(self.run_finished(home=home, cwd=self.machine.home.parent), ["missing 100"])

    def test_a_link_on_the_way_to_the_cache_is_not_followed(self):
        # Each folder from the home down to librarycache, moved aside and a
        # link to it put in its place.
        self.machine.add_game(100)
        self.machine.add_header(100)
        folder = self.machine.cache
        while folder != self.machine.home:
            with self.subTest(folder=folder.name):
                moved = self.machine.root / "moved"
                folder.rename(moved)
                folder.symlink_to(moved)
                try:
                    self.assertEqual(self.run_finished(), ["missing 100"])
                    self.assertEqual(self.pictures(), [])
                finally:
                    folder.unlink()
                    moved.rename(folder)
            folder = folder.parent

    def test_a_file_on_the_way_to_the_cache_is_no_cache(self):
        self.machine.add_game(100)
        shutil.rmtree(self.machine.home / ".local" / "share")
        (self.machine.home / ".local" / "share").write_bytes(b"")
        self.assertEqual(self.run_finished(), ["missing 100"])

    def test_a_home_that_is_a_link_is_followed(self):
        # On Bazzite /home is a link to /var/home.
        self.machine.add_game(100)
        self.machine.add_header(100)
        link = self.machine.root / "home-link"
        link.symlink_to(self.machine.home)
        self.assertEqual(self.run_finished(home=str(link)), ["copied 100"])

    def test_a_picture_is_kept_when_its_source_goes(self):
        self.machine.add_game(100)
        self.machine.add_header(100, stand_in("kept"))
        self.run_finished()
        shutil.rmtree(self.machine.cache / "100")
        self.assertEqual(self.run_finished(), [])
        self.assertEqual(self.machine.picture(100).read_bytes(), stand_in("kept"))


class HashedFolderTest(PicturesTestCase):
    """Steam keeps some games' picture one folder further down, in a folder
    named by a hash: <appid>/<hash>/header.jpg."""

    def test_a_picture_in_a_hashed_folder_is_copied(self):
        self.machine.add_game(730)
        self.machine.add_header(730, stand_in("hashed"), hashed=HASH)
        self.assertEqual(self.run_finished(), ["copied 730"])
        self.assertEqual(self.machine.picture(730).read_bytes(), stand_in("hashed"))

    def test_the_picture_beside_the_hashed_folder_comes_first(self):
        self.machine.add_game(730)
        self.machine.add_header(730, stand_in("hashed"), hashed=HASH)
        self.machine.add_header(730, stand_in("plain"))
        self.assertEqual(self.run_finished(), ["copied 730"])
        self.assertEqual(self.machine.picture(730).read_bytes(), stand_in("plain"))

    def test_a_refused_picture_beside_it_is_not_passed_over(self):
        self.machine.add_game(730)
        self.machine.add_header(730, stand_in("hashed"), hashed=HASH)
        self.machine.add_header(730, b"not a jpeg")
        self.assertEqual(self.run_finished(), ["refused 730 not-a-jpeg"])
        self.assertEqual(self.pictures(), [])

    def test_of_two_the_newest_picture_wins(self):
        self.machine.add_game(730)
        for hashed, seconds in ((HASH, 2000), (OTHER_HASH, 1000)):
            path = self.machine.add_header(730, stand_in(hashed), hashed=hashed)
            self.set_time(path, seconds)
        self.assertEqual(self.run_finished(), ["copied 730"])
        self.assertEqual(self.machine.picture(730).read_bytes(), stand_in(HASH))

    def test_of_two_as_new_the_first_by_name_wins(self):
        self.machine.add_game(730)
        for hashed in (HASH, OTHER_HASH):
            self.set_time(self.machine.add_header(730, stand_in(hashed), hashed=hashed), 1000)
        self.assertEqual(self.run_finished(), ["copied 730"])
        self.assertEqual(self.machine.picture(730).read_bytes(), stand_in(OTHER_HASH))

    def test_a_hashed_folder_without_a_picture_is_passed_over(self):
        self.machine.add_game(730)
        (self.machine.cache / "730" / OTHER_HASH).mkdir(parents=True)
        self.assertEqual(self.run_finished(), ["missing 730"])
        self.machine.add_header(730, stand_in("hashed"), hashed=HASH)
        self.assertEqual(self.run_finished(), ["copied 730"])

    def test_only_a_hash_names_the_folder(self):
        self.machine.add_game(730)
        for name in (HASH.upper(), HASH[:-1], HASH + "0", "g" + HASH[1:], "library_600x900"):
            self.machine.add_header(730, hashed=name)
        self.assertEqual(self.run_finished(), ["missing 730"])

    def test_a_link_at_a_hashed_folder_is_passed_over(self):
        self.machine.add_game(730)
        elsewhere = self.machine.root / "elsewhere"
        elsewhere.mkdir()
        (elsewhere / "header.jpg").write_bytes(stand_in("someone else's"))
        (self.machine.cache / "730").mkdir()
        (self.machine.cache / "730" / OTHER_HASH).symlink_to(elsewhere)
        self.assertEqual(self.run_finished(), ["missing 730"])
        self.assertEqual(self.pictures(), [])
        # A real one beside it is still found.
        self.machine.add_header(730, stand_in("hashed"), hashed=HASH)
        self.assertEqual(self.run_finished(), ["copied 730"])
        self.assertEqual(self.machine.picture(730).read_bytes(), stand_in("hashed"))

    def test_a_file_with_a_hashed_name_is_passed_over(self):
        self.machine.add_game(730)
        (self.machine.cache / "730").mkdir()
        (self.machine.cache / "730" / HASH).write_bytes(stand_in())
        self.assertEqual(self.run_finished(), ["missing 730"])

    def test_a_hashed_picture_is_refused_as_any_other(self):
        for case, (reason, make) in refusals(self.machine.root).items():
            with self.subTest(case=case):
                shutil.rmtree(self.machine.cache / "730", ignore_errors=True)
                self.machine.add_game(730)
                path = self.machine.header(730, HASH)
                path.parent.mkdir(parents=True)
                make(path)
                self.assertEqual(self.run_finished(), [f"refused 730 {reason}"])
                self.assertEqual(self.pictures(), [])

    def test_a_picture_from_a_hashed_folder_is_left_as_it_is(self):
        self.machine.add_game(730)
        self.machine.add_header(730, hashed=HASH)
        self.assertEqual(self.run_finished(), ["copied 730"])
        self.assertEqual(self.run_finished(), [])


class LibraryHeaderTest(PicturesTestCase):
    """Steam keeps some games' picture as library_header.jpg instead of
    header.jpg, beside the hashed folders or in one. On the kids' laptop 9
    of the 25 shared games had only that name."""

    def test_a_library_header_is_copied(self):
        self.machine.add_game(100)
        self.machine.add_header(100, stand_in("library"), name=LIBRARY_HEADER)
        self.assertEqual(self.run_finished(), ["copied 100"])
        self.assertEqual(self.machine.picture(100).read_bytes(), stand_in("library"))

    def test_a_library_header_in_a_hashed_folder_is_copied(self):
        self.machine.add_game(730)
        self.machine.add_header(730, stand_in("hashed library"), hashed=HASH, name=LIBRARY_HEADER)
        self.assertEqual(self.run_finished(), ["copied 730"])
        self.assertEqual(self.machine.picture(730).read_bytes(), stand_in("hashed library"))

    def test_a_header_comes_before_a_library_header_beside_it(self):
        self.machine.add_game(730)
        self.machine.add_header(730, stand_in("library"), name=LIBRARY_HEADER)
        self.machine.add_header(730, stand_in("header"))
        self.assertEqual(self.run_finished(), ["copied 730"])
        self.assertEqual(self.machine.picture(730).read_bytes(), stand_in("header"))

    def test_a_header_comes_before_a_library_header_in_the_same_hashed_folder(self):
        self.machine.add_game(730)
        self.machine.add_header(730, stand_in("library"), hashed=HASH, name=LIBRARY_HEADER)
        self.machine.add_header(730, stand_in("header"), hashed=HASH)
        self.assertEqual(self.run_finished(), ["copied 730"])
        self.assertEqual(self.machine.picture(730).read_bytes(), stand_in("header"))

    def test_a_library_header_beside_the_hashed_folder_comes_first(self):
        self.machine.add_game(730)
        self.machine.add_header(730, stand_in("hashed"), hashed=HASH)
        self.machine.add_header(730, stand_in("library"), name=LIBRARY_HEADER)
        self.assertEqual(self.run_finished(), ["copied 730"])
        self.assertEqual(self.machine.picture(730).read_bytes(), stand_in("library"))

    def test_a_refused_header_is_not_passed_over_for_a_library_header(self):
        self.machine.add_game(730)
        for hashed in (None, HASH):
            with self.subTest(hashed=hashed):
                shutil.rmtree(self.machine.cache / "730", ignore_errors=True)
                self.machine.add_header(730, stand_in("library"), hashed=hashed,
                                        name=LIBRARY_HEADER)
                self.machine.add_header(730, b"not a jpeg", hashed=hashed)
                self.assertEqual(self.run_finished(), ["refused 730 not-a-jpeg"])
                self.assertEqual(self.pictures(), [])

    def test_of_two_hashed_folders_the_newest_picture_wins_whatever_its_name(self):
        self.machine.add_game(730)
        for newer, expected in ((HASH, "header"), (OTHER_HASH, "library")):
            with self.subTest(newer=newer):
                shutil.rmtree(self.machine.cache / "730", ignore_errors=True)
                header = self.machine.add_header(730, stand_in("header"), hashed=HASH)
                library = self.machine.add_header(730, stand_in("library"), hashed=OTHER_HASH,
                                                  name=LIBRARY_HEADER)
                self.set_time(header, 2000 if newer == HASH else 1000)
                self.set_time(library, 1000 if newer == HASH else 2000)
                self.run_finished()
                self.assertEqual(self.machine.picture(730).read_bytes(), stand_in(expected))

    def test_a_library_header_is_refused_as_any_other(self):
        for hashed in (None, HASH):
            for case, (reason, make) in refusals(self.machine.root).items():
                with self.subTest(hashed=hashed, case=case):
                    shutil.rmtree(self.machine.cache / "730", ignore_errors=True)
                    self.machine.add_game(730)
                    path = self.machine.header(730, hashed, LIBRARY_HEADER)
                    path.parent.mkdir(parents=True)
                    make(path)
                    self.assertEqual(self.run_finished(), [f"refused 730 {reason}"])
                    self.assertEqual(self.pictures(), [])

    def test_only_those_two_names_are_the_picture(self):
        self.machine.add_game(730)
        for name in ("library_600x900.jpg", "library_hero.jpg", "logo.png", "Header.jpg",
                     "library_header.jpeg"):
            self.machine.add_header(730, name=name)
            self.machine.add_header(730, hashed=HASH, name=name)
        self.assertEqual(self.run_finished(), ["missing 730"])


class RefusedTest(PicturesTestCase):
    def refused(self, appid=100):
        lines = self.run_finished()
        self.assertEqual(len(lines), 1, lines)
        self.assertTrue(lines[0].startswith(f"refused {appid} "), lines)
        return lines[0].split()[-1]

    def test_a_link_at_the_picture(self):
        self.machine.add_game(100)
        elsewhere = self.machine.root / "elsewhere.jpg"
        elsewhere.write_bytes(stand_in("someone else's"))
        (self.machine.cache / "100").mkdir()
        self.machine.header(100).symlink_to(elsewhere)
        self.assertEqual(self.refused(), "not-a-file")
        self.assertEqual(self.pictures(), [])

    def test_a_link_at_the_game_folder(self):
        self.machine.add_game(100)
        elsewhere = self.machine.root / "elsewhere"
        elsewhere.mkdir()
        (elsewhere / "header.jpg").write_bytes(stand_in("someone else's"))
        (self.machine.cache / "100").symlink_to(elsewhere)
        self.assertEqual(self.refused(), "not-a-file")
        self.assertEqual(self.pictures(), [])

    def test_a_folder_at_the_picture(self):
        self.machine.add_game(100)
        self.machine.header(100).mkdir(parents=True)
        self.assertEqual(self.refused(), "not-a-file")

    def test_a_fifo_at_the_picture(self):
        # Opened without waiting for a writer, so the run still ends.
        self.machine.add_game(100)
        (self.machine.cache / "100").mkdir()
        os.mkfifo(self.machine.header(100))
        self.assertEqual(self.refused(), "not-a-file")

    def test_a_picture_too_big(self):
        self.machine.add_game(100)
        self.machine.add_header(100, stand_in().ljust(LIMIT + 1, b"x"))
        self.assertEqual(self.refused(), "too-big")
        self.assertEqual(self.pictures(), [])

    def test_a_picture_that_is_not_a_jpeg(self):
        for data in (b"\x89PNG\r\n\x1a\n", b"", b"\xff\xd8"):
            with self.subTest(data=data):
                self.machine.add_game(100)
                self.machine.add_header(100, data)
                self.assertEqual(self.refused(), "not-a-jpeg")
                self.assertEqual(self.pictures(), [])

    @needs_not_root
    def test_a_picture_that_cannot_be_read(self):
        self.machine.add_game(100)
        self.machine.add_header(100).chmod(0)
        self.assertEqual(self.refused(), "unreadable")

    def test_a_picture_is_kept_when_its_source_is_refused(self):
        self.machine.add_game(100)
        self.machine.add_header(100, stand_in("kept"))
        self.run_finished()
        self.machine.add_header(100, b"not a jpeg")
        self.assertEqual(self.refused(), "not-a-jpeg")
        self.assertEqual(self.machine.picture(100).read_bytes(), stand_in("kept"))


class SharedFolderTest(PicturesTestCase):
    def test_a_picture_goes_when_its_game_leaves_the_shared_folder(self):
        for appid in (100, 200):
            self.machine.add_game(appid)
            self.machine.add_header(appid)
        self.run_finished()
        self.machine.remove_game(100)
        self.assertEqual(self.run_finished(), ["removed 100"])
        self.assertEqual(self.pictures(), ["200.jpg"])

    def test_a_game_without_a_tile_has_no_picture_and_no_note(self):
        self.machine.add_game(100)
        self.machine.add_header(100)
        self.run_finished()
        # An update that is still downloading: kidscan gives it no tile.
        self.machine.add_game(100, state_flags="1026")
        self.machine.add_game(200, state_flags="2")
        self.assertEqual(self.run_finished(), ["removed 100"])
        self.assertEqual(self.pictures(), [])

    def test_a_manifest_name_without_a_game_number_is_ignored(self):
        for appid in ("abc", "012", "0", "12345678901"):
            self.machine.add_game(appid)
            self.machine.add_header(appid)
        (self.machine.shared / "steamapps" / "appmanifest_100.acf.bak").write_text("")
        self.assertEqual(self.run_finished(), [])
        self.assertEqual(self.pictures(), [])

    def test_a_link_named_as_a_manifest_is_not_a_game(self):
        manifest = self.machine.root / "manifest.acf"
        manifest.write_text(manifest_text(100))
        (self.machine.shared / "steamapps" / "common" / "Game 100").mkdir(parents=True)
        (self.machine.shared / "steamapps" / "appmanifest_100.acf").symlink_to(manifest)
        self.machine.add_header(100)
        self.assertEqual(self.run_finished(), [])
        self.assertEqual(self.pictures(), [])

    def test_leftovers_are_cleared_without_a_word(self):
        (self.machine.pictures / ".cairn-100.jpg").write_bytes(b"half")
        (self.machine.pictures / ".cairn-other").write_bytes(b"")
        self.assertEqual(self.run_finished(), [])
        self.assertEqual(self.pictures(), [])

    def test_files_that_are_not_pictures_stay(self):
        for name in ("README", "100.png", "x100.jpg"):
            (self.machine.pictures / name).write_bytes(b"")
        self.assertEqual(self.run_finished(), [])
        self.assertEqual(self.pictures(), ["100.png", "README", "x100.jpg"])

    def test_a_steamapps_that_is_a_link_fails_and_removes_nothing(self):
        self.machine.add_game(100)
        self.machine.add_header(100)
        self.run_finished()
        steamapps = self.machine.shared / "steamapps"
        moved = self.machine.root / "someone-elses-steamapps"
        steamapps.rename(moved)
        steamapps.symlink_to(moved)
        self.machine.add_header(200)
        (moved / "appmanifest_200.acf").write_text(manifest_text(200))
        (moved / "common" / "Game 200").mkdir()
        self.assertEqual(self.machine.run(), ["failed"])
        self.assertEqual(self.machine.code, 1)
        self.assertEqual(self.pictures(), ["100.jpg"])

    def test_no_shared_library_fails_and_removes_nothing(self):
        self.machine.add_game(100)
        self.machine.add_header(100)
        self.run_finished()
        shutil.rmtree(self.machine.shared / "steamapps")
        self.assertEqual(self.machine.run(), ["failed"])
        self.assertEqual(self.machine.code, 1)
        self.assertEqual(self.pictures(), ["100.jpg"])


class FolderTest(PicturesTestCase):
    def assertRefusedFolder(self):
        self.assertEqual(self.machine.run(), ["refused folder"])
        self.assertEqual(self.machine.code, 1)

    def test_a_link_for_the_pictures_folder_is_refused_untouched(self):
        self.machine.add_game(100)
        self.machine.add_header(100)
        elsewhere = self.machine.root / "elsewhere"
        elsewhere.mkdir()
        (elsewhere / "200.jpg").write_bytes(b"")
        (elsewhere / ".cairn-200.jpg").write_bytes(b"")
        self.machine.pictures.rmdir()
        self.machine.pictures.symlink_to(elsewhere)
        self.assertRefusedFolder()
        self.assertEqual(sorted(p.name for p in elsewhere.iterdir()), [".cairn-200.jpg", "200.jpg"])

    def test_no_pictures_folder_is_refused(self):
        self.machine.pictures.rmdir()
        self.assertRefusedFolder()
        self.assertFalse(self.machine.pictures.exists())

    def test_a_file_for_the_pictures_folder_is_refused(self):
        self.machine.pictures.rmdir()
        self.machine.pictures.write_bytes(b"")
        self.assertRefusedFolder()


if __name__ == "__main__":
    unittest.main(verbosity=2)
