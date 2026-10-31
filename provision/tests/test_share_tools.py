#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Runs cairn-share-tools.py, which fills the tools folder with Steam's
tools from a Guardian's Steam (ADR-0046), on folders made for each test.

Standard library only. The program is installed into a temporary folder
beside kidscan.py, as the image installs it, and run there with `python3 -I
-B`, or loaded from there when a test needs to change one of its parts. It
always gets --home and --tools, so it never reads this account's own
Steam or the computer's tools folder.

A real copy needs a filesystem that can reflink (btrfs or XFS), found as
test_copy_games finds one; only the tests that need one skip, saying so,
when there is none. Most tests copy with a stand-in for cp instead, and
"other-drive" runs on /dev/shm, which is tmpfs.
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

from test_copy_games import NO_REFLINK, REFLINK_PLACE, needs_not_root, needs_reflink, reflinks

HERE = pathlib.Path(__file__).resolve().parent
PROGRAM = HERE.parent / "cairn-share-tools.py"
COPY_PROGRAM = HERE.parent / "cairn-copy-games.py"
KIDSCAN = HERE.parents[1] / "tools" / "kidscan" / "kidscan.py"

# Every line the program may print (its docstring).
GRAMMAR = re.compile(
    r"tool-(shared|had) [0-9]{1,10}|tool-(kept|not-shared) [0-9]{1,10} [a-z-]{1,32}"
    r"|no-steam|refused root|refused folder|finished|failed"
)

# The four tools of the test VM, as (appid, name, installdir, the tool it
# runs in).
SCOUT = ("1070560", "Steam Linux Runtime 1.0 (scout)", "SteamLinuxRuntime", "1391110")
SOLDIER = ("1391110", "Steam Linux Runtime 2.0 (soldier)", "SteamLinuxRuntime_soldier", None)
PROTON = ("1493710", "Proton - Experimental", "Proton - Experimental", "4183110")
SLR4 = ("4183110", "Steam Linux Runtime 4.0", "SteamLinuxRuntime_4", None)


def manifest(appid, name, installdir, build="100", flags="4", extra=""):
    build_line = f'\t"buildid"\t\t"{build}"\n' if build is not None else ""
    return (
        '"AppState"\n{\n'
        f'\t"appid"\t\t"{appid}"\n'
        f'\t"name"\t\t"{name}"\n'
        f'\t"StateFlags"\t\t"{flags}"\n'
        f'\t"installdir"\t\t"{installdir}"\n'
        f"{build_line}"
        '\t"LastOwner"\t\t"76561198000000000"\n'
        f"{extra}"
        "}\n"
    ).encode()


def tool_manifest(runs_in=None):
    needs = f'\t"require_tool_appid" "{runs_in}"\n' if runs_in else ""
    return f'"manifest"\n{{\n\t"version" "2"\n\t"commandline" "/_v2-entry-point --"\n{needs}}}\n'.encode()


def copy_tree(sources, target):
    """A stand-in for cp that needs no reflink, which copies what cp would:
    into a folder that is there, or to a new name, links as links."""
    sources = [pathlib.Path(source) for source in sources]
    target = pathlib.Path(target)
    places = [target / source.name for source in sources] if target.is_dir() else [target]
    for source, place in zip(sources, places):
        if source.is_dir() and not source.is_symlink():
            shutil.copytree(source, place, symlinks=True)
        else:
            shutil.copy2(source, place, follow_symlinks=False)
    return True


def tree(folder):
    """Every path under a folder with its kind, size, mode and change time,
    to tell whether anything in it changed."""
    seen = {}
    for path in sorted(pathlib.Path(folder).rglob("*")):
        details = path.lstat()
        seen[str(path)] = (details.st_mode, details.st_size, details.st_mtime_ns)
    return seen


