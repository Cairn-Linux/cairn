#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Runs cairn-copy-games.py, the copy of each shared game into a child's own
Steam library (ADR-0033), on folders made for each test.

Standard library only. The program is installed into a temporary folder
beside kidscan.py, as provisioning installs it, and run there with
`python3 -I`, or loaded from there when a test needs to change one of its
parts. It always gets --shared and --library, so it never reads this
account's own Steam.

A copy needs a filesystem that can reflink (btrfs or XFS). The tests look
for one in the default temporary folder and then in the build folder, and
skip only the tests that copy, saying so, when there is none. Everything
else runs anywhere, and "no-reflink" runs on /dev/shm, which is tmpfs.
"""

import contextlib
import importlib.machinery
import importlib.util
import io
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

HERE = pathlib.Path(__file__).resolve().parent
PROGRAM = HERE.parent / "cairn-copy-games.py"
KIDSCAN = HERE.parents[1] / "tools" / "kidscan" / "kidscan.py"
BUILD = HERE.parents[1] / "build"

# Every line the program may print (its docstring).
GRAMMAR = re.compile(
    r"(copied|have) [0-9]{1,10}|skipped [0-9]{1,10} [a-z-]{1,32}"
    r"|unnumbered|no-library|no-shared|lists-shared|no-reflink"
)


def reflinks(folder):
    """Whether cp can reflink inside `folder`."""
    with tempfile.TemporaryDirectory(dir=folder) as work:
        source = pathlib.Path(work, "source")
        source.write_bytes(b"x" * 8192)
        done = subprocess.run(
            ["cp", "--reflink=always", str(source), str(pathlib.Path(work, "copy"))],
            capture_output=True,
            check=False,
        )
        return done.returncode == 0


def find_reflink_place():
    for candidate in (pathlib.Path(tempfile.gettempdir()), BUILD):
        try:
            if candidate.is_dir() and reflinks(candidate):
                return candidate
        except OSError:
            continue
    return None


REFLINK_PLACE = find_reflink_place()
NO_REFLINK = "no folder that can reflink (btrfs or XFS) in the temporary folder or build/"


def needs_reflink(test):
    return unittest.skipIf(REFLINK_PLACE is None, NO_REFLINK)(test)


def manifest(appid, installdir="A Game", flags="4", extra="", name="A Game"):
    return (
        '"AppState"\n{\n'
        f'\t"appid"\t\t"{appid}"\n'
        f'\t"name"\t\t"{name}"\n'
        f'\t"StateFlags"\t\t"{flags}"\n'
        f'\t"installdir"\t\t"{installdir}"\n'
        f"{extra}"
        "}\n"
    ).encode()


class Machine:
    """A shared library, a child's Steam, and the program installed beside
    kidscan.py, all under one folder."""

    def __init__(self, root):
        self.root = pathlib.Path(root)
        self.libexec = self.root / "libexec"
        self.libexec.mkdir()
        shutil.copy(PROGRAM, self.libexec / "cairn-copy-games")
        shutil.copy(KIDSCAN, self.libexec / "kidscan.py")
        self.shared = self.root / "var" / "steam-library" / "steamapps"
        (self.shared / "common").mkdir(parents=True)
        self.steam = self.root / "home" / "ada" / ".local" / "share" / "Steam"
        self.library = self.steam / "steamapps"
        (self.library / "common").mkdir(parents=True)

    def add_game(self, appid, installdir="A Game", data=None, files=None):
        """A game in the shared library: its manifest, and a folder with a
        program and a data file unless `files` says otherwise."""
        (self.shared / f"appmanifest_{appid}.acf").write_bytes(data or manifest(appid, installdir))
        folder = self.shared / "common" / installdir
        folder.mkdir(parents=True, exist_ok=True)
        for name, (content, mode) in (files or {
            "start.sh": (b"#!/bin/sh\necho start\n", 0o755),
            "data/level1.dat": (b"level" * 1000, 0o644),
        }).items():
            path = folder / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(content)
            path.chmod(mode)
        return folder

    def run(self):
        done = subprocess.run(
            [sys.executable, "-I", str(self.libexec / "cairn-copy-games"),
             "--shared", str(self.shared), "--library", str(self.library)],
            stdin=subprocess.DEVNULL,
            capture_output=True,
            text=True,
            check=False,
        )
        self.code = done.returncode
        self.stderr = done.stderr
        return done.stdout.splitlines()

    def load(self):
        """The installed program as a module, to change one of its parts."""
        loader = importlib.machinery.SourceFileLoader(
            "cairn_copy_games", str(self.libexec / "cairn-copy-games"))
        spec = importlib.util.spec_from_loader(loader.name, loader)
        module = importlib.util.module_from_spec(spec)
        loader.exec_module(module)
        return module

    def run_in_process(self, module, *arguments):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = module.main(["--shared", str(self.shared), "--library", str(self.library), *arguments])
        return code, out.getvalue().splitlines()


class CopyTestCase(unittest.TestCase):
    place = None

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(dir=self.place)
        self.machine = Machine(self.directory.name)

    def tearDown(self):
        self.directory.cleanup()

    def assertOnlyGrammar(self, lines):
        for line in lines:
            self.assertRegex(line, f"^(?:{GRAMMAR.pattern})$")
            self.assertNotIn("/", line)


class WhichGamesTest(CopyTestCase):
    """Every reason to leave a game out comes from its manifest's one read,
    and none of them needs a copy, so these run on any filesystem."""

    def skipped(self, appid, data, **game):
        self.machine.add_game(appid, data=data, **game)
        lines = self.machine.run()
        self.assertEqual(self.machine.code, 0, self.machine.stderr)
        self.assertOnlyGrammar(lines)
        return lines

    def assertNothingCopied(self):
        self.assertEqual(sorted(p.name for p in self.machine.library.iterdir()), ["common"])
        self.assertEqual(list((self.machine.library / "common").iterdir()), [])

    def test_an_appid_that_is_not_its_files(self):
        self.assertEqual(self.skipped(100, manifest(200)), ["skipped 100 appid"])
        self.assertNothingCopied()

    def test_an_appid_that_is_not_a_number(self):
        self.assertEqual(self.skipped(100, manifest("10a")), ["skipped 100 appid"])

    def test_a_file_name_without_a_number(self):
        (self.machine.shared / "appmanifest_x1.acf").write_bytes(manifest(1))
        (self.machine.shared / "appmanifest_012.acf").write_bytes(manifest("012"))
        self.assertEqual(self.machine.run(), ["unnumbered", "unnumbered"])
        self.assertNothingCopied()

    def test_an_installdir_that_is_not_one_plain_folder_name(self):
        for installdir in ("", ".", "..", "a/b", "../../.ssh", ".cairn-copy-100", "a\x00b"):
            with self.subTest(installdir=installdir):
                data = manifest(100, installdir=installdir)
                (self.machine.shared / "appmanifest_100.acf").write_bytes(data)
                self.assertEqual(self.machine.run(), ["skipped 100 installdir"])
                self.assertNothingCopied()

    def test_a_beta_branch(self):
        for block in ("UserConfig", "MountedConfig"):
            with self.subTest(block=block):
                extra = f'\t"{block}"\n\t{{\n\t\t"BetaKey"\t\t"beta"\n\t}}\n'
                (self.machine.shared / "appmanifest_100.acf").write_bytes(manifest(100, extra=extra))
                (self.machine.shared / "common" / "A Game").mkdir(exist_ok=True)
                self.assertEqual(self.machine.run(), ["skipped 100 beta-branch"])
                self.assertNothingCopied()

    def test_anything_but_fully_installed(self):
        for flags in ("6", "1026", "0", "", "44"):
            with self.subTest(flags=flags):
                (self.machine.shared / "appmanifest_100.acf").write_bytes(manifest(100, flags=flags))
                self.assertEqual(self.machine.run(), ["skipped 100 not-installed"])

    def test_something_downloading(self):
        (self.machine.shared / "downloading" / "100").mkdir(parents=True)
        self.assertEqual(self.skipped(100, None), ["skipped 100 downloading"])
        self.assertNothingCopied()

    def test_no_folder_in_the_shared_library(self):
        (self.machine.shared / "appmanifest_100.acf").write_bytes(manifest(100))
        self.assertEqual(self.machine.run(), ["skipped 100 no-folder"])

    def test_a_shared_folder_that_is_a_link(self):
        elsewhere = self.machine.root / "elsewhere"
        elsewhere.mkdir()
        (self.machine.shared / "common" / "A Game").symlink_to(elsewhere)
        (self.machine.shared / "appmanifest_100.acf").write_bytes(manifest(100))
        self.assertEqual(self.machine.run(), ["skipped 100 no-folder"])

    def test_a_manifest_it_cannot_read(self):
        for data in (b'"AppState"\n{\n\t"appid" "100"\n', b"\xff\xfe" + manifest(100), b"just words"):
            with self.subTest(data=data[:12]):
                (self.machine.shared / "appmanifest_100.acf").write_bytes(data)
                self.assertEqual(self.machine.run(), ["skipped 100 unreadable"])

    def test_what_the_child_has_is_never_touched(self):
        # Their copy holds their saves: not replaced, updated or deleted,
        # whatever the shared one says now.
        self.machine.add_game(100, data=manifest(100, flags="6"))
        own = self.machine.library / "appmanifest_100.acf"
        own.write_bytes(b"the child's own\n")
        save = self.machine.library / "common" / "A Game" / "Saves" / "save1"
        save.parent.mkdir(parents=True)
        save.write_bytes(b"mine")
        self.assertEqual(self.machine.run(), ["have 100"])
        self.assertEqual(own.read_bytes(), b"the child's own\n")
        self.assertEqual(save.read_bytes(), b"mine")
        self.assertEqual(sorted(p.name for p in save.parent.parent.iterdir()), ["Saves"])

    def test_a_folder_without_a_manifest_is_left_alone(self):
        self.machine.add_game(100)
        save = self.machine.library / "common" / "A Game" / "Saves" / "save1"
        save.parent.mkdir(parents=True)
        save.write_bytes(b"mine")
        self.assertEqual(self.machine.run(), ["skipped 100 folder-without-manifest"])
        self.assertEqual(save.read_bytes(), b"mine")
        self.assertFalse((self.machine.library / "appmanifest_100.acf").exists())


class ConditionsTest(CopyTestCase):
    def test_no_library_when_the_childs_steam_never_ran(self):
        shutil.rmtree(self.machine.steam)
        self.machine.add_game(100)
        self.assertEqual(self.machine.run(), ["no-library"])
        self.assertEqual(self.machine.code, 0)
        self.assertFalse(self.machine.steam.exists(), "nothing is made for it")

    def test_no_shared_library(self):
        shutil.rmtree(self.machine.shared)
        self.assertEqual(self.machine.run(), ["no-shared"])
        self.assertEqual(self.machine.code, 0)

    def test_a_childs_steam_that_lists_the_shared_library(self):
        shared_root = self.machine.shared.parent
        for name in ("config/libraryfolders.vdf", "steamapps/libraryfolders.vdf"):
            for path in (str(shared_root), str(shared_root) + "/"):
                with self.subTest(name=name, path=path):
                    vdf = self.machine.steam / name
                    vdf.parent.mkdir(parents=True, exist_ok=True)
                    vdf.write_text(
                        '"libraryfolders"\n{\n\t"0"\n\t{\n\t\t"path"\t\t"/home/ada/.local/share/Steam"\n\t}\n'
                        f'\t"1"\n\t{{\n\t\t"path"\t\t"{path}"\n\t}}\n}}\n'
                    )
                    self.machine.add_game(100)
                    self.assertEqual(self.machine.run(), ["lists-shared"])
                    self.assertEqual(list((self.machine.library / "common").iterdir()), [])
                    vdf.unlink()

    def test_another_library_in_the_list_is_no_reason_to_stop(self):
        vdf = self.machine.steam / "config" / "libraryfolders.vdf"
        vdf.parent.mkdir(parents=True)
        vdf.write_text(f'"libraryfolders"\n{{\n\t"1"\n\t{{\n\t\t"path"\t\t"{self.machine.shared.parent}-old"\n\t}}\n}}\n')
        self.machine.add_game(100, data=manifest(100, flags="6"))
        self.assertEqual(self.machine.run(), ["skipped 100 not-installed"])

    def test_leftovers_of_its_own_go_and_nothing_else(self):
        common = self.machine.library / "common"
        (common / ".cairn-copy-100" / "half").mkdir(parents=True)
        (common / ".cairn-copy-probe").write_bytes(b"")
        (self.machine.library / ".cairn-manifest-100").write_bytes(b"half")
        kept = [common / ".cairn-copy-notes", common / "Another Game",
                self.machine.library / ".cairn-manifest-x", self.machine.library / "appmanifest_5.acf"]
        for path in kept:
            path.write_bytes(b"keep")
        self.assertEqual(self.machine.run(), [])
        self.assertFalse((common / ".cairn-copy-100").exists())
        self.assertFalse((common / ".cairn-copy-probe").exists())
        self.assertFalse((self.machine.library / ".cairn-manifest-100").exists())
        for path in kept:
            self.assertEqual(path.read_bytes(), b"keep", path.name)

    def test_refuses_to_run_as_root(self):
        module = self.machine.load()
        self.machine.add_game(100)
        with mock.patch.object(module.os, "geteuid", return_value=0):
            code, lines = self.machine.run_in_process(module)
        self.assertEqual((code, lines), (2, ["refused root"]))
        self.assertEqual(list((self.machine.library / "common").iterdir()), [])

    def test_the_home_comes_from_the_password_database(self):
        module = self.machine.load()
        account = mock.Mock(pw_dir=str(self.machine.root / "home" / "ada"))
        out = io.StringIO()
        with (mock.patch.object(module.pwd, "getpwuid", return_value=account),
              mock.patch.dict(os.environ, {"HOME": str(self.machine.root / "elsewhere")}),
              contextlib.redirect_stdout(out)):
            module.main(["--shared", str(self.machine.root / "nowhere")])
        # It found the library under the home the password database gave.
        self.assertEqual(out.getvalue(), "no-shared\n")


class CopyTest(CopyTestCase):
    """Real copies, by reflink."""

    place = REFLINK_PLACE

    @needs_reflink
    def test_a_game_is_copied_with_its_manifest_byte_for_byte(self):
        # Unusual spacing and a key Cairn does not know stay as Steam wrote them.
        data = manifest(100, extra='\t"UserConfig"\n\t{\n\t\t"BetaKey"\t\t"public"\n\t}\n\t"Odd"   "x"\r\n')
        source = self.machine.add_game(100, data=data)
        os.utime(source / "start.sh", (1_000_000_000, 1_000_000_000))
        lines = self.machine.run()
        self.assertEqual(lines, ["copied 100"], self.machine.stderr)
        self.assertEqual(self.machine.code, 0)
        copy = self.machine.library / "common" / "A Game"
        self.assertEqual((self.machine.library / "appmanifest_100.acf").read_bytes(), data)
        self.assertEqual((copy / "data" / "level1.dat").read_bytes(), b"level" * 1000)
        self.assertEqual((copy / "start.sh").stat().st_mode & 0o777, 0o755, "the program still runs")
        self.assertEqual((copy / "data" / "level1.dat").stat().st_mode & 0o777, 0o644)
        self.assertEqual((copy / "start.sh").stat().st_mtime, 1_000_000_000)
        self.assertEqual((self.machine.library / "appmanifest_100.acf").stat().st_mode & 0o777, 0o644)
        self.assertEqual(sorted(p.name for p in (self.machine.library / "common").iterdir()), ["A Game"])
        self.assertEqual(sorted(p.name for p in self.machine.library.iterdir()),
                         ["appmanifest_100.acf", "common"])

    @needs_reflink
    def test_a_second_run_has_it(self):
        self.machine.add_game(100)
        self.assertEqual(self.machine.run(), ["copied 100"])
        self.assertEqual(self.machine.run(), ["have 100"])

    @needs_reflink
    def test_not_the_shared_folders_setgid_or_acl(self):
        source = self.machine.add_game(100)
        source.chmod(0o2775)
        (source / "data").chmod(0o2775)
        if shutil.which("setfacl"):
            subprocess.run(["setfacl", "-m", "d:u::rwx,d:g::rwx,d:o::r-x", str(source)],
                           capture_output=True, check=False)
        self.assertEqual(self.machine.run(), ["copied 100"])
        copy = self.machine.library / "common" / "A Game"
        for folder in (copy, copy / "data"):
            self.assertFalse(folder.stat().st_mode & 0o2000, folder)
            self.assertFalse([a for a in os.listxattr(folder) if a.startswith("system.posix_acl")], folder)

    @needs_reflink
    def test_each_game_on_its_own(self):
        self.machine.add_game(100, "First")
        self.machine.add_game(200, "Second")
        self.machine.add_game(300, "Third", data=manifest(300, "Third", flags="6"))
        lines = self.machine.run()
        self.assertEqual(lines, ["copied 100", "copied 200", "skipped 300 not-installed"])
        self.assertTrue((self.machine.library / "common" / "Second" / "start.sh").exists())

    @needs_reflink
    def test_a_manifest_that_changed_during_the_copy_throws_the_copy_away(self):
        self.machine.add_game(100)
        module = self.machine.load()
        real_copy = module.reflink_copy

        def copy_while_steam_updates(source, target):
            done = real_copy(source, target)
            if source.name == "A Game":
                (self.machine.shared / "appmanifest_100.acf").write_bytes(manifest(100, flags="6"))
            return done

        with mock.patch.object(module, "reflink_copy", copy_while_steam_updates):
            code, lines = self.machine.run_in_process(module)
        self.assertEqual((code, lines), (0, ["skipped 100 changed"]))
        self.assertEqual(list((self.machine.library / "common").iterdir()), [])
        self.assertEqual(sorted(p.name for p in self.machine.library.iterdir()), ["common"])

    def test_a_copy_that_fails_leaves_nothing_half_done(self):
        # cp stopping halfway, which needs no reflink to stand in for.
        self.machine.add_game(100)
        module = self.machine.load()

        def half_a_copy(source, target):
            if source.name != "A Game":
                return True
            target.mkdir()
            (target / "start.sh").write_bytes(b"half")
            return False

        with mock.patch.object(module, "reflink_copy", half_a_copy):
            code, lines = self.machine.run_in_process(module)
        self.assertEqual((code, lines), (0, ["skipped 100 copy-failed"]))
        self.assertEqual(list((self.machine.library / "common").iterdir()), [])
        self.assertEqual(sorted(p.name for p in self.machine.library.iterdir()), ["common"])

    def test_a_manifest_that_cannot_be_written_takes_the_folder_back_out(self):
        self.machine.add_game(100)
        module = self.machine.load()

        def copy_tree(source, target):
            if source.is_dir():
                shutil.copytree(source, target)
            return True

        with (mock.patch.object(module, "reflink_copy", copy_tree),
              mock.patch.object(module, "put_manifest", side_effect=OSError("disk full"))):
            code, lines = self.machine.run_in_process(module)
        self.assertEqual((code, lines), (0, ["skipped 100 copy-failed"]))
        self.assertEqual(list((self.machine.library / "common").iterdir()), [],
                         "no game folder without its manifest")

    @needs_reflink
    def test_nothing_it_prints_is_a_path_or_a_name(self):
        for appid, installdir in ((100, "Pajama Sam"), (200, "Spy Fox")):
            self.machine.add_game(appid, installdir)
        self.machine.add_game(300, data=manifest(300, name="Freddi Fish", flags="6"))
        self.machine.add_game(400, data=manifest(500))
        (self.machine.shared / "appmanifest_bad.acf").write_bytes(b"x")
        (self.machine.library / "appmanifest_200.acf").write_bytes(manifest(200))
        lines = self.machine.run()
        self.assertEqual(lines, ["copied 100", "have 200", "skipped 300 not-installed",
                                 "skipped 400 appid", "unnumbered"])
        self.assertOnlyGrammar(lines)
        for line in lines:
            for word in ("Pajama", "Spy", "Freddi", "ada", str(self.machine.root)):
                self.assertNotIn(word, line)


class NoReflinkTest(unittest.TestCase):
    """On tmpfs, which cannot reflink, nothing is copied."""

    @unittest.skipUnless(pathlib.Path("/dev/shm").is_dir(), "no /dev/shm")
    def test_nothing_is_copied_without_reflink(self):
        with tempfile.TemporaryDirectory(dir="/dev/shm") as root:
            if reflinks(root):
                self.skipTest("/dev/shm can reflink here")
            machine = Machine(root)
            machine.add_game(100)
            machine.add_game(200, "Second")
            self.assertEqual(machine.run(), ["no-reflink"])
            self.assertEqual(machine.code, 0)
            self.assertEqual(list((machine.library / "common").iterdir()), [])
            self.assertEqual(sorted(p.name for p in machine.library.iterdir()), ["common"])


if __name__ == "__main__":
    if REFLINK_PLACE is None:
        print(f"note: the tests that copy are skipped: {NO_REFLINK}", file=sys.stderr)
    unittest.main()
