#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Runs cairn-copy-games.py, the copy of each shared game into a child's own
Steam library (ADR-0033), and of Steam's tools after the games (ADR-0046),
on folders made for each test.

Standard library only. The program is installed into a temporary folder
beside kidscan.py, as provisioning installs it, and run there with
`python3 -I`, or loaded from there when a test needs to change one of its
parts. It always gets --shared, --library and --tools, so it never reads
this account's own Steam or the computer's tools folder. The tools folder
is not there unless a test makes it, as on a computer set up by
provisioning.

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
import signal
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
    r"|unnumbered|no-library|no-shared|lists-shared|unreadable-config|no-reflink"
    r"|tool-(copied|have|missing) [0-9]{1,10}|tool-skipped [0-9]{1,10} [a-z-]{1,32}"
    r"|copying-tools|no-tools|tools-unreadable"
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


# Root can read every file, so a file kept from this account is not one.
def needs_not_root(test):
    return unittest.skipIf(os.geteuid() == 0, "root can read every file")(test)


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
        # Made only by a test about Steam's tools.
        self.tools = self.root / "var" / "steam-tools"

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

    def add_tool(self, appid, installdir, name, build=100, runs_in=None, data=None):
        """One build of a tool in the tools folder, as cairn-share-tools
        makes it: <appid>-<build>, with the manifest and the tool's folder."""
        self.tools.mkdir(parents=True, exist_ok=True)
        folder = self.tools / f"{appid}-{build}"
        (folder / "common" / installdir / "files").mkdir(parents=True)
        extra = f'\t"buildid"\t\t"{build}"\n'
        (folder / f"appmanifest_{appid}.acf").write_bytes(
            data or manifest(appid, installdir, extra=extra, name=name))
        needs = f'\t"require_tool_appid" "{runs_in}"\n' if runs_in else ""
        tool = folder / "common" / installdir
        (tool / "toolmanifest.vdf").write_bytes(f'"manifest"\n{{\n\t"version" "2"\n{needs}}}\n'.encode())
        (tool / "_v2-entry-point").write_bytes(b"#!/bin/sh\n")
        (tool / "_v2-entry-point").chmod(0o755)
        (tool / "files" / "lib.so").write_bytes(b"lib" * 1000)
        return folder

    def run(self, *arguments):
        # A run that blocks fails the test rather than hanging it.
        done = subprocess.run(
            [sys.executable, "-I", str(self.libexec / "cairn-copy-games"), "--shared", str(self.shared),
             "--library", str(self.library), "--tools", str(self.tools), *arguments],
            stdin=subprocess.DEVNULL,
            capture_output=True,
            text=True,
            check=False,
            timeout=60,
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
            code = module.main(["--shared", str(self.shared), "--library", str(self.library),
                                "--tools", str(self.tools), *arguments])
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

    def test_a_library_list_that_is_not_a_plain_file_stops_everything(self):
        # Each would block the read for ever, or never end, or is too big
        # to be Steam's.
        self.machine.add_game(100)
        big = self.machine.root / "big.vdf"
        big.write_bytes(b"x" * ((1 << 20) + 1))
        real = self.machine.root / "real.vdf"
        real.write_text('"libraryfolders"\n{\n}\n')
        for name in ("config/libraryfolders.vdf", "steamapps/libraryfolders.vdf"):
            vdf = self.machine.steam / name
            vdf.parent.mkdir(parents=True, exist_ok=True)
            makers = {
                "fifo": lambda: os.mkfifo(vdf),
                "zero": lambda: vdf.symlink_to("/dev/zero"),
                "link": lambda: vdf.symlink_to(real),
                "folder": lambda: vdf.mkdir(),
                "big": lambda: shutil.copy(big, vdf),
            }
            for kind, make in makers.items():
                with self.subTest(name=name, kind=kind):
                    make()
                    lines = self.machine.run()
                    if vdf.is_dir() and not vdf.is_symlink():
                        vdf.rmdir()
                    else:
                        vdf.unlink()
                    self.assertEqual(lines, ["unreadable-config"], self.machine.stderr)
                    self.assertEqual(self.machine.code, 0)
                    self.assertEqual(list((self.machine.library / "common").iterdir()), [])

    def test_an_empty_library_list_is_no_reason_to_stop(self):
        vdf = self.machine.steam / "config" / "libraryfolders.vdf"
        vdf.parent.mkdir(parents=True)
        vdf.write_bytes(b"")
        self.machine.add_game(100, data=manifest(100, flags="6"))
        self.assertEqual(self.machine.run(), ["skipped 100 not-installed"])

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
    def test_a_list_copies_only_its_games(self):
        self.machine.add_game(100, "First")
        self.machine.add_game(200, "Second")
        self.assertEqual(self.machine.run("--only", "200"), ["copied 200"], self.machine.stderr)
        self.assertEqual(sorted(p.name for p in (self.machine.library / "common").iterdir()), ["Second"])
        self.assertEqual(self.machine.run("--only", "100", "200"), ["copied 100", "have 200"])

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

    @staticmethod
    def copy_tree(source, target):
        """A stand-in for cp that needs no reflink."""
        if source.is_dir():
            shutil.copytree(source, target)
        return True

    def assertNothingLeft(self):
        self.assertEqual(list((self.machine.library / "common").iterdir()), [],
                         "no game folder without its manifest")
        self.assertEqual(sorted(p.name for p in self.machine.library.iterdir()), ["common"])

    def test_a_manifest_that_cannot_be_written_leaves_nothing(self):
        self.machine.add_game(100)
        module = self.machine.load()
        with (mock.patch.object(module, "reflink_copy", self.copy_tree),
              mock.patch.object(module, "write_manifest", side_effect=OSError("disk full"))):
            code, lines = self.machine.run_in_process(module)
        self.assertEqual((code, lines), (0, ["skipped 100 copy-failed"]))
        self.assertNothingLeft()

    def test_a_manifest_that_cannot_go_into_place_takes_the_folder_back_out(self):
        self.machine.add_game(100)
        module = self.machine.load()
        real_rename = os.rename

        def rename(source, target):
            if pathlib.Path(target).name == "appmanifest_100.acf":
                raise OSError("disk full")
            real_rename(source, target)

        with (mock.patch.object(module, "reflink_copy", self.copy_tree),
              mock.patch.object(module.os, "rename", rename)):
            code, lines = self.machine.run_in_process(module)
        self.assertEqual((code, lines), (0, ["skipped 100 copy-failed"]))
        self.assertNothingLeft()

    def test_the_manifest_is_whole_before_the_folder_goes_into_place(self):
        self.machine.add_game(100)
        module = self.machine.load()
        real_rename = os.rename
        seen = []

        def rename(source, target):
            if pathlib.Path(target).name == "A Game":
                waiting = self.machine.library / ".cairn-manifest-100"
                seen.append(waiting.read_bytes())
            real_rename(source, target)

        with (mock.patch.object(module, "reflink_copy", self.copy_tree),
              mock.patch.object(module.os, "rename", rename)):
            code, lines = self.machine.run_in_process(module)
        self.assertEqual((code, lines), (0, ["copied 100"]))
        self.assertEqual(seen, [manifest(100)])

    def test_a_ctrl_c_between_the_folder_and_its_manifest_waits_for_both(self):
        # The signal comes after the folder is in place, and is felt only
        # once the manifest is too.
        self.machine.add_game(100)
        module = self.machine.load()
        real_rename = os.rename

        def rename(source, target):
            real_rename(source, target)
            if pathlib.Path(target).name == "A Game":
                os.kill(os.getpid(), signal.SIGINT)

        with (mock.patch.object(module, "reflink_copy", self.copy_tree),
              mock.patch.object(module.os, "rename", rename),
              self.assertRaises(KeyboardInterrupt)):
            self.machine.run_in_process(module)
        self.assertTrue((self.machine.library / "common" / "A Game" / "start.sh").exists())
        self.assertEqual((self.machine.library / "appmanifest_100.acf").read_bytes(), manifest(100))
        self.assertEqual(signal.pthread_sigmask(signal.SIG_BLOCK, []), set(), "nothing stays held")

    @needs_not_root
    def test_a_shared_file_this_account_cannot_read_has_its_own_reason(self):
        for kind in ("file", "folder"):
            with self.subTest(kind=kind):
                source = self.machine.add_game(100)
                private = source / "data" / "level1.dat" if kind == "file" else source / "data"
                private.chmod(0o000 if kind == "file" else 0o600)
                module = self.machine.load()
                with mock.patch.object(module, "reflink_copy", lambda source, target: source.name != "A Game"):
                    code, lines = self.machine.run_in_process(module)
                private.chmod(0o755)
                self.assertEqual((code, lines), (0, ["skipped 100 unreadable-files"]))
                self.assertNothingLeft()

    @needs_reflink
    @needs_not_root
    def test_cp_failing_on_a_private_file_says_so(self):
        source = self.machine.add_game(100)
        (source / "data" / "level1.dat").chmod(0o000)
        lines = self.machine.run()
        (source / "data" / "level1.dat").chmod(0o644)
        self.assertEqual(lines, ["skipped 100 unreadable-files"])
        self.assertNothingLeft()

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


class OneChildsListTest(CopyTestCase):
    """--only: the games on one child's own list (ADR-0041). Copies use a
    stand-in for cp, so these run on any filesystem."""

    def run_listed(self, *appids):
        module = self.machine.load()
        with mock.patch.object(module, "reflink_copy", CopyTest.copy_tree):
            return module, self.machine.run_in_process(module, "--only", *appids)

    def games(self):
        return sorted(p.name for p in (self.machine.library / "common").iterdir())

    def test_only_the_listed_games_are_copied(self):
        self.machine.add_game(100, "First")
        self.machine.add_game(200, "Second")
        self.machine.add_game(300, "Third")
        _, (code, lines) = self.run_listed("300", "100")
        self.assertEqual((code, lines), (0, ["copied 100", "copied 300"]))
        self.assertEqual(self.games(), ["First", "Third"])
        self.assertFalse((self.machine.library / "appmanifest_200.acf").exists())

    def test_a_listed_game_twice_is_one_game(self):
        self.machine.add_game(100)
        _, (code, lines) = self.run_listed("100", "100")
        self.assertEqual((code, lines), (0, ["copied 100"]))

    def test_a_listed_game_not_in_the_shared_library_is_reported(self):
        self.machine.add_game(100)
        _, (code, lines) = self.run_listed("100", "555")
        self.assertEqual((code, lines), (0, ["copied 100", "skipped 555 not-shared"]))
        self.assertOnlyGrammar(lines)

    def test_a_listed_game_is_left_out_for_the_same_reasons(self):
        self.machine.add_game(100, data=manifest(100, flags="6"))
        self.machine.add_game(200, "Second", data=manifest(999, "Second"))
        _, (code, lines) = self.run_listed("100", "200")
        self.assertEqual((code, lines), (0, ["skipped 100 not-installed", "skipped 200 appid"]))
        self.assertEqual(self.games(), [])

    def test_a_listed_game_the_child_has_is_never_touched(self):
        self.machine.add_game(100)
        own = self.machine.library / "appmanifest_100.acf"
        own.write_bytes(b"the child's own\n")
        _, (code, lines) = self.run_listed("100")
        self.assertEqual((code, lines), (0, ["have 100"]))
        self.assertEqual(own.read_bytes(), b"the child's own\n")

    def test_a_game_off_the_list_is_never_deleted(self):
        # Taking a game off a child's list keeps their copy and saves.
        self.machine.add_game(100, "First")
        self.machine.add_game(200, "Second")
        kept = self.machine.library / "common" / "Second" / "Saves" / "save1"
        kept.parent.mkdir(parents=True)
        kept.write_bytes(b"mine")
        (self.machine.library / "appmanifest_200.acf").write_bytes(manifest(200, "Second"))
        _, (code, lines) = self.run_listed("100")
        self.assertEqual((code, lines), (0, ["copied 100"]))
        self.assertEqual(kept.read_bytes(), b"mine")
        self.assertTrue((self.machine.library / "appmanifest_200.acf").exists())

    def test_an_appid_that_is_not_a_plain_number_changes_nothing(self):
        self.machine.add_game(100)
        leftover = self.machine.library / "common" / ".cairn-copy-100"
        leftover.mkdir()
        for bad in ("10a", "0100", "-5", "", " 100", "100/..", "../100", "12345678901", "١٢"):
            with self.subTest(appid=bad):
                lines = self.machine.run("--only", "100", bad)
                self.assertEqual((self.machine.code, lines), (2, ["refused appid"]))
                self.assertEqual(self.games(), [".cairn-copy-100"], "not even its leftovers go")

    def test_an_empty_list_is_a_mistake_not_every_game(self):
        self.machine.add_game(100)
        lines = self.machine.run("--only")
        self.assertEqual(lines, [])
        self.assertEqual(self.machine.code, 2)
        self.assertEqual(self.games(), [])

    def test_no_list_still_copies_every_game(self):
        self.machine.add_game(100, "First")
        self.machine.add_game(200, "Second")
        module = self.machine.load()
        with mock.patch.object(module, "reflink_copy", CopyTest.copy_tree):
            code, lines = self.machine.run_in_process(module)
        self.assertEqual((code, lines), (0, ["copied 100", "copied 200"]))

    def test_a_list_still_refuses_root(self):
        self.machine.add_game(100)
        module = self.machine.load()
        with mock.patch.object(module.os, "geteuid", return_value=0):
            code, lines = self.machine.run_in_process(module, "--only", "100")
        self.assertEqual((code, lines), (2, ["refused root"]))
        self.assertEqual(self.games(), [])


SCOUT = {"appid": "1070560", "installdir": "SteamLinuxRuntime", "name": "Steam Linux Runtime 1.0 (scout)",
         "runs_in": "1391110"}
SOLDIER = {"appid": "1391110", "installdir": "SteamLinuxRuntime_soldier",
           "name": "Steam Linux Runtime 2.0 (soldier)"}
PROTON = {"appid": "1493710", "installdir": "Proton - Experimental", "name": "Proton - Experimental",
          "runs_in": "4183110"}


class ToolsTest(CopyTestCase):
    """Steam's tools from the tools folder, after the games (ADR-0046).
    Most copies use a stand-in for cp, and run on any filesystem; the tests
    that run the program as it is need reflink."""

    place = REFLINK_PLACE

    def run_with_stand_in(self, *arguments, **patches):
        module = self.machine.load()
        with contextlib.ExitStack() as stack:
            stack.enter_context(mock.patch.object(module, "reflink_copy", CopyTest.copy_tree))
            for name, value in patches.items():
                stack.enter_context(mock.patch.object(module, name, value))
            return self.machine.run_in_process(module, *arguments)

    def child_has(self, appid):
        return (self.machine.library / f"appmanifest_{appid}.acf").exists()

    def test_each_tool_the_child_lacks_comes_after_the_games(self):
        self.machine.add_game(100)
        build = self.machine.add_tool(**SCOUT)
        self.machine.add_tool(**SOLDIER)
        code, lines = self.run_with_stand_in()
        self.assertEqual((code, lines), (0, ["copied 100", "copying-tools", "tool-copied 1070560",
                                             "tool-copied 1391110"]))
        self.assertOnlyGrammar(lines)
        self.assertEqual((self.machine.library / "appmanifest_1070560.acf").read_bytes(),
                         (build / "appmanifest_1070560.acf").read_bytes())
        copy = self.machine.library / "common" / "SteamLinuxRuntime"
        self.assertEqual((copy / "files" / "lib.so").read_bytes(), b"lib" * 1000)
        self.assertTrue((self.machine.library / "common" / "SteamLinuxRuntime_soldier" / "toolmanifest.vdf").is_file())

    @needs_reflink
    def test_a_real_copy_keeps_no_setgid_or_acl_of_the_tools_folder(self):
        build = self.machine.add_tool(**SOLDIER)
        tool = build / "common" / "SteamLinuxRuntime_soldier"
        for folder in (self.machine.tools, build, tool, tool / "files"):
            folder.chmod(0o2775)
        if shutil.which("setfacl"):
            subprocess.run(["setfacl", "-m", "d:u::rwx,d:g::rwx,d:o::r-x", str(tool)], capture_output=True,
                           check=False)
        os.utime(tool / "_v2-entry-point", (1_000_000_000, 1_000_000_000))
        lines = self.machine.run()
        self.assertEqual(lines, ["copying-tools", "tool-copied 1391110"], self.machine.stderr)
        copy = self.machine.library / "common" / "SteamLinuxRuntime_soldier"
        self.assertEqual((copy / "_v2-entry-point").stat().st_mode & 0o777, 0o755, "it still runs")
        self.assertEqual((copy / "_v2-entry-point").stat().st_mtime, 1_000_000_000)
        for folder in (copy, copy / "files"):
            self.assertFalse(folder.stat().st_mode & 0o2000, folder)
            self.assertFalse([a for a in os.listxattr(folder) if a.startswith("system.posix_acl")], folder)

    def test_copying_tools_is_said_once_and_only_before_a_copy(self):
        self.machine.add_tool(**SCOUT)
        self.machine.add_tool(**SOLDIER)
        self.assertEqual(self.run_with_stand_in(), (0, ["copying-tools", "tool-copied 1070560",
                                                       "tool-copied 1391110"]))
        self.assertEqual(self.run_with_stand_in(), (0, ["tool-have 1070560", "tool-have 1391110"]))

    def test_a_tool_the_child_has_is_never_touched(self):
        # Their Steam owns it and updates it, whatever its build or state.
        self.machine.add_tool(**SOLDIER, build=200)
        own = self.machine.library / "appmanifest_1391110.acf"
        own.write_bytes(manifest(1391110, "SteamLinuxRuntime_soldier", flags="6",
                                 extra='\t"buildid"\t\t"100"\n'))
        os.utime(own, (1_000_000_000, 1_000_000_000))
        kept = own.read_bytes()
        self.assertEqual(self.run_with_stand_in(), (0, ["tool-have 1391110"]))
        self.assertEqual(own.read_bytes(), kept)
        self.assertEqual(own.stat().st_mtime, 1_000_000_000)
        self.assertFalse((self.machine.library / "common" / "SteamLinuxRuntime_soldier").exists())

    def test_a_tool_folder_without_its_manifest_is_left_alone(self):
        self.machine.add_tool(**SOLDIER)
        (self.machine.library / "common" / "SteamLinuxRuntime_soldier").mkdir()
        self.assertEqual(self.run_with_stand_in(), (0, ["tool-skipped 1391110 folder-without-manifest"]))
        self.assertFalse(self.child_has("1391110"))

    def test_the_newest_build_is_copied(self):
        self.machine.add_tool(**SOLDIER, build=100)
        newest = self.machine.add_tool(**SOLDIER, build=250)
        self.machine.add_tool(**SOLDIER, build=99)
        self.assertEqual(self.run_with_stand_in(), (0, ["copying-tools", "tool-copied 1391110"]))
        self.assertEqual((self.machine.library / "appmanifest_1391110.acf").read_bytes(),
                         (newest / "appmanifest_1391110.acf").read_bytes())

    def test_anything_but_a_builds_folder_is_passed_over(self):
        self.machine.add_tool(**SOLDIER, build=100)
        tools = self.machine.tools
        # A newer build that is a link, a file, a half-made build and odd
        # names: none of them is a build.
        (tools / "1391110-900").symlink_to(tools / "1391110-100")
        (tools / "1391110-901").write_bytes(b"not a build")
        shutil.copytree(tools / "1391110-100", tools / ".cairn-tool-abc")
        shutil.copytree(tools / "1391110-100", tools / "1391110-0100")
        shutil.copytree(tools / "1391110-100", tools / "soldier")
        self.assertEqual(self.run_with_stand_in(), (0, ["copying-tools", "tool-copied 1391110"]))
        self.assertIn(b'"buildid"\t\t"100"', (self.machine.library / "appmanifest_1391110.acf").read_bytes())

    def test_a_game_put_in_the_tools_folder_is_never_copied(self):
        self.machine.add_tool("620", "Portal 2", "Portal 2")
        build = self.machine.add_tool("2805730", "Proton Shadow", "Proton 9.0")
        (build / "common" / "Proton Shadow" / "toolmanifest.vdf").unlink()
        self.assertEqual(self.run_with_stand_in(), (0, ["tool-skipped 620 not-a-tool",
                                                       "tool-skipped 2805730 not-a-tool"]))
        self.assertEqual(list((self.machine.library / "common").iterdir()), [])

    def test_a_manifest_that_names_another_appid(self):
        self.machine.add_tool(**SOLDIER, data=manifest(1070560, "SteamLinuxRuntime_soldier",
                                                       name="Steam Linux Runtime 2.0 (soldier)"))
        self.assertEqual(self.run_with_stand_in(), (0, ["tool-skipped 1391110 appid"]))

    def test_a_tool_that_runs_in_one_nobody_has(self):
        # #206: the child's Steam would fetch scout and never soldier.
        self.machine.add_tool(**SCOUT)
        self.assertEqual(self.run_with_stand_in(), (0, ["copying-tools", "tool-copied 1070560",
                                                       "tool-missing 1391110"]))
        # With the child's own soldier there is nothing missing.
        shutil.rmtree(self.machine.library / "common" / "SteamLinuxRuntime")
        (self.machine.library / "appmanifest_1070560.acf").unlink()
        (self.machine.library / "appmanifest_1391110.acf").write_bytes(manifest(1391110, "SteamLinuxRuntime_soldier"))
        self.assertEqual(self.run_with_stand_in(), (0, ["copying-tools", "tool-copied 1070560"]))

    def test_tools_the_child_has_still_count_for_what_is_missing(self):
        self.machine.add_tool(**PROTON)
        (self.machine.library / "appmanifest_1493710.acf").write_bytes(manifest(1493710, "Proton - Experimental"))
        self.assertEqual(self.run_with_stand_in(), (0, ["tool-have 1493710", "tool-missing 4183110"]))

    def test_an_empty_tools_folder(self):
        self.machine.tools.mkdir(parents=True)
        self.machine.add_game(100)
        self.assertEqual(self.run_with_stand_in(), (0, ["copied 100", "no-tools"]))

    def test_no_tools_folder_says_nothing_about_tools(self):
        self.machine.add_game(100)
        self.assertEqual(self.run_with_stand_in(), (0, ["copied 100"]))
        self.machine.tools.parent.mkdir(parents=True, exist_ok=True)
        self.machine.tools.symlink_to(self.machine.root)
        self.assertEqual(self.run_with_stand_in(), (0, ["have 100"]))

    def test_an_early_stop_says_nothing_about_tools(self):
        self.machine.add_tool(**SOLDIER)
        self.machine.add_game(100)
        vdf = self.machine.steam / "config" / "libraryfolders.vdf"
        vdf.parent.mkdir(parents=True)
        vdf.write_text(f'"libraryfolders"\n{{\n\t"1"\n\t{{\n\t\t"path"\t\t"{self.machine.shared.parent}"\n\t}}\n}}\n')
        self.assertEqual(self.run_with_stand_in(), (0, ["lists-shared"]))
        vdf.unlink()
        os.mkfifo(vdf)
        self.assertEqual(self.machine.run(), ["unreadable-config"])
        vdf.unlink()
        shutil.rmtree(self.machine.shared)
        self.assertEqual(self.machine.run(), ["no-shared"])
        shutil.rmtree(self.machine.steam)
        self.assertEqual(self.machine.run(), ["no-library"])

    def test_no_reflink_found_only_at_the_tools_holds_no_game(self):
        self.machine.add_game(100)
        (self.machine.library / "appmanifest_100.acf").write_bytes(manifest(100))
        self.machine.add_tool(**SCOUT)
        self.machine.add_tool(**SOLDIER)
        code, lines = self.run_with_stand_in(can_reflink=lambda manifest, common: False)
        self.assertEqual((code, lines), (0, ["have 100", "tool-skipped 1070560 no-reflink",
                                             "tool-skipped 1391110 no-reflink"]))
        self.assertNotIn("no-reflink", lines)

    def test_tools_come_with_a_list_and_without_one(self):
        self.machine.add_game(100, "First")
        self.machine.add_game(200, "Second")
        self.machine.add_tool(**SOLDIER)
        self.assertEqual(self.run_with_stand_in("--only", "200"), (0, ["copied 200", "copying-tools",
                                                                       "tool-copied 1391110"]))
        self.assertEqual(self.run_with_stand_in(), (0, ["copied 100", "have 200", "tool-have 1391110"]))

    @needs_reflink
    @needs_not_root
    def test_a_tools_folder_the_child_cannot_read_leaves_the_games_as_they_are(self):
        self.machine.add_game(100)
        self.machine.add_tool(**SOLDIER)
        self.machine.tools.chmod(0o000)
        try:
            lines = self.machine.run()
        finally:
            self.machine.tools.chmod(0o755)
        self.assertEqual((self.machine.code, lines), (0, ["copied 100", "tools-unreadable"]))

    @needs_reflink
    def test_a_fifo_for_a_manifest_never_holds_the_copy_up(self):
        self.machine.add_game(100)
        build = self.machine.add_tool(**SOLDIER)
        (build / "appmanifest_1391110.acf").unlink()
        os.mkfifo(build / "appmanifest_1391110.acf")
        module = self.machine.load()
        with mock.patch.object(module, "reflink_copy", CopyTest.copy_tree):
            code, lines = self.machine.run_in_process(module)
        self.assertEqual((code, lines), (0, ["copied 100", "tool-skipped 1391110 unreadable"]))
        # And as its own program, which would block for ever otherwise.
        (self.machine.library / "appmanifest_100.acf").unlink()
        shutil.rmtree(self.machine.library / "common" / "A Game")
        lines = self.machine.run()
        self.assertEqual(lines[-1], "tool-skipped 1391110 unreadable", self.machine.stderr)

    def test_a_game_manifest_that_is_a_fifo_is_unreadable_too(self):
        # The same read for a game, whose folder every Guardian can write.
        os.mkfifo(self.machine.shared / "appmanifest_100.acf")
        self.assertEqual(self.machine.run(), ["skipped 100 unreadable"])


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