class Machine:
    """A Guardian's home with their Steam, a tools folder, and the program
    installed beside kidscan.py, all under one folder."""

    def __init__(self, root):
        self.root = pathlib.Path(root)
        self.libexec = self.root / "libexec"
        self.libexec.mkdir()
        shutil.copy(PROGRAM, self.libexec / "cairn-share-tools")
        shutil.copy(KIDSCAN, self.libexec / "kidscan.py")
        self.home = self.root / "home" / "gina"
        self.steam = self.home / ".local" / "share" / "Steam"
        self.library = self.steam / "steamapps"
        (self.library / "common").mkdir(parents=True)
        self.tools = self.root / "var" / "steam-tools"
        self.tools.mkdir(parents=True)

    def add_tool(self, tool, build="100", flags="4", extra="", library=None, data=None):
        """A tool in one of the Guardian's libraries: its manifest, and a
        folder with its toolmanifest.vdf, a program, a library file, and a
        var/ with a private folder, as pressure-vessel leaves one."""
        appid, name, installdir, runs_in = tool
        library = library or self.library
        (library / f"appmanifest_{appid}.acf").write_bytes(
            data or manifest(appid, name, installdir, build, flags, extra))
        folder = library / "common" / installdir
        for path, content, mode in (
            ("toolmanifest.vdf", tool_manifest(runs_in), 0o644),
            ("_v2-entry-point", b"#!/bin/sh\nexec \"$@\"\n", 0o755),
            ("files/lib/libc.so.6", b"lib" * 1000, 0o644),
            ("var/tmp-1234/private/secret", b"another account's", 0o600),
        ):
            file = folder / path
            file.parent.mkdir(parents=True, exist_ok=True)
            file.write_bytes(content)
            file.chmod(mode)
        (folder / "var" / "tmp-1234" / "private").chmod(0o700)
        return folder

    def manifest_of(self, tool, library=None):
        return (library or self.library) / f"appmanifest_{tool[0]}.acf"

    def builds(self):
        return sorted(path.name for path in self.tools.iterdir())

    def command(self, *arguments):
        return [sys.executable, "-I", "-B", str(self.libexec / "cairn-share-tools"),
                "--home", str(self.home), "--tools", str(self.tools), *arguments]

    def run(self, *arguments):
        # A run that blocks fails the test rather than hanging it.
        done = subprocess.run(self.command(*arguments), stdin=subprocess.DEVNULL, capture_output=True,
                              text=True, check=False, timeout=60)
        self.code = done.returncode
        self.stderr = done.stderr
        return done.stdout.splitlines()

    def load(self):
        """The installed program as a module, to change one of its parts."""
        loader = importlib.machinery.SourceFileLoader(
            "cairn_share_tools", str(self.libexec / "cairn-share-tools"))
        spec = importlib.util.spec_from_loader(loader.name, loader)
        module = importlib.util.module_from_spec(spec)
        loader.exec_module(module)
        return module

    def run_in_process(self, module, *arguments, home=True):
        given = ["--home", str(self.home)] if home else []
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = module.main([*given, "--tools", str(self.tools), *arguments])
        return code, out.getvalue().splitlines()

    def run_with_stand_in(self, *arguments):
        """In this process, with a stand-in for cp, so no reflink is needed."""
        module = self.load()
        with mock.patch.object(module, "reflink_copy", copy_tree):
            return self.run_in_process(module, *arguments)


class ShareTestCase(unittest.TestCase):
    place = None

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(dir=self.place)
        self.machine = Machine(self.directory.name)

    def tearDown(self):
        # Folders the tests made private, so the folder can be removed.
        for path in pathlib.Path(self.directory.name).rglob("*"):
            if path.is_dir() and not path.is_symlink():
                path.chmod(0o755)
        self.directory.cleanup()

    def assertOnlyGrammar(self, lines):
        for line in lines:
            self.assertRegex(line, f"^(?:{GRAMMAR.pattern})$")
            self.assertNotIn("/", line)

    def assertNoScratch(self):
        self.assertEqual([name for name in self.machine.builds() if name.startswith(".cairn-")], [])


class WhichAppsTest(ShareTestCase):
    """Only Steam's tools, by kidscan's rule and their toolmanifest.vdf;
    everything else is passed over without a word."""

    def test_games_and_what_cannot_be_read_are_passed_over(self):
        library = self.machine.library
        (library / "appmanifest_620.acf").write_bytes(manifest("620", "Portal 2", "Portal 2"))
        (library / "common" / "Portal 2").mkdir()
        (library / "appmanifest_700.acf").write_bytes(b'"AppState"\n{\n\t"appid" "700"\n')
        (library / "appmanifest_701.acf").write_bytes(b"\xff\xfe" + manifest("701", "Proton 9.0", "P"))
        (library / "appmanifest_1070560.acf").write_bytes(manifest("1070561", *SCOUT[1:3]))
        (library / "appmanifest_x.acf").write_bytes(manifest("1391110", *SOLDIER[1:3]))
        # Steamworks has no toolmanifest.vdf, and Legacy Steam Runtime is
        # not in kidscan's rule.
        steamworks = library / "common" / "Steamworks Shared"
        steamworks.mkdir()
        (library / "appmanifest_228980.acf").write_bytes(
            manifest("228980", "Steamworks Common Redistributables", "Steamworks Shared"))
        legacy = library / "common" / "SteamLinuxRuntime_legacy"
        legacy.mkdir()
        (legacy / "toolmanifest.vdf").write_bytes(tool_manifest())
        (library / "appmanifest_4690330.acf").write_bytes(
            manifest("4690330", "Legacy Steam Runtime", "SteamLinuxRuntime_legacy"))
        code, lines = self.machine.run_with_stand_in()
        self.assertEqual((code, lines), (0, ["finished"]))
        self.assertEqual(self.machine.builds(), [])

    def test_a_tool_by_its_name_or_its_appid(self):
        self.machine.add_tool(("2805730", "Proton 9.0", "Proton 9.0", None))
        self.machine.add_tool(("1628350", "Some Runtime", "SteamLinuxRuntime_sniper", None))
        self.machine.add_tool(("3000001", "Steam Linux Runtime 5.0", "SteamLinuxRuntime_5", None))
        code, lines = self.machine.run_with_stand_in()
        self.assertEqual((code, lines), (0, ["tool-shared 1628350", "tool-shared 2805730",
                                             "tool-shared 3000001", "finished"]))

    def test_a_toolmanifest_does_not_make_a_game_a_tool(self):
        self.machine.add_tool(("620", "Portal 2", "Portal 2", None))
        self.assertEqual(self.machine.run_with_stand_in(), (0, ["finished"]))
        self.assertEqual(self.machine.builds(), [])

    def test_an_installdir_that_is_not_one_plain_folder_name(self):
        # Even with a toolmanifest.vdf where it leads: such a build would
        # hold the whole steamapps, or the whole common, in a folder every
        # account reads.
        library = self.machine.library
        for installdir, where in (("..", library), ("a/b", library / "common" / "a" / "b"),
                                  ("", library / "common")):
            with self.subTest(installdir=installdir):
                where.mkdir(parents=True, exist_ok=True)
                (where / "toolmanifest.vdf").write_bytes(tool_manifest())
                self.machine.manifest_of(SCOUT).write_bytes(manifest(SCOUT[0], SCOUT[1], installdir))
                try:
                    self.assertEqual(self.machine.run_with_stand_in(), (0, ["finished"]))
                    self.assertEqual(self.machine.builds(), [])
                finally:
                    (where / "toolmanifest.vdf").unlink()

    def test_a_folder_that_is_a_link_or_a_toolmanifest_that_is_not_a_file(self):
        elsewhere = self.machine.root / "elsewhere"
        elsewhere.mkdir()
        (elsewhere / "toolmanifest.vdf").write_bytes(tool_manifest())
        (self.machine.library / "common" / SCOUT[2]).symlink_to(elsewhere)
        self.machine.manifest_of(SCOUT).write_bytes(manifest(*SCOUT[:3]))
        folder = self.machine.add_tool(SOLDIER)
        (folder / "toolmanifest.vdf").unlink()
        (folder / "toolmanifest.vdf").mkdir()
        self.assertEqual(self.machine.run_with_stand_in(), (0, ["finished"]))


class ShareTest(ShareTestCase):
    """Real copies, by reflink."""

    place = REFLINK_PLACE

    @needs_reflink
    def test_a_tool_goes_in_whole_without_its_var(self):
        folder = self.machine.add_tool(SCOUT, build="26115262")
        os.utime(folder / "_v2-entry-point", (1_000_000_000, 1_000_000_000))
        lines = self.machine.run()
        self.assertEqual(lines, ["tool-shared 1070560", "finished"], self.machine.stderr)
        self.assertEqual(self.machine.code, 0)
        self.assertEqual(self.machine.builds(), ["1070560-26115262"])
        build = self.machine.tools / "1070560-26115262"
        self.assertEqual(sorted(path.name for path in build.iterdir()), ["appmanifest_1070560.acf", "common"])
        self.assertEqual((build / "appmanifest_1070560.acf").read_bytes(),
                         self.machine.manifest_of(SCOUT).read_bytes())
        copy = build / "common" / "SteamLinuxRuntime"
        self.assertEqual(sorted(path.name for path in copy.iterdir()), ["_v2-entry-point", "files", "toolmanifest.vdf"])
        self.assertEqual((copy / "files" / "lib" / "libc.so.6").read_bytes(), b"lib" * 1000)
        self.assertEqual((copy / "_v2-entry-point").stat().st_mode & 0o777, 0o755, "it still runs")
        self.assertEqual((copy / "toolmanifest.vdf").stat().st_mode & 0o777, 0o644)
        self.assertEqual((copy / "_v2-entry-point").stat().st_mtime, 1_000_000_000)
        self.assertEqual((build / "appmanifest_1070560.acf").stat().st_mode & 0o777, 0o644)

    @needs_reflink
    def test_the_four_tools_of_the_vm(self):
        for tool in (PROTON, SCOUT, SLR4, SOLDIER):
            self.machine.add_tool(tool)
        lines = self.machine.run()
        self.assertEqual(lines, ["tool-shared 1070560", "tool-shared 1391110", "tool-shared 1493710",
                                 "tool-shared 4183110", "finished"], self.machine.stderr)
        self.assertEqual(self.machine.builds(), ["1070560-100", "1391110-100", "1493710-100", "4183110-100"])
        self.assertOnlyGrammar(lines)

    @needs_reflink
    def test_the_copy_step_takes_what_this_makes(self):
        # The two programs agree on the folder's layout: a child's copy step
        # finds the build and copies it as the child.
        self.machine.add_tool(SCOUT)
        self.assertEqual(self.machine.run(), ["tool-shared 1070560", "finished"])
        shared = self.machine.root / "steam-library" / "steamapps"
        (shared / "common").mkdir(parents=True)
        child = self.machine.root / "home" / "ada" / ".local" / "share" / "Steam" / "steamapps"
        (child / "common").mkdir(parents=True)
        shutil.copy(COPY_PROGRAM, self.machine.libexec / "cairn-copy-games")
        done = subprocess.run(
            [sys.executable, "-I", "-B", str(self.machine.libexec / "cairn-copy-games"), "--shared", str(shared),
             "--library", str(child), "--tools", str(self.machine.tools)],
            stdin=subprocess.DEVNULL, capture_output=True, text=True, check=False, timeout=60)
        self.assertEqual(done.stdout.splitlines(), ["copying-tools", "tool-copied 1070560", "tool-missing 1391110"],
                         done.stderr)
        self.assertEqual((child / "appmanifest_1070560.acf").read_bytes(),
                         self.machine.manifest_of(SCOUT).read_bytes())
        self.assertTrue((child / "common" / "SteamLinuxRuntime" / "toolmanifest.vdf").is_file())
        self.assertFalse((child / "common" / "SteamLinuxRuntime" / "var").exists())


class AgainTest(ShareTestCase):
    """A build only goes forward, and the folder keeps one build of each
    tool. Copies use a stand-in for cp."""

    def test_a_second_run_has_it_and_changes_nothing(self):
        self.machine.add_tool(SCOUT)
        self.assertEqual(self.machine.run_with_stand_in(), (0, ["tool-shared 1070560", "finished"]))
        build = self.machine.tools / "1070560-100"
        before = build.stat().st_ino
        # Nothing is copied again, not even the probe.
        copies = []

        def recording_copy(sources, target):
            copies.append(target)
            return copy_tree(sources, target)

        module = self.machine.load()
        with mock.patch.object(module, "reflink_copy", recording_copy):
            self.assertEqual(self.machine.run_in_process(module), (0, ["tool-had 1070560", "finished"]))
        self.assertEqual(copies, [])
        self.assertEqual(build.stat().st_ino, before)
        self.assertEqual(self.machine.builds(), ["1070560-100"])

    def test_a_build_of_that_name_that_came_meanwhile_is_had(self):
        # Another Guardian's run put the same build in place first, so the
        # rename finds a folder that holds something.
        self.machine.add_tool(SCOUT)
        module = self.machine.load()
        real_rename = os.rename

        def rename(source, target):
            if pathlib.Path(target).name == "1070560-100":
                (self.machine.tools / "1070560-100" / "common").mkdir(parents=True)
            real_rename(source, target)

        with (mock.patch.object(module, "reflink_copy", copy_tree),
              mock.patch.object(module.os, "rename", rename)):
            code, lines = self.machine.run_in_process(module)
        self.assertEqual((code, lines), (0, ["tool-had 1070560", "finished"]))
        self.assertEqual(self.machine.builds(), ["1070560-100"])
        self.assertNoScratch()

    def test_a_newer_build_takes_the_older_ones_place(self):
        self.machine.add_tool(SCOUT, build="100")
        self.machine.run_with_stand_in()
        self.machine.manifest_of(SCOUT).write_bytes(manifest(*SCOUT[:3], build="200"))
        self.assertEqual(self.machine.run_with_stand_in(), (0, ["tool-shared 1070560", "finished"]))
        self.assertEqual(self.machine.builds(), ["1070560-200"])

    def test_a_newer_build_in_the_folder_is_never_replaced_by_an_older_one(self):
        # Another Guardian's Steam had the newer one.
        self.machine.add_tool(SCOUT, build="300")
        self.machine.run_with_stand_in()
        self.machine.manifest_of(SCOUT).write_bytes(manifest(*SCOUT[:3], build="200"))
        self.assertEqual(self.machine.run_with_stand_in(), (0, ["tool-had 1070560", "finished"]))
        self.assertEqual(self.machine.builds(), ["1070560-300"])

    def test_tool_had_also_clears_older_builds(self):
        self.machine.add_tool(SCOUT, build="100")
        self.machine.run_with_stand_in()
        self.machine.manifest_of(SCOUT).write_bytes(manifest(*SCOUT[:3], build="300"))
        self.machine.run_with_stand_in()
        # An older build that came back, by hand or from an older Cairn.
        shutil.copytree(self.machine.tools / "1070560-300", self.machine.tools / "1070560-50")
        self.assertEqual(self.machine.run_with_stand_in(), (0, ["tool-had 1070560", "finished"]))
        self.assertEqual(self.machine.builds(), ["1070560-300"])

    def test_leftovers_go_and_nothing_else(self):
        tools = self.machine.tools
        (tools / ".cairn-tool-abc" / "1070560-100" / "common").mkdir(parents=True)
        (tools / ".cairn-tool-gone-xyz").mkdir()
        (tools / ".cairn-tool-probe").write_bytes(b"half")
        (tools / "1070560-50").symlink_to(self.machine.home)
        (tools / "1391110-60").write_bytes(b"not a build")
        kept = [tools / "notes.txt", tools / ".cairn-other"]
        for path in kept:
            path.write_bytes(b"keep")
        (tools / "a-folder").mkdir()
        self.assertEqual(self.machine.run_with_stand_in(), (0, ["finished"]))
        self.assertEqual(self.machine.builds(), [".cairn-other", "a-folder", "notes.txt"])
        self.assertTrue(self.machine.home.is_dir(), "a link is removed, never what it leads to")


class ReasonsTest(ShareTestCase):
    """Each reason a tool stays out, with no build in the folder before
    (tool-not-shared) and with an older one (tool-kept), which stays."""

    def left_out(self, reason, change, module_patches=()):
        for older in (False, True):
            with self.subTest(older=older):
                shutil.rmtree(self.machine.tools)
                self.machine.tools.mkdir()
                if older:
                    (self.machine.tools / "1070560-50" / "common").mkdir(parents=True)
                self.machine.add_tool(SCOUT)
                change()
                module = self.machine.load()
                with contextlib.ExitStack() as stack:
                    stack.enter_context(mock.patch.object(module, "reflink_copy", copy_tree))
                    for name, value in module_patches:
                        stack.enter_context(mock.patch.object(module, name, value))
                    code, lines = self.machine.run_in_process(module)
                word = "tool-kept" if older else "tool-not-shared"
                self.assertEqual((code, lines), (0, [f"{word} 1070560 {reason}", "finished"]))
                self.assertEqual(self.machine.builds(), ["1070560-50"] if older else [])
                self.assertOnlyGrammar(lines)

    def write(self, **fields):
        return lambda: self.machine.manifest_of(SCOUT).write_bytes(manifest(*SCOUT[:3], **fields))

    def test_not_fully_installed(self):
        for flags in ("6", "1026", "0"):
            with self.subTest(flags=flags):
                self.left_out("not-installed", self.write(flags=flags))

    def test_a_beta_branch(self):
        extra = '\t"UserConfig"\n\t{\n\t\t"BetaKey"\t\t"previous_release"\n\t}\n'
        self.left_out("beta-branch", self.write(extra=extra))

    def test_something_downloading(self):
        self.left_out("downloading",
                      lambda: (self.machine.library / "downloading" / "1070560").mkdir(parents=True, exist_ok=True))

    def test_no_build(self):
        for build in (None, "abc", "0123", "", "12345678901"):
            with self.subTest(build=build):
                self.left_out("no-build", self.write(build=build))

    def test_a_file_or_a_folder_only_the_guardian_can_read(self):
        folder = self.machine.library / "common" / "SteamLinuxRuntime"
        self.left_out("unreadable-files", lambda: (folder / "files" / "lib" / "libc.so.6").chmod(0o600))
        self.left_out("unreadable-files", lambda: (folder / "files").chmod(0o700))

    def test_another_drive(self):
        self.left_out("other-drive", lambda: None, [("can_reflink", lambda manifest, scratch: False)])

    def test_a_copy_that_fails(self):
        def failing(sources, target):
            return pathlib.Path(target).name == "probe" and copy_tree(sources, target)

        self.left_out("copy-failed", lambda: None, [("reflink_copy", failing)])

    def test_a_manifest_that_changed_during_the_copy(self):
        def copy_while_steam_updates(sources, target):
            copy_tree(sources, target)
            if pathlib.Path(target).name != "probe":
                self.write(flags="6")()
            return True

        self.left_out("changed", lambda: None, [("reflink_copy", copy_while_steam_updates)])


class LibrariesTest(ShareTestCase):
    """Every library of the Guardian's Steam, each once, from its two lists."""

    def second_library(self, name="second"):
        steamapps = self.machine.root / "drives" / name / "steamapps"
        (steamapps / "common").mkdir(parents=True)
        return steamapps

    def list_libraries(self, list_name, *paths):
        entries = "".join(f'\t"{i}"\n\t{{\n\t\t"path"\t\t"{path}"\n\t}}\n' for i, path in enumerate(paths))
        vdf = self.machine.steam / list_name
        vdf.parent.mkdir(parents=True, exist_ok=True)
        vdf.write_text(f'"libraryfolders"\n{{\n{entries}}}\n')
        return vdf

    def test_a_library_from_either_list(self):
        for list_name in ("steamapps/libraryfolders.vdf", "config/libraryfolders.vdf"):
            with self.subTest(list_name=list_name):
                second = self.second_library(list_name.split("/")[0])
                self.machine.add_tool(SOLDIER, library=second)
                vdf = self.list_libraries(list_name, self.machine.steam, second.parent)
                code, lines = self.machine.run_with_stand_in()
                vdf.unlink()
                self.assertEqual((code, lines), (0, ["tool-shared 1391110", "finished"]))
                shutil.rmtree(self.machine.tools / "1391110-100")

    def test_an_old_list_of_plain_paths(self):
        second = self.second_library()
        self.machine.add_tool(SOLDIER, library=second)
        (self.machine.library / "libraryfolders.vdf").write_text(
            f'"LibraryFolders"\n{{\n\t"TimeNextStatsReport"\t"0"\n\t"1"\t"{second.parent}"\n}}\n')
        self.assertEqual(self.machine.run_with_stand_in(), (0, ["tool-shared 1391110", "finished"]))

    def test_a_relative_path_is_not_followed(self):
        second = self.second_library()
        self.machine.add_tool(SOLDIER, library=second)
        self.list_libraries("steamapps/libraryfolders.vdf", os.path.relpath(second.parent))
        self.assertEqual(self.machine.run_with_stand_in(), (0, ["finished"]))

    def test_a_list_that_is_a_link_or_a_fifo_is_not_read(self):
        second = self.second_library()
        self.machine.add_tool(SOLDIER, library=second)
        real = self.machine.root / "real.vdf"
        real.write_text(f'"libraryfolders"\n{{\n\t"1"\n\t{{\n\t\t"path"\t\t"{second.parent}"\n\t}}\n}}\n')
        vdf = self.machine.library / "libraryfolders.vdf"
        for make in (lambda: vdf.symlink_to(real), lambda: os.mkfifo(vdf)):
            make()
            lines = self.machine.run()
            vdf.unlink()
            self.assertEqual(lines, ["finished"], self.machine.stderr)

    def test_a_steamapps_or_common_that_is_a_link_is_passed_over(self):
        second = self.second_library()
        self.machine.add_tool(SOLDIER, library=second)
        linked = self.machine.root / "drives" / "linked"
        linked.mkdir()
        (linked / "steamapps").symlink_to(second)
        third = self.machine.root / "drives" / "third"
        (third / "steamapps").mkdir(parents=True)
        (third / "steamapps" / "common").symlink_to(second / "common")
        shutil.copy(second / "appmanifest_1391110.acf", third / "steamapps")
        self.list_libraries("steamapps/libraryfolders.vdf", linked, third)
        self.assertEqual(self.machine.run_with_stand_in(), (0, ["finished"]))

    def test_one_tool_in_two_libraries_comes_once_from_the_first(self):
        second = self.second_library()
        self.machine.add_tool(SCOUT, build="100")
        self.machine.add_tool(SCOUT, build="200", library=second)
        self.list_libraries("steamapps/libraryfolders.vdf", self.machine.steam, second.parent)
        self.assertEqual(self.machine.run_with_stand_in(), (0, ["tool-shared 1070560", "finished"]))
        self.assertEqual(self.machine.builds(), ["1070560-100"])

    def test_the_guardians_own_library_listed_again_is_read_once(self):
        self.machine.add_tool(SCOUT)
        self.list_libraries("config/libraryfolders.vdf", self.machine.steam, str(self.machine.steam) + "/")
        self.list_libraries("steamapps/libraryfolders.vdf", self.machine.steam)
        self.assertEqual(self.machine.run_with_stand_in(), (0, ["tool-shared 1070560", "finished"]))


class ConditionsTest(ShareTestCase):
    def test_no_library_at_all(self):
        shutil.rmtree(self.machine.steam)
        self.assertEqual(self.machine.run(), ["no-steam", "finished"])
        self.assertEqual(self.machine.code, 0)

    def test_refuses_to_run_as_root(self):
        self.machine.add_tool(SCOUT)
        module = self.machine.load()
        with mock.patch.object(module.os, "geteuid", return_value=0):
            code, lines = self.machine.run_in_process(module)
        self.assertEqual((code, lines), (1, ["refused root"]))
        self.assertEqual(self.machine.builds(), [])

    def test_a_tools_folder_it_may_not_change(self):
        self.machine.add_tool(SCOUT)
        real = self.machine.tools
        linked = self.machine.root / "linked-tools"
        linked.symlink_to(real)
        for tools in (linked, self.machine.root / "missing"):
            with self.subTest(tools=tools.name):
                self.machine.tools = tools
                self.assertEqual(self.machine.run(), ["refused folder"])
                self.assertEqual(self.machine.code, 1)
        self.machine.tools = real
        self.assertEqual(sorted(real.iterdir()), [])

    @needs_not_root
    def test_a_tools_folder_this_account_cannot_write(self):
        # A grown-up in wheel but not in cairn-guardian.
        self.machine.add_tool(SCOUT)
        self.machine.tools.chmod(0o555)
        self.assertEqual(self.machine.run(), ["refused folder"])
        self.assertEqual(self.machine.code, 1)

    def test_anything_unexpected_says_only_failed(self):
        self.machine.add_tool(SCOUT)
        module = self.machine.load()
        with mock.patch.object(module, "libraries", side_effect=RuntimeError(str(self.machine.home))):
            code, lines = self.machine.run_in_process(module)
        self.assertEqual((code, lines), (1, ["failed"]))

    def test_the_home_comes_from_the_password_database(self):
        self.machine.add_tool(SCOUT)
        module = self.machine.load()
        account = mock.Mock(pw_dir=str(self.machine.home))
        with (mock.patch.object(module.pwd, "getpwuid", return_value=account),
              mock.patch.dict(os.environ, {"HOME": str(self.machine.root / "elsewhere")}),
              mock.patch.object(module, "reflink_copy", copy_tree)):
            code, lines = self.machine.run_in_process(module, home=False)
        self.assertEqual((code, lines), (0, ["tool-shared 1070560", "finished"]))

    def test_nothing_is_written_in_the_home(self):
        for tool in (SCOUT, SOLDIER, PROTON):
            self.machine.add_tool(tool)
        (self.machine.library / "downloading" / "1493710").mkdir(parents=True)
        before = tree(self.machine.home)
        self.machine.run_with_stand_in()
        self.machine.run_with_stand_in()
        self.assertEqual(tree(self.machine.home), before)

    def test_a_build_that_cannot_be_renamed_into_place_is_made_at_the_next_run(self):
        self.machine.add_tool(SCOUT)
        module = self.machine.load()
        real_rename = os.rename

        def rename(source, target):
            if pathlib.Path(target).name == "1070560-100":
                raise OSError("disk full")
            real_rename(source, target)

        with (mock.patch.object(module, "reflink_copy", copy_tree),
              mock.patch.object(module.os, "rename", rename)):
            code, lines = self.machine.run_in_process(module)
        self.assertEqual((code, lines), (0, ["tool-not-shared 1070560 copy-failed", "finished"]))
        self.assertEqual(self.machine.builds(), [])
        self.assertEqual(self.machine.run_with_stand_in(), (0, ["tool-shared 1070560", "finished"]))

    def test_nothing_it_prints_is_a_path_or_a_name(self):
        self.machine.add_tool(SCOUT)
        self.machine.add_tool(SOLDIER, flags="6")
        self.machine.add_tool(("620", "Pajama Sam", "Pajama Sam", None))
        code, lines = self.machine.run_with_stand_in()
        self.assertEqual(lines, ["tool-shared 1070560", "tool-not-shared 1391110 not-installed", "finished"])
        self.assertOnlyGrammar(lines)
        for line in lines:
            for word in ("Pajama", "Runtime", "gina", str(self.machine.root)):
                self.assertNotIn(word, line)


class PlantedLinksTest(ShareTestCase):
    """Any account in cairn-guardian can make entries in the tools folder.
    A link under a name the program uses never sends a write into a home."""

    def setUp(self):
        super().setUp()
        self.target = self.machine.home / "notes"
        self.target.mkdir()
        self.bashrc = self.machine.home / ".bashrc"
        self.bashrc.write_bytes(b"the Guardian's own\n")
        self.machine.add_tool(SCOUT)

    def assertHomeUntouched(self):
        self.assertEqual(self.bashrc.read_bytes(), b"the Guardian's own\n")
        self.assertEqual(list(self.target.iterdir()), [])

    def run_without_cleanup(self):
        # As if the link came just after the run removed what it found.
        module = self.machine.load()
        with (mock.patch.object(module, "reflink_copy", copy_tree),
              mock.patch.object(module, "remove_leftovers", lambda tools: None)):
            return self.machine.run_in_process(module)

    def test_a_link_at_the_old_probe_name(self):
        (self.machine.tools / ".cairn-tool-probe").symlink_to(self.bashrc)
        self.assertEqual(self.run_without_cleanup(), (0, ["tool-shared 1070560", "finished"]))
        self.assertHomeUntouched()

    def test_a_link_at_a_temporary_name(self):
        (self.machine.tools / ".cairn-tool-1070560").symlink_to(self.target)
        self.assertEqual(self.run_without_cleanup(), (0, ["tool-shared 1070560", "finished"]))
        self.assertHomeUntouched()

    def test_a_link_at_a_builds_name(self):
        (self.machine.tools / "1070560-100").symlink_to(self.target)
        # Removed before anything else, so the build goes in.
        self.assertEqual(self.machine.run_with_stand_in(), (0, ["tool-shared 1070560", "finished"]))
        self.assertTrue((self.machine.tools / "1070560-100").is_dir())
        self.assertFalse((self.machine.tools / "1070560-100").is_symlink())
        self.assertHomeUntouched()
        # Planted again after that: never written through, never a build.
        shutil.rmtree(self.machine.tools / "1070560-100")
        (self.machine.tools / "1070560-100").symlink_to(self.target)
        self.assertEqual(self.run_without_cleanup(), (0, ["tool-not-shared 1070560 copy-failed", "finished"]))
        self.assertHomeUntouched()
        self.assertNoScratch()

    def test_a_link_at_an_older_builds_name_is_never_followed_when_it_goes(self):
        (self.machine.tools / "1070560-50").symlink_to(self.target)
        (self.target / "keep").write_bytes(b"mine")
        self.assertEqual(self.run_without_cleanup(), (0, ["tool-shared 1070560", "finished"]))
        self.assertEqual((self.target / "keep").read_bytes(), b"mine")
        self.assertEqual(self.machine.builds(), ["1070560-100"])


class NoReflinkTest(unittest.TestCase):
    """On tmpfs, which cannot reflink, nothing goes into the tools folder."""

    @unittest.skipUnless(pathlib.Path("/dev/shm").is_dir(), "no /dev/shm")
    def test_other_drive_without_reflink(self):
        with tempfile.TemporaryDirectory(dir="/dev/shm") as root:
            if reflinks(root):
                self.skipTest("/dev/shm can reflink here")
            machine = Machine(root)
            machine.add_tool(SCOUT)
            self.assertEqual(machine.run(), ["tool-not-shared 1070560 other-drive", "finished"])
            self.assertEqual(machine.code, 0)
            self.assertEqual(machine.builds(), [])


if __name__ == "__main__":
    if REFLINK_PLACE is None:
        print(f"note: the tests that copy by reflink are skipped: {NO_REFLINK}", file=sys.stderr)
    unittest.main()
