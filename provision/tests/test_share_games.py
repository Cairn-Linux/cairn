#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Runs cairn-share-games.py, which moves a game from a Guardian's own
Steam into the family folder (ADR-0042), on folders made for each test.

Standard library only. The program is installed into a temporary folder
beside kidscan.py, as the image installs it, and run there with `python3 -I
-B`, or loaded from there when a test needs to change one of its parts. It
always gets --home, --shared and --proc, so it never reads this account's
own Steam, the computer's family folder, or the real /proc, where the
developer's own Steam may be running.

A real move needs a filesystem that can reflink (btrfs or XFS), found as
test_copy_games finds one; only the tests that need one skip, saying so,
when there is none. Most tests copy with a stand-in for cp instead, and
"other-drive" runs on /dev/shm, which is tmpfs. The tests that end a move,
or a recovery, at each point run it in a child process, which a stand-in
for each change to the disk ends at once with os._exit, so that nothing
after that point runs, as after a kill.
"""

import contextlib
import fcntl
import importlib.machinery
import importlib.util
import io
import os
import pathlib
import re
import shutil
import signal
import stat
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

from test_copy_games import NO_REFLINK, REFLINK_PLACE, needs_not_root, needs_reflink, reflinks

HERE = pathlib.Path(__file__).resolve().parent
PROGRAM = HERE.parent / "cairn-share-games.py"
COPY_PROGRAM = HERE.parent / "cairn-copy-games.py"
KIDSCAN = HERE.parents[1] / "tools" / "kidscan" / "kidscan.py"
LISTS = ("config/libraryfolders.vdf", "steamapps/libraryfolders.vdf")

# Every line the program may print (its docstring), with ADR-0042's twelve
# reasons.
REASONS = ("steam-open", "not-found", "not-installed", "downloading", "installdir", "beta-branch",
           "unreadable-files", "exists", "other-drive", "no-shared", "unreadable-config", "failed")
GRAMMAR = re.compile(
    rf"library (added|had)|(moved|shared) [0-9]{{1,10}}|not-moved [0-9]{{1,10}} ({'|'.join(REASONS)})"
    r"|refused (root|appid)"
)

# Made-up numbers in the places the test VM's lists have them.
OWN_CONTENTID = "4113219736151183021"
FAMILY_CONTENTID = "5807131917442398110"
# The four tools in the "apps" map of the VM's own library.
TOOL_SIZES = (("1070560", "306415842"), ("1391110", "730208519"), ("1493710", "1259117339"),
              ("4183110", "755218114"))

# A game's files: a program, a data file and a save.
GAME_FILES = {
    "start.sh": (b"#!/bin/sh\necho start\n", 0o755),
    "data/level1.dat": (b"level" * 1000, 0o644),
    "Saves/save1": (b"saved", 0o644),
}
BETA = '\t"UserConfig"\n\t{\n\t\t"BetaKey"\t\t"beta"\n\t}\n'

# How a child process ends when the test's stand-in ends it.
KILLED = 77


def manifest(appid, installdir="A Game", flags="4", extra="", name="A Game", size="8000"):
    size_line = f'\t"SizeOnDisk"\t\t"{size}"\n' if size is not None else ""
    return (
        '"AppState"\n{\n'
        f'\t"appid"\t\t"{appid}"\n'
        f'\t"name"\t\t"{name}"\n'
        f'\t"StateFlags"\t\t"{flags}"\n'
        f'\t"installdir"\t\t"{installdir}"\n'
        f"{size_line}"
        '\t"LastOwner"\t\t"76561198000000000"\n'
        f"{extra}"
        "}\n"
    ).encode()


def library_entry(number, path, contentid, apps=(), steams_own=True):
    """One library's entry as the test VM's lists have it: tabs, two between
    a key and its value, and, in Steam's own entries, the two keys Steam
    writes in them."""
    lines = [f'\t"{number}"', "\t{", f'\t\t"path"\t\t"{path}"', '\t\t"label"\t\t""',
             f'\t\t"contentid"\t\t"{contentid}"', '\t\t"totalsize"\t\t"0"']
    if steams_own:
        lines += ['\t\t"update_clean_bytes_tally"\t\t"5521837"', '\t\t"time_last_update_verified"\t\t"1761731411"']
    lines += ['\t\t"apps"', "\t\t{", *(f'\t\t\t"{appid}"\t\t"{size}"' for appid, size in apps), "\t\t}", "\t}"]
    return "\n".join(lines) + "\n"


def steam_list(*entries):
    return ('"libraryfolders"\n{\n' + "".join(entries) + "}\n").encode()


def cairn_entry(number, family, contentid, apps):
    """The entry the program adds, as Steam's own but for those two keys,
    which Steam is expected to add when it next writes the list (ADR-0042
    check 1's second half compares)."""
    return library_entry(number, family, contentid, apps, steams_own=False).encode()


def marker(contentid):
    return f'"libraryfolder"\n{{\n\t"contentid"\t\t"{contentid}"\n\t"label"\t\t""\n}}\n'.encode()


def copy_tree(source, target):
    """A stand-in for cp that needs no reflink, which copies what cp would:
    a folder whole, to a new name, with its links as links."""
    source, target = pathlib.Path(source), pathlib.Path(target)
    if source.is_dir() and not source.is_symlink():
        shutil.copytree(source, target, symlinks=True)
    else:
        shutil.copy2(source, target, follow_symlinks=False)
    return True


def tree(folder, times_of=()):
    """Every path under a folder with its kind, size, mode and change time,
    to tell whether anything in it changed. A folder in `times_of` may have
    new times, from a folder made in it and removed again."""
    seen = {}
    for path in sorted(pathlib.Path(folder).rglob("*")):
        details = path.lstat()
        seen[str(path)] = (details.st_mode, details.st_size,
                           None if path in times_of else details.st_mtime_ns)
    return seen


def files_of(folder):
    """Each file and link under a folder, with what it holds or leads to."""
    found = {}
    for path in sorted(pathlib.Path(folder).rglob("*")):
        if path.is_symlink():
            found[str(path.relative_to(folder))] = ("link", os.readlink(path))
        elif path.is_file():
            found[str(path.relative_to(folder))] = path.read_bytes()
    return found


def files_of_game():
    return {name: content for name, (content, _) in GAME_FILES.items()}


def load_kidscan():
    loader = importlib.machinery.SourceFileLoader("kidscan_for_tests", str(KIDSCAN))
    spec = importlib.util.spec_from_loader(loader.name, loader)
    module = importlib.util.module_from_spec(spec)
    loader.exec_module(module)
    return module


KIDSCAN_MODULE = load_kidscan()


class Machine:
    """A Guardian's home with their Steam, whose two lists name its own
    library as entry "0" laid out as on the test VM, a family folder, a
    stand-in /proc, and the program installed beside kidscan.py, all under
    one folder."""

    def __init__(self, root):
        self.root = pathlib.Path(root)
        self.libexec = self.root / "libexec"
        self.libexec.mkdir()
        shutil.copy(PROGRAM, self.libexec / "cairn-share-games")
        shutil.copy(KIDSCAN, self.libexec / "kidscan.py")
        self.home = self.root / "home" / "gina"
        self.steam = self.home / ".local" / "share" / "Steam"
        self.library = self.steam / "steamapps"
        (self.library / "common").mkdir(parents=True)
        (self.steam / "config").mkdir()
        self.own_list = steam_list(library_entry(0, self.steam, OWN_CONTENTID, TOOL_SIZES))
        for name in LISTS:
            self.write_list(name, self.own_list)
        self.family = self.root / "var" / "steam-library"
        self.shared = self.family / "steamapps"
        (self.shared / "common").mkdir(parents=True)
        # The lock an earlier run made, which stays, so that a test sees
        # only its own run's changes.
        self.lock = self.shared / ".cairn-move-lock"
        self.lock.touch()
        self.lock.chmod(0o660)
        self.proc = self.root / "proc"
        self.proc.mkdir()

    def write_list(self, name, data):
        path = self.steam / name
        path.write_bytes(data)
        # As Steam leaves them on the test VM.
        path.chmod(0o755)

    def lists(self):
        return {name: (self.steam / name).read_bytes() for name in LISTS}

    def name_family(self):
        """The lists and the marker as Steam's own Storage step leaves them,
        as on the test VM."""
        family_entry = library_entry(1, self.family, FAMILY_CONTENTID, (("292780", "143998977"),))
        for name in LISTS:
            self.write_list(name, steam_list(library_entry(0, self.steam, OWN_CONTENTID, TOOL_SIZES),
                                             family_entry))
        (self.family / "libraryfolder.vdf").write_bytes(marker(FAMILY_CONTENTID))

    def add_game(self, appid, installdir="A Game", data=None, library=None, files=None):
        """A game in one of the Guardian's libraries: its manifest and its
        folder."""
        library = library or self.library
        (library / f"appmanifest_{appid}.acf").write_bytes(data or manifest(appid, installdir))
        folder = library / "common" / installdir
        folder.mkdir(parents=True, exist_ok=True)
        for name, (content, mode) in (files or GAME_FILES).items():
            path = folder / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(content)
            path.chmod(mode)
        return folder

    def second_library(self, name="second"):
        """Another library, which both of the Guardian's lists name."""
        steamapps = self.root / "drives" / name / "steamapps"
        (steamapps / "common").mkdir(parents=True)
        entries = [library_entry(0, self.steam, OWN_CONTENTID, TOOL_SIZES),
                   library_entry(1, steamapps.parent, "6000000000000000001")]
        for list_name in LISTS:
            self.write_list(list_name, steam_list(*entries))
        return steamapps

    def steam_runs(self, pid="4242", comm=b"steam\n"):
        """A process of this account in the stand-in /proc."""
        (self.proc / pid).mkdir()
        (self.proc / pid / "comm").write_bytes(comm)

    def arguments(self):
        return ["--home", str(self.home), "--shared", str(self.shared), "--proc", str(self.proc)]

    def command(self, *appids):
        return [sys.executable, "-I", "-B", str(self.libexec / "cairn-share-games"), *self.arguments(), *appids]

    def run(self, *appids):
        # A run that blocks fails the test rather than hanging it.
        done = subprocess.run(self.command(*appids), stdin=subprocess.DEVNULL, capture_output=True, text=True,
                              check=False, timeout=60)
        self.code = done.returncode
        self.stderr = done.stderr
        return done.stdout.splitlines()

    def load(self):
        """The installed program as a module, to change one of its parts."""
        loader = importlib.machinery.SourceFileLoader(
            "cairn_share_games", str(self.libexec / "cairn-share-games"))
        spec = importlib.util.spec_from_loader(loader.name, loader)
        module = importlib.util.module_from_spec(spec)
        loader.exec_module(module)
        return module

    def run_in_process(self, module, *appids, home=True):
        given = self.arguments() if home else self.arguments()[2:]
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = module.main([*given, *appids])
        return code, out.getvalue().splitlines()

    def run_with_stand_in(self, *appids, module=None, parts=None, calls=None):
        """In this process, with a stand-in for cp, so no reflink is needed.
        `parts` replaces parts of the program, and `calls` functions of os.
        The disk is not synced, which only the order test looks at."""
        module = module or self.load()
        with contextlib.ExitStack() as stack:
            stack.enter_context(mock.patch.object(module, "reflink_copy", copy_tree))
            stack.enter_context(mock.patch.object(os, "sync", lambda: None))
            for name, value in (parts or {}).items():
                stack.enter_context(mock.patch.object(module, name, value))
            for name, value in (calls or {}).items():
                stack.enter_context(mock.patch.object(os, name, value))
            return self.run_in_process(module, *appids)


class MoveTestCase(unittest.TestCase):
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

    def fresh_machine(self):
        """A new machine in place of this test's, for a test that tries a
        case more than once."""
        self.tearDown()
        self.setUp()

    def assertOnlyGrammar(self, lines):
        for line in lines:
            self.assertRegex(line, f"^(?:{GRAMMAR.pattern})$")
            self.assertNotIn("/", line)

    def everything(self, strict=True):
        """The home and the family folder, to tell whether anything changed.
        Unless `strict`, the family's common/ may have new times, from this
        run's own folder, made in it at the first probe and removed."""
        times_of = set() if strict else {self.machine.shared / "common"}
        return tree(self.machine.root / "home"), tree(self.machine.family, times_of)

    def assertNoLeftovers(self):
        # The lock is no leftover: it stays.
        for folder in (self.machine.root / "home", self.machine.family):
            left = [str(path) for path in folder.rglob(".cairn-*") if path != self.machine.lock]
            self.assertEqual(left, [])

    def short(self, path):
        """A path under the test's folder, with this run's own folder's
        random name taken out."""
        relative = os.path.relpath(path, self.machine.root)
        return re.sub(r"common/\.cairn-move-[^/]+", "common/<scratch>", relative)

    def assertOnlyInOwnLibrary(self, appid="100", installdir="A Game", data=None):
        machine = self.machine
        self.assertEqual((machine.library / f"appmanifest_{appid}.acf").read_bytes(), data or manifest(appid))
        self.assertEqual(files_of(machine.library / "common" / installdir), files_of_game())
        self.assertFalse(os.path.lexists(machine.shared / f"appmanifest_{appid}.acf"))
        self.assertFalse(os.path.lexists(machine.shared / "common" / installdir))

    def assertOnlyInFamily(self, appid="100", installdir="A Game", data=None):
        machine = self.machine
        self.assertEqual((machine.shared / f"appmanifest_{appid}.acf").read_bytes(), data or manifest(appid))
        self.assertEqual(files_of(machine.shared / "common" / installdir), files_of_game())
        self.assertFalse(os.path.lexists(machine.library / f"appmanifest_{appid}.acf"))
        self.assertFalse(os.path.lexists(machine.library / "common" / installdir))
        self.assertFalse(os.path.lexists(machine.library / f".cairn-move-aside-{appid}"))


class WhichGamesTest(MoveTestCase):
    """Every reason a game stays where it is that comes before a copy: from
    one read of its manifest, its library and the family folder. None of
    them needs reflink, and nothing changes."""

    def left(self, *appids):
        before = self.everything()
        lines = self.machine.run(*appids)
        self.assertEqual(self.machine.code, 0, self.machine.stderr)
        self.assertOnlyGrammar(lines)
        self.assertEqual(self.everything(), before, "nothing changed")
        return lines

    def test_shared_when_the_family_folder_has_its_manifest(self):
        # Check 10: the Guardian's own copy is not touched either.
        (self.machine.shared / "appmanifest_100.acf").write_bytes(manifest(100))
        (self.machine.shared / "common" / "A Game").mkdir()
        self.machine.add_game(100)
        self.assertEqual(self.left("100"), ["shared 100"])
        self.assertTrue((self.machine.library / "common" / "A Game" / "start.sh").is_file())

    def test_not_found_in_any_library(self):
        self.assertEqual(self.left("100"), ["not-moved 100 not-found"])
        real = self.machine.root / "real.acf"
        real.write_bytes(manifest(100))
        own = self.machine.library / "appmanifest_100.acf"
        self.machine.add_game(100)
        makers = {
            "fifo": lambda: os.mkfifo(own),
            "link": lambda: own.symlink_to(real),
            "folder": lambda: own.mkdir(),
            "big": lambda: own.write_bytes(manifest(100) + b" " * (1 << 20)),
            "not utf-8": lambda: own.write_bytes(b"\xff\xfe" + manifest(100)),
            "not vdf": lambda: own.write_bytes(b'"AppState"\n{\n\t"appid" "100"\n'),
            "another appid": lambda: own.write_bytes(manifest(200)),
            "no AppState": lambda: own.write_bytes(b'"Other"\n{\n\t"appid"\t\t"100"\n}\n'),
            # kidscan leaves such a game out too.
            "a name that is a block": lambda: own.write_bytes(
                manifest(100).replace(b'\t"name"\t\t"A Game"\n', b'\t"name"\n\t{\n\t}\n')),
        }
        for kind, make in makers.items():
            with self.subTest(kind=kind):
                own.unlink()
                make()
                try:
                    self.assertEqual(self.left("100"), ["not-moved 100 not-found"])
                finally:
                    own.rmdir() if own.is_dir() and not own.is_symlink() else own.unlink()
                    own.write_bytes(manifest(100))

    def test_one_of_steams_own_tools_is_not_a_game(self):
        # By kidscan's rule: its appid, or a name like a runtime's or Proton's.
        self.machine.add_game(1628350, "SteamLinuxRuntime_sniper",
                              data=manifest(1628350, "SteamLinuxRuntime_sniper", name="Steam Linux Runtime 3.0"))
        self.machine.add_game(2805730, "Proton 9.0", data=manifest(2805730, "Proton 9.0", name="Proton 9.0"))
        self.assertEqual(self.left("1628350", "2805730"),
                         ["not-moved 1628350 not-found", "not-moved 2805730 not-found"])

    def test_an_installdir_that_is_not_one_plain_folder_name(self):
        for installdir in ("", ".", "..", "a/b", "../../.ssh", ".cairn-move-100", "a\x00b"):
            with self.subTest(installdir=installdir):
                (self.machine.library / "appmanifest_100.acf").write_bytes(manifest(100, installdir))
                self.assertEqual(self.left("100"), ["not-moved 100 installdir"])

    def test_a_beta_branch(self):
        self.machine.add_game(100)
        for block in ("UserConfig", "MountedConfig"):
            with self.subTest(block=block):
                extra = f'\t"{block}"\n\t{{\n\t\t"BetaKey"\t\t"beta"\n\t}}\n'
                (self.machine.library / "appmanifest_100.acf").write_bytes(manifest(100, extra=extra))
                self.assertEqual(self.left("100"), ["not-moved 100 beta-branch"])

    def test_anything_but_fully_installed(self):
        self.machine.add_game(100)
        for flags in ("6", "1026", "0", "", "44"):
            with self.subTest(flags=flags):
                (self.machine.library / "appmanifest_100.acf").write_bytes(manifest(100, flags=flags))
                self.assertEqual(self.left("100"), ["not-moved 100 not-installed"])

    def test_something_downloading(self):
        # In its own library, or in the family folder, where the copy step
        # looks.
        self.machine.add_game(100)
        for folder in (self.machine.library, self.machine.shared):
            with self.subTest(folder=folder.parent.name):
                (folder / "downloading" / "100").mkdir(parents=True)
                self.assertEqual(self.left("100"), ["not-moved 100 downloading"])
                shutil.rmtree(folder / "downloading")

    def test_a_game_folder_that_is_missing_a_link_or_a_file_is_not_installed(self):
        (self.machine.library / "appmanifest_100.acf").write_bytes(manifest(100))
        folder = self.machine.library / "common" / "A Game"
        elsewhere = self.machine.add_game(200, "Elsewhere")
        for make in (lambda: None, lambda: folder.symlink_to(elsewhere), lambda: folder.write_bytes(b"x")):
            make()
            self.assertEqual(self.left("100"), ["not-moved 100 not-installed"])
            if os.path.lexists(folder):
                folder.unlink()

    def test_the_family_folder_has_its_folder_but_no_manifest(self):
        self.machine.add_game(100)
        place = self.machine.shared / "common" / "A Game"
        place.mkdir()
        self.assertEqual(self.left("100"), ["not-moved 100 exists"])
        place.rmdir()
        place.symlink_to(self.machine.root)
        self.assertEqual(self.left("100"), ["not-moved 100 exists"])

    @needs_not_root
    def test_a_file_or_folder_not_every_account_can_read(self):
        folder = self.machine.add_game(100)
        for path, mode in ((folder / "data" / "level1.dat", 0o600), (folder / "data", 0o700),
                           (folder / "Saves", 0o750), (folder, 0o700)):
            with self.subTest(path=path.name, mode=oct(mode)):
                before = path.stat().st_mode & 0o777
                path.chmod(mode)
                try:
                    self.assertEqual(self.left("100"), ["not-moved 100 unreadable-files"])
                finally:
                    path.chmod(before)

    def test_reasons_come_in_the_copy_steps_order(self):
        # A manifest with two problems gets the copy step's first word.
        self.machine.add_game(100)
        (self.machine.library / "downloading" / "100").mkdir(parents=True)
        for data, word in ((manifest(100, "../x", flags="6", extra=BETA), "installdir"),
                           (manifest(100, flags="6", extra=BETA), "beta-branch"),
                           (manifest(100, flags="6"), "not-installed")):
            with self.subTest(word=word):
                (self.machine.library / "appmanifest_100.acf").write_bytes(data)
                self.assertEqual(self.left("100"), [f"not-moved 100 {word}"])

    def test_a_game_in_two_of_the_guardians_libraries_does_not_move(self):
        # Moved from one, it would stay installed in the other too, which
        # ADR-0042 rejects a copy for.
        second = self.machine.second_library()
        self.machine.add_game(100)
        self.machine.add_game(100, library=second)
        self.assertEqual(self.left("100"), ["not-moved 100 failed"])


class SameRulesTest(MoveTestCase):
    """A game moves only if the copy step would then copy it (ADR-0042), so
    the two programs give one manifest the same word."""

    place = REFLINK_PLACE

    def copy_step(self, shared, *only):
        child = self.machine.root / "home" / "ada" / ".local" / "share" / "Steam" / "steamapps"
        (child / "common").mkdir(parents=True, exist_ok=True)
        shutil.copy(COPY_PROGRAM, self.machine.libexec / "cairn-copy-games")
        done = subprocess.run(
            [sys.executable, "-I", "-B", str(self.machine.libexec / "cairn-copy-games"), "--shared", str(shared),
             "--library", str(child), "--tools", str(self.machine.root / "no-tools"), "--only", *only],
            stdin=subprocess.DEVNULL, capture_output=True, text=True, check=False, timeout=60)
        self.assertEqual(done.returncode, 0, done.stderr)
        return child, done.stdout.splitlines()

    def test_a_game_the_move_step_leaves_the_copy_step_skips_for_the_same_word(self):
        # The copy step's shared folder is another folder here, with the
        # same manifest and folder as the Guardian's library.
        elsewhere = self.machine.root / "copy-shared" / "steamapps"
        (elsewhere / "common").mkdir(parents=True)
        for word, data in (("installdir", manifest(100, "../x")), ("beta-branch", manifest(100, extra=BETA)),
                           ("not-installed", manifest(100, flags="6")), ("downloading", manifest(100))):
            with self.subTest(word=word):
                for library in (self.machine.library, elsewhere):
                    self.machine.add_game(100, data=data, library=library)
                    if word == "downloading":
                        (library / "downloading" / "100").mkdir(parents=True, exist_ok=True)
                self.assertEqual(self.machine.run("100"), [f"not-moved 100 {word}"], self.machine.stderr)
                self.assertEqual(self.copy_step(elsewhere, "100")[1], [f"skipped 100 {word}"])
                for library in (self.machine.library, elsewhere):
                    shutil.rmtree(library / "downloading", ignore_errors=True)

    @needs_reflink
    def test_the_copy_step_takes_what_this_moved(self):
        self.machine.add_game(100)
        self.assertEqual(self.machine.run("100"), ["library added", "moved 100"], self.machine.stderr)
        child, lines = self.copy_step(self.machine.shared, "100")
        self.assertEqual(lines, ["copied 100"])
        self.assertEqual((child / "appmanifest_100.acf").read_bytes(), manifest(100))
        self.assertEqual(files_of(child / "common" / "A Game"), files_of_game())


class MoveTest(MoveTestCase):
    """Real moves, by reflink."""

    place = REFLINK_PLACE

    @needs_reflink
    def test_a_game_moves_whole_with_its_manifest_byte_for_byte(self):
        # Unusual spacing and a key Cairn does not know stay as Steam wrote
        # them.
        data = manifest(100, extra='\t"UserConfig"\n\t{\n\t\t"BetaKey"\t\t"public"\n\t}\n\t"Odd"   "x"\r\n')
        folder = self.machine.add_game(100, data=data)
        os.utime(folder / "start.sh", (1_000_000_000, 1_000_000_000))
        lines = self.machine.run("100")
        self.assertEqual(lines, ["library added", "moved 100"], self.machine.stderr)
        self.assertEqual(self.machine.code, 0)
        self.assertOnlyInFamily(data=data)
        moved = self.machine.shared / "common" / "A Game"
        self.assertEqual((moved / "start.sh").stat().st_mode & 0o100, 0o100, "the program still runs")
        self.assertEqual((moved / "start.sh").stat().st_mtime, 1_000_000_000)
        self.assertEqual(sorted(path.name for path in self.machine.library.iterdir()),
                         ["common", "libraryfolders.vdf", "libraryfolders.vdf.before-cairn"])
        self.assertEqual(list((self.machine.library / "common").iterdir()), [])
        self.assertEqual(sorted(path.name for path in (self.machine.shared / "common").iterdir()), ["A Game"])
        self.assertNoLeftovers()

    @needs_reflink
    def test_a_second_run_says_shared_and_changes_nothing(self):
        self.machine.add_game(100)
        self.assertEqual(self.machine.run("100"), ["library added", "moved 100"], self.machine.stderr)
        before = self.everything()
        self.assertEqual(self.machine.run("100"), ["shared 100"])
        self.assertEqual(self.everything(), before)

    @needs_reflink
    def test_every_account_can_read_the_moved_game_and_the_group_can_write_it(self):
        # The family folder is set up like the image's: setgid, with a
        # default ACL that takes the umask's place. Steam's own installs
        # there give the group what they give the owner, and so does a move.
        for folder in (self.machine.shared, self.machine.shared / "common"):
            folder.chmod(0o2775)
            if shutil.which("setfacl"):
                subprocess.run(["setfacl", "-m", "d:u::rwx,d:g::rwx,d:m::rwx,d:o::r-x", str(folder)],
                               capture_output=True, check=False)
        acl = "system.posix_acl_default" in os.listxattr(self.machine.shared / "common")
        folder = self.machine.add_game(100)
        (folder / "Saves").chmod(0o755)
        self.assertEqual(self.machine.run("100"), ["library added", "moved 100"], self.machine.stderr)
        moved = self.machine.shared / "common" / "A Game"
        for path in [moved, *moved.rglob("*")]:
            with self.subTest(path=str(path.relative_to(moved))):
                mode = path.stat().st_mode
                self.assertEqual(mode & 0o004, 0o004, "every account can read it")
                self.assertEqual((mode >> 3) & 0o7, (mode >> 6) & 0o7, "the group has the owner's bits")
                if path.is_dir():
                    self.assertEqual(mode & 0o2001, 0o2001, "setgid, and every account can enter it")
                    if acl:
                        self.assertIn("system.posix_acl_default", os.listxattr(path))
        self.assertEqual((self.machine.shared / "appmanifest_100.acf").stat().st_mode & 0o004, 0o004)

    @needs_reflink
    def test_what_stays_in_the_old_library(self):
        # The Wine prefix stays, as Mason chose, and so do the shader cache,
        # the workshop items and the old entry's "apps" map.
        self.machine.add_game(100)
        library = self.machine.library
        kept = [library / "compatdata" / "100" / "pfx" / "user.reg", library / "shadercache" / "100" / "cache",
                library / "workshop" / "content" / "100" / "item", library / "workshop" / "appworkshop_100.acf"]
        for path in kept:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"kept")
        self.machine.write_list("steamapps/libraryfolders.vdf", steam_list(
            library_entry(0, self.machine.steam, OWN_CONTENTID, (*TOOL_SIZES, ("100", "8000")))))
        old_entry = self.machine.lists()["steamapps/libraryfolders.vdf"][:-2]
        self.assertEqual(self.machine.run("100"), ["library added", "moved 100"], self.machine.stderr)
        for path in kept:
            self.assertEqual(path.read_bytes(), b"kept", path.name)
        self.assertTrue(self.machine.lists()["steamapps/libraryfolders.vdf"].startswith(old_entry))

    @needs_reflink
    def test_a_game_in_a_second_library_moves_from_there(self):
        second = self.machine.second_library()
        self.machine.add_game(100, library=second)
        before = self.machine.lists()
        self.assertEqual(self.machine.run("100"), ["library added", "moved 100"], self.machine.stderr)
        self.assertEqual((self.machine.shared / "appmanifest_100.acf").read_bytes(), manifest(100))
        self.assertFalse(os.path.lexists(second / "appmanifest_100.acf"))
        self.assertFalse(os.path.lexists(second / "common" / "A Game"))
        # That library's entry, and every other byte, is as it was.
        for name in LISTS:
            self.assertTrue(self.machine.lists()[name].startswith(before[name][:-2]))

    @needs_reflink
    def test_nothing_it_prints_is_a_path_or_a_name(self):
        self.machine.add_game(100, "Pajama Sam", data=manifest(100, "Pajama Sam", name="Pajama Sam"))
        self.machine.add_game(200, "Spy Fox", data=manifest(200, "Spy Fox", name="Spy Fox", flags="6"))
        (self.machine.library / "appmanifest_300.acf").write_bytes(manifest(301, name="Freddi Fish"))
        lines = self.machine.run("100", "200", "300", "400")
        self.assertEqual(lines, ["not-moved 200 not-installed", "not-moved 300 not-found",
                                 "not-moved 400 not-found", "library added", "moved 100"], self.machine.stderr)
        self.assertOnlyGrammar(lines)
        for line in lines:
            for word in ("Pajama", "Spy", "Freddi", "gina", str(self.machine.root)):
                self.assertNotIn(word, line)


class OrderTest(MoveTestCase):
    """ADR-0042's order, with a stand-in for cp."""

    def test_the_order_of_a_move(self):
        # The lists name the family folder already, as on the test VM, so
        # the only changes are the move's.
        self.machine.name_family()
        self.machine.add_game(100)
        module = self.machine.load()
        events = []
        seen_at_set_aside = []
        real = {name: getattr(os, name) for name in ("rename", "unlink")}
        real_rmtree, real_sync_folder, real_write_new = shutil.rmtree, module.sync_folder, module.write_new
        family, own = self.machine.shared, self.machine.library

        def copy(source, target):
            events.append(("cp", self.short(source), self.short(target)))
            return copy_tree(source, target)

        def rename(source, target):
            if pathlib.Path(source) == own / "appmanifest_100.acf":
                scratch = next((family / "common").glob(".cairn-move-*"))
                seen_at_set_aside.append(((family / ".cairn-move-manifest-100").read_bytes(),
                                          files_of(scratch / "100")))
            events.append(("rename", self.short(source), self.short(target)))
            real["rename"](source, target)

        def unlink(path, *arguments, **keywords):
            if "dir_fd" not in keywords:
                events.append(("unlink", self.short(path)))
            real["unlink"](path, *arguments, **keywords)

        def rmtree(path, *arguments, **keywords):
            events.append(("rmtree", self.short(path)))
            real_rmtree(path, *arguments, **keywords)

        def sync_folder(path):
            events.append(("fsync", self.short(path)))
            real_sync_folder(path)

        def write_new(path, *arguments, **keywords):
            events.append(("write", self.short(path)))
            real_write_new(path, *arguments, **keywords)

        with (mock.patch.object(module, "reflink_copy", copy),
              mock.patch.object(module, "sync_folder", sync_folder),
              mock.patch.object(module, "write_new", write_new),
              mock.patch.object(module.shutil, "rmtree", rmtree),
              mock.patch.object(os, "rename", rename),
              mock.patch.object(os, "unlink", unlink),
              mock.patch.object(os, "sync", lambda: events.append(("sync",)))):
            code, lines = self.machine.run_in_process(module, "100")
        self.assertEqual((code, lines), (0, ["library had", "moved 100"]))
        own_s, family_s = self.short(own), self.short(family)
        first = events.index(next(event for event in events if event[0] == "cp"))
        self.assertEqual(events[first:], [
            ("cp", f"{own_s}/appmanifest_100.acf", f"{family_s}/common/<scratch>/probe"),
            ("unlink", f"{family_s}/common/<scratch>/probe"),
            ("cp", f"{own_s}/common/A Game", f"{family_s}/common/<scratch>/100"),
            ("write", f"{family_s}/.cairn-move-manifest-100"),
            # The copy and its manifest are on the disk before the
            # Guardian's library changes.
            ("sync",),
            ("rename", f"{own_s}/appmanifest_100.acf", f"{own_s}/.cairn-move-aside-100"),
            ("fsync", own_s),
            ("rename", f"{family_s}/common/<scratch>/100", f"{family_s}/common/A Game"),
            ("rename", f"{family_s}/.cairn-move-manifest-100", f"{family_s}/appmanifest_100.acf"),
            # Both renames are on the disk before the original goes.
            ("fsync", f"{family_s}/common"),
            ("fsync", family_s),
            ("rmtree", f"{own_s}/common/A Game"),
            ("unlink", f"{own_s}/.cairn-move-aside-100"),
            ("fsync", f"{own_s}/common"),
            ("fsync", own_s),
            ("rmtree", f"{family_s}/common/<scratch>"),
        ])
        # At the set-aside, the family manifest and the copy were whole.
        self.assertEqual(seen_at_set_aside, [(manifest(100), files_of_game())])
        self.assertOnlyInFamily()

    def test_a_ctrl_c_between_the_set_aside_and_the_renames_waits_for_both(self):
        self.machine.add_game(100)
        module = self.machine.load()
        real_rename = os.rename

        def rename(source, target):
            real_rename(source, target)
            if pathlib.Path(target).name == ".cairn-move-aside-100":
                os.kill(os.getpid(), signal.SIGINT)

        with self.assertRaises(KeyboardInterrupt):
            self.machine.run_with_stand_in("100", module=module, calls={"rename": rename})
        # Felt only once both names were taken.
        self.assertEqual((self.machine.shared / "appmanifest_100.acf").read_bytes(), manifest(100))
        self.assertEqual(files_of(self.machine.shared / "common" / "A Game"), files_of_game())
        self.assertFalse(os.path.lexists(self.machine.library / "appmanifest_100.acf"))
        self.assertEqual(signal.pthread_sigmask(signal.SIG_BLOCK, []), set(), "nothing stays held")
        # The next run, with no games, removes the original and the
        # set-aside manifest.
        self.assertEqual(self.machine.run_with_stand_in(), (0, []))
        self.assertOnlyInFamily()
        self.assertNoLeftovers()

    def test_a_stop_or_a_hang_up_after_the_set_aside_waits_for_both(self):
        # SIGTERM is what a stop and a job's time limit send, and SIGHUP what
        # a closed terminal sends. Neither has a handler, so each ends the
        # run as soon as it is let through, in a child process.
        for sent in (signal.SIGTERM, signal.SIGHUP):
            with self.subTest(signal=sent.name):
                self.fresh_machine()
                self.machine.add_game(100)
                machine = self.machine
                real_rename = os.rename

                def rename(source, target):
                    real_rename(source, target)
                    if pathlib.Path(target).name == ".cairn-move-aside-100":
                        os.kill(os.getpid(), sent)

                def work():
                    # Ending the process, even under a runner that ignores
                    # the signal.
                    signal.signal(sent, signal.SIG_DFL)
                    return machine.run_with_stand_in("100", calls={"rename": rename})

                code, _ = in_child(work)
                self.assertEqual(code, -sent, "ended by the signal")
                # Felt only once both names were taken.
                self.assertEqual((machine.shared / "appmanifest_100.acf").read_bytes(), manifest(100))
                self.assertEqual(files_of(machine.shared / "common" / "A Game"), files_of_game())
                self.assertEqual(machine.run_with_stand_in(), (0, []))
                self.assertOnlyInFamily()
                self.assertNoLeftovers()


# The os functions through which the program, tempfile and shutil change
# the disk. Opening a file counts only when it may make one. While the
# stand-ins are in place os.open is one of them, so the real one is kept.
CHANGES = ("mkdir", "rename", "replace", "unlink", "rmdir", "chmod", "fchmod", "fsync", "open")
REAL_OPEN = os.open


class Ender:
    """Counts the program's changes to the disk, and ends the process at the
    one asked for, just before it or just after it, with os._exit, which
    runs no finally and no clean-up, as a kill would. With none asked for it
    only counts. os.sync only counts: a kill, not a power cut, is the case
    here."""

    def __init__(self, at=None, after=False):
        self.at = at
        self.after = after
        self.count = 0

    def wrap(self, real):
        def change(*arguments, **keywords):
            if real is REAL_OPEN and not (arguments[1] if len(arguments) > 1 else keywords["flags"]) & os.O_CREAT:
                return real(*arguments, **keywords)
            self.count += 1
            number = self.count
            if number == self.at and not self.after:
                os._exit(KILLED)
            try:
                return real(*arguments, **keywords)
            finally:
                if number == self.at:
                    os._exit(KILLED)
        return change

    def patches(self, module):
        yield mock.patch.object(module, "reflink_copy", self.wrap(copy_tree))
        yield mock.patch.object(os, "sync", self.wrap(lambda: None))
        for name in CHANGES:
            yield mock.patch.object(os, name, self.wrap(getattr(os, name)))


def in_child(work):
    """Runs work() in a child process, and gives back how the child ended and
    what work() returned. The child always ends with os._exit, so it never
    returns into the tests."""
    sys.stdout.flush()
    sys.stderr.flush()
    read_end, write_end = os.pipe()
    pid = os.fork()
    if pid == 0:
        code = 70
        try:
            os.close(read_end)
            os.write(write_end, str(work()).encode())
            code = 0
        finally:
            os._exit(code)
    os.close(write_end)
    with os.fdopen(read_end, "rb") as answer:
        sent = answer.read().decode()
    _, status = os.waitpid(pid, 0)
    return os.waitstatus_to_exitcode(status), sent


def run_until(machine, ender, *appids):
    """A run in this process, ended where `ender` says: with appid 100, a
    move, and with none, only a recovery. The number of changes it made, if
    it was not ended."""
    module = machine.load()
    with contextlib.ExitStack() as stack:
        for patch in ender.patches(module):
            stack.enter_context(patch)
        stack.enter_context(contextlib.redirect_stdout(io.StringIO()))
        module.main([*machine.arguments(), *appids])
    return ender.count


class InterruptedTest(MoveTestCase):
    """The order keeps a game out of two libraries at every point, and a run
    ended anywhere is finished or undone by the next one (ADR-0042). The
    runs here are ended by a kill, with os._exit, which runs nothing after
    it."""

    def assertInAtMostOneLibrary(self, machine):
        own = os.path.lexists(machine.library / "appmanifest_100.acf")
        family = os.path.lexists(machine.shared / "appmanifest_100.acf")
        self.assertFalse(own and family, "never in both libraries")
        if not own and not family:
            self.assertTrue(os.path.lexists(machine.library / ".cairn-move-aside-100"),
                            "in neither only while its set-aside manifest is there")
        for name in LISTS:
            self.assertLessEqual(machine.lists()[name].count(f'"{machine.family}"'.encode()), 1)

    def assertListsNameTheFamilyFolderOnce(self, machine):
        contentid = KIDSCAN_MODULE.parse_vdf((machine.family / "libraryfolder.vdf").read_text())
        contentid = contentid["libraryfolder"]["contentid"]
        for name in LISTS:
            folders = KIDSCAN_MODULE.parse_vdf(machine.lists()[name].decode())["libraryfolders"]
            family = [entry for entry in folders.values() if entry["path"] == str(machine.family)]
            self.assertEqual([entry["contentid"] for entry in family], [contentid], name)

    def assertEveryChangedListKeptItsBackup(self, machine):
        """A list names the family folder only once its old bytes are
        beside it."""
        for name in LISTS:
            path = machine.steam / name
            if str(machine.family).encode() in path.read_bytes():
                self.assertEqual(path.with_name(path.name + ".before-cairn").read_bytes(), machine.own_list, name)

    def assertNoCopyAnotherRunCouldFinishIsHalf(self, machine):
        """A later run finishes a move whose copy has its full name while its
        temporary manifest is there, so such a copy is always whole."""
        copy = machine.shared / "common" / "A Game"
        if os.path.lexists(machine.shared / ".cairn-move-manifest-100") and os.path.lexists(copy):
            self.assertEqual(files_of(copy), files_of_game())

    def test_a_move_ended_at_any_point_leaves_the_game_in_one_library(self):
        # Every change a whole move makes, list writes included; the run is
        # ended just before each, and just after.
        with tempfile.TemporaryDirectory(dir=self.place) as root:
            whole = Machine(root)
            whole.add_game(100)
            code, sent = in_child(lambda: run_until(whole, Ender(), "100"))
            self.assertEqual(code, 0)
            self.assertEqual(whole.lists()["config/libraryfolders.vdf"].count(str(whole.family).encode()), 1)
        changes = int(sent)
        self.assertGreater(changes, 30)
        for at in range(1, changes + 1):
            for after in (False, True):
                with self.subTest(at=at, after=after), tempfile.TemporaryDirectory(dir=self.place) as root:
                    machine = Machine(root)
                    machine.add_game(100)
                    code, _ = in_child(lambda: run_until(machine, Ender(at, after), "100"))
                    self.assertEqual(code, KILLED)
                    self.assertInAtMostOneLibrary(machine)
                    self.assertEveryChangedListKeptItsBackup(machine)
                    self.assertNoCopyAnotherRunCouldFinishIsHalf(machine)
                    code, lines = machine.run_with_stand_in("100")
                    self.assertEqual(code, 0)
                    self.assertIn(lines, (["library added", "moved 100"], ["library had", "moved 100"],
                                          ["shared 100"]))
                    self.machine = machine
                    self.assertOnlyInFamily()
                    self.assertNoLeftovers()
                    self.assertListsNameTheFamilyFolderOnce(machine)

    def scratch(self):
        """An earlier run's own folder."""
        scratch = self.machine.shared / "common" / ".cairn-move-k2j4x9q1"
        scratch.mkdir(mode=0o700)
        return scratch

    # What a run ended at each of ADR-0042's points leaves, built on disk,
    # in the order of a move.

    def ended_during_the_copy(self):
        """Part of a copy in the run's own folder."""
        self.machine.add_game(100)
        (self.scratch() / "100" / "data").mkdir(parents=True)

    def ended_after_the_temporary_manifest(self):
        """A whole copy in the run's own folder, and its manifest in the
        family folder under a temporary name."""
        self.machine.add_game(100)
        scratch = self.scratch()
        shutil.copytree(self.machine.library / "common" / "A Game", scratch / "100", symlinks=True)
        (self.machine.shared / ".cairn-move-manifest-100").write_bytes(manifest(100))
        return scratch

    def ended_after_the_set_aside(self):
        """The above, and the original manifest set aside."""
        scratch = self.ended_after_the_temporary_manifest()
        (self.machine.library / "appmanifest_100.acf").rename(self.machine.library / ".cairn-move-aside-100")
        return scratch

    def ended_after_the_copys_rename(self):
        scratch = self.ended_after_the_set_aside()
        (scratch / "100").rename(self.machine.shared / "common" / "A Game")

    def ended_after_step_4(self):
        self.ended_after_the_copys_rename()
        (self.machine.shared / ".cairn-move-manifest-100").rename(self.machine.shared / "appmanifest_100.acf")

    def ended_during_the_removal(self):
        """Part of the original folder, and its set-aside manifest."""
        self.ended_after_step_4()
        shutil.rmtree(self.machine.library / "common" / "A Game" / "data")

    # The manifest Steam writes when the parent installs the game again.
    steams = manifest(100, flags="1026")

    def installed_again(self, ended):
        """What `ended` leaves, and then the game installed again in the
        Guardian's own Steam, whose new manifest is in their library."""
        def state():
            ended()
            (self.machine.library / "appmanifest_100.acf").write_bytes(self.steams)
        state.__name__ = f"{ended.__name__}_then_installed_again"
        return state

    def assertEndedAs(self, where):
        """Where game 100 is once a recovery has run to its end: "own" or
        "family", one library; "steam", Steam's copy only, when Steam
        installed the game again before the family manifest took its name;
        "both", when it had (the ADR's open question about two libraries).
        Steam's copy is never touched."""
        machine = self.machine
        if where == "own":
            self.assertOnlyInOwnLibrary()
        elif where == "family":
            self.assertOnlyInFamily()
        else:
            self.assertEqual((machine.library / "appmanifest_100.acf").read_bytes(), self.steams)
            self.assertEqual(files_of(machine.library / "common" / "A Game"), files_of_game())
            if where == "steam":
                self.assertFalse(os.path.lexists(machine.shared / "appmanifest_100.acf"))
                self.assertEqual(list((machine.shared / "common").iterdir()), [])
            else:
                self.assertEqual((machine.shared / "appmanifest_100.acf").read_bytes(), manifest(100))
                self.assertEqual(files_of(machine.shared / "common" / "A Game"), files_of_game())
        self.assertNoLeftovers()

    def assertNextRunSaysNothing(self):
        self.assertEqual(self.machine.run_with_stand_in(), (0, []))
        self.assertNoLeftovers()

    def test_a_run_ended_during_the_copy(self):
        self.ended_during_the_copy()
        self.assertNextRunSaysNothing()
        self.assertOnlyInOwnLibrary()

    def test_a_run_ended_after_the_temporary_manifest(self):
        self.ended_after_the_temporary_manifest()
        self.assertNextRunSaysNothing()
        self.assertOnlyInOwnLibrary()

    def test_a_run_ended_after_the_set_aside(self):
        self.ended_after_the_set_aside()
        self.assertNextRunSaysNothing()
        self.assertOnlyInOwnLibrary()

    def test_a_copy_already_renamed_whose_family_manifest_is_still_temporary(self):
        # Step 2 came first, so the temporary family manifest is there, and
        # the next run finishes the move.
        self.ended_after_the_copys_rename()
        self.assertNextRunSaysNothing()
        self.assertOnlyInFamily()

    def test_a_run_ended_after_step_4(self):
        self.ended_after_step_4()
        self.assertNextRunSaysNothing()
        self.assertOnlyInFamily()

    def test_a_run_ended_during_the_removal_of_the_original(self):
        self.ended_during_the_removal()
        self.assertNextRunSaysNothing()
        self.assertOnlyInFamily()

    def test_steam_installed_the_game_again_meanwhile(self):
        # Steam's own copy is kept, with its folder, and the move's goes.
        for ended, where in ((self.ended_after_the_set_aside, "steam"), (self.ended_after_the_copys_rename, "steam"),
                             (self.ended_after_step_4, "both")):
            with self.subTest(ended=ended.__name__):
                self.fresh_machine()
                self.installed_again(ended)()
                self.assertNextRunSaysNothing()
                self.assertEndedAs(where)

    def test_a_recovery_ended_at_any_point_is_finished_by_the_next_one(self):
        # A recovery, a run with no appids, from each point a move can be
        # ended at, ended in turn just before and just after each of its own
        # changes to the disk.
        starts = ((self.ended_during_the_copy, "own"), (self.ended_after_the_temporary_manifest, "own"),
                  (self.ended_after_the_set_aside, "own"), (self.ended_after_the_copys_rename, "family"),
                  (self.ended_after_step_4, "family"), (self.ended_during_the_removal, "family"),
                  (self.installed_again(self.ended_after_the_set_aside), "steam"),
                  (self.installed_again(self.ended_after_the_copys_rename), "steam"),
                  (self.installed_again(self.ended_after_step_4), "both"))
        for start, where in starts:
            self.fresh_machine()
            start()
            whole = self.machine
            code, sent = in_child(lambda: run_until(whole, Ender()))
            self.assertEqual(code, 0, start.__name__)
            for at in range(1, int(sent) + 1):
                for after in (False, True):
                    with self.subTest(start=start.__name__, at=at, after=after):
                        self.fresh_machine()
                        start()
                        machine = self.machine
                        code, _ = in_child(lambda: run_until(machine, Ender(at, after)))
                        self.assertEqual(code, KILLED)
                        if where in ("own", "family"):
                            self.assertInAtMostOneLibrary(machine)
                        self.assertNoCopyAnotherRunCouldFinishIsHalf(machine)
                        self.assertNextRunSaysNothing()
                        self.assertEndedAs(where)

    def test_the_order_of_a_recovery(self):
        # From a run ended after the copy's rename: the family manifest takes
        # its name, both family folders are on the disk, and only then does
        # the original go, before its set-aside manifest.
        self.ended_after_the_copys_rename()
        module = self.machine.load()
        events = []
        real_rename, real_unlink, real_rmtree = os.rename, os.unlink, shutil.rmtree
        real_sync_folder = module.sync_folder

        def rename(source, target):
            events.append(("rename", self.short(source), self.short(target)))
            real_rename(source, target)

        def unlink(path, *arguments, **keywords):
            if "dir_fd" not in keywords:
                events.append(("unlink", self.short(path)))
            real_unlink(path, *arguments, **keywords)

        def rmtree(path, *arguments, **keywords):
            events.append(("rmtree", self.short(path)))
            real_rmtree(path, *arguments, **keywords)

        def sync_folder(path):
            events.append(("fsync", self.short(path)))
            real_sync_folder(path)

        with (mock.patch.object(module, "sync_folder", sync_folder),
              mock.patch.object(module.shutil, "rmtree", rmtree),
              mock.patch.object(os, "rename", rename),
              mock.patch.object(os, "unlink", unlink)):
            self.assertEqual(self.machine.run_in_process(module), (0, []))
        family, own, steam = (self.short(path) for path in (self.machine.shared, self.machine.library,
                                                             self.machine.steam))
        self.assertEqual(events, [
            ("rename", f"{family}/.cairn-move-manifest-100", f"{family}/appmanifest_100.acf"),
            # On the disk before anything in the Guardian's library goes.
            ("fsync", f"{family}/common"),
            ("fsync", family),
            ("rmtree", f"{own}/common/A Game"),
            ("unlink", f"{own}/.cairn-move-aside-100"),
            ("fsync", f"{own}/common"),
            ("fsync", own),
            # The leftovers, the run's own folder among them.
            ("rmtree", f"{family}/common/<scratch>"),
            ("unlink", f"{steam}/config/.cairn-list-new"),
            ("unlink", f"{own}/.cairn-list-new"),
            ("unlink", f"{self.short(self.machine.family)}/.cairn-marker-new"),
        ])
        self.assertOnlyInFamily()

    def test_a_temporary_manifest_beside_the_familys_own_install_goes_alone(self):
        # A run ended before its copy took its name, and then another
        # Guardian's Steam installed the game in the family folder itself.
        # That install is the family's copy now, and stays as it is.
        theirs = manifest(100, extra='\t"buildid"\t\t"9"\n')
        for ended in (self.ended_after_the_temporary_manifest, self.ended_after_the_set_aside):
            with self.subTest(ended=ended.__name__):
                self.fresh_machine()
                ended()
                (self.machine.shared / "appmanifest_100.acf").write_bytes(theirs)
                (self.machine.shared / "common" / "A Game").mkdir()
                (self.machine.shared / "common" / "A Game" / "theirs.dat").write_bytes(b"theirs")
                self.assertNextRunSaysNothing()
                self.assertEqual((self.machine.shared / "appmanifest_100.acf").read_bytes(), theirs)
                self.assertEqual(files_of(self.machine.shared / "common" / "A Game"), {"theirs.dat": b"theirs"})
                if ended == self.ended_after_the_temporary_manifest:
                    # Never set aside, so the Guardian's own copy is as it was.
                    self.assertEqual((self.machine.library / "appmanifest_100.acf").read_bytes(), manifest(100))
                    self.assertEqual(files_of(self.machine.library / "common" / "A Game"), files_of_game())
                else:
                    # Set aside, and the family folder has the game, so the
                    # original goes.
                    self.assertFalse(os.path.lexists(self.machine.library / "appmanifest_100.acf"))
                    self.assertFalse(os.path.lexists(self.machine.library / "common" / "A Game"))

    def test_a_temporary_manifest_that_cannot_take_its_name_keeps_the_set_aside(self):
        # Putting the original manifest back now would leave a copy that the
        # next run, or another Guardian's, finishes: the game in two
        # libraries.
        self.ended_after_the_copys_rename()
        real_rename = os.rename

        def rename(source, target):
            if pathlib.Path(target) == self.machine.shared / "appmanifest_100.acf":
                raise OSError("read-only")
            real_rename(source, target)

        self.assertEqual(self.machine.run_with_stand_in(calls={"rename": rename}), (0, []))
        self.assertTrue((self.machine.library / ".cairn-move-aside-100").is_file())
        self.assertFalse(os.path.lexists(self.machine.library / "appmanifest_100.acf"))
        self.assertEqual((self.machine.shared / ".cairn-move-manifest-100").read_bytes(), manifest(100))
        self.assertEqual(files_of(self.machine.shared / "common" / "A Game"), files_of_game())
        self.assertNextRunSaysNothing()
        self.assertOnlyInFamily()

    @needs_not_root
    def test_an_original_recovery_cannot_remove_keeps_its_set_aside(self):
        # Every later run tries again, as after a move whose original could
        # not go.
        self.ended_after_step_4()
        (self.machine.library / "common").chmod(0o555)
        try:
            self.assertEqual(self.machine.run_with_stand_in(), (0, []))
            self.assertTrue((self.machine.library / ".cairn-move-aside-100").is_file())
        finally:
            (self.machine.library / "common").chmod(0o755)
        self.assertNextRunSaysNothing()
        self.assertOnlyInFamily()

    def test_a_set_aside_manifest_that_cannot_be_read_is_left_alone(self):
        aside = self.machine.library / ".cairn-move-aside-100"
        real = self.machine.root / "real.acf"
        real.write_bytes(manifest(100))
        for make in (lambda: aside.write_bytes(b"\xff\xfe"), lambda: aside.write_bytes(manifest(200)),
                     lambda: aside.symlink_to(real), lambda: aside.write_bytes(manifest(100, "../x"))):
            make()
            before = self.everything()
            self.assertEqual(self.machine.run_with_stand_in(), (0, []))
            self.assertEqual(self.everything(), before)
            aside.unlink()

    def test_another_guardians_move_ended_in_step_4_is_finished(self):
        # Their set-aside manifest is in their own home, which this account
        # cannot read, and this Guardian's Steam does not have the game.
        (self.machine.shared / "common" / "A Game").mkdir()
        (self.machine.shared / "common" / "A Game" / "start.sh").write_bytes(b"theirs")
        (self.machine.shared / ".cairn-move-manifest-100").write_bytes(manifest(100))
        self.assertNextRunSaysNothing()
        self.assertEqual((self.machine.shared / "appmanifest_100.acf").read_bytes(), manifest(100))

    def test_an_original_goes_only_once_the_family_folder_has_the_whole_game(self):
        # A family manifest without its folder, such as another Guardian's
        # Steam starting an install there.
        self.ended_after_step_4()
        shutil.rmtree(self.machine.shared / "common" / "A Game")
        self.assertEqual(self.machine.run_with_stand_in(), (0, []))
        self.assertTrue((self.machine.library / ".cairn-move-aside-100").is_file())
        self.assertEqual(files_of(self.machine.library / "common" / "A Game"), files_of_game())

    def test_recovery_waits_while_steam_is_open(self):
        self.ended_after_the_set_aside()
        self.machine.steam_runs()
        before = self.everything()
        self.assertEqual(self.machine.run_with_stand_in(), (0, []))
        self.assertEqual(self.everything(), before)
        shutil.rmtree(self.machine.proc / "4242")
        self.assertNextRunSaysNothing()
        self.assertOnlyInOwnLibrary()

    def test_recovery_removes_every_leftover_and_says_nothing(self):
        self.machine.add_game(100)
        self.scratch()
        (self.machine.shared / "common" / ".cairn-move-probe").write_bytes(b"half")
        (self.machine.steam / "config" / ".cairn-list-new").write_bytes(b"half")
        (self.machine.library / ".cairn-list-new").write_bytes(b"half")
        (self.machine.family / ".cairn-marker-new").write_bytes(b"half")
        (self.machine.shared / ".cairn-move-manifest-200").write_bytes(b"half")
        kept = [self.machine.shared / "common" / ".cairn-other", self.machine.shared / ".cairn-move-notes",
                self.machine.library / ".cairn-move-aside-x"]
        for path in kept:
            path.write_bytes(b"keep")
        lists = self.machine.lists()
        self.assertEqual(self.machine.run_with_stand_in(), (0, []))
        self.assertEqual(sorted(path.name for path in (self.machine.shared / "common").iterdir()), [".cairn-other"])
        self.assertFalse(os.path.lexists(self.machine.family / ".cairn-marker-new"))
        self.assertFalse(os.path.lexists(self.machine.shared / ".cairn-move-manifest-200"))
        self.assertEqual(self.machine.lists(), lists)
        for path in kept:
            self.assertEqual(path.read_bytes(), b"keep", path.name)
        self.assertOnlyInOwnLibrary()


class LibraryListTest(MoveTestCase):
    """The family folder in the Guardian's library lists, once, before the
    first move. Copies use a stand-in for cp."""

    def test_the_entry_goes_into_both_lists_before_the_last_brace(self):
        second = self.machine.second_library()
        self.machine.add_game(100)
        # A mode the umask would change: each list keeps its own.
        modes = {"config/libraryfolders.vdf": 0o664, "steamapps/libraryfolders.vdf": 0o755}
        for name, mode in modes.items():
            (self.machine.steam / name).chmod(mode)
        before = self.machine.lists()
        self.assertEqual(self.machine.run_with_stand_in("100"), (0, ["library added", "moved 100"]))
        contentid = KIDSCAN_MODULE.parse_vdf((self.machine.family / "libraryfolder.vdf").read_text())
        contentid = contentid["libraryfolder"]["contentid"]
        self.assertRegex(contentid, r"^[1-9][0-9]{0,18}$")
        self.assertLess(int(contentid), 2**63)
        self.assertNotIn(contentid, (OWN_CONTENTID, "6000000000000000001"))
        for name in LISTS:
            with self.subTest(name=name):
                path = self.machine.steam / name
                self.assertEqual(path.read_bytes(), before[name][:-2] + cairn_entry(
                    2, self.machine.family, contentid, (("100", "8000"),)) + b"}\n")
                self.assertEqual(path.with_name(path.name + ".before-cairn").read_bytes(), before[name])
                self.assertEqual(path.stat().st_mode & 0o777, modes[name], "the list keeps its mode")
                self.assertEqual(path.with_name(path.name + ".before-cairn").stat().st_mode & 0o777, modes[name])
        self.assertTrue(second.is_dir())

    def test_the_new_entry_is_numbered_one_more_than_the_highest(self):
        # With "0" and "2", a count of the entries would give a second "2".
        self.machine.add_game(100)
        entries = (library_entry(0, self.machine.steam, OWN_CONTENTID),
                   library_entry(2, self.machine.root / "unplugged", "6000000000000000002"))
        for name in LISTS:
            self.machine.write_list(name, steam_list(*entries))
        (self.machine.family / "libraryfolder.vdf").write_bytes(marker(FAMILY_CONTENTID))
        before = self.machine.lists()
        self.assertEqual(self.machine.run_with_stand_in("100"), (0, ["library added", "moved 100"]))
        for name in LISTS:
            self.assertEqual(self.machine.lists()[name], before[name][:-2] + cairn_entry(
                3, self.machine.family, FAMILY_CONTENTID, (("100", "8000"),)) + b"}\n")

    def test_the_contentid_comes_from_the_marker(self):
        # Valve documents no format for it, and it is only copied.
        for contentid in (FAMILY_CONTENTID, "-1234567890"):
            with self.subTest(contentid=contentid):
                self.fresh_machine()
                self.machine.add_game(100)
                (self.machine.family / "libraryfolder.vdf").write_bytes(marker(contentid))
                os.utime(self.machine.family / "libraryfolder.vdf", (1_000_000_000, 1_000_000_000))
                before = self.machine.lists()
                self.assertEqual(self.machine.run_with_stand_in("100"), (0, ["library added", "moved 100"]))
                for name in LISTS:
                    self.assertEqual(self.machine.lists()[name], before[name][:-2] + cairn_entry(
                        1, self.machine.family, contentid, (("100", "8000"),)) + b"}\n")
                self.assertEqual((self.machine.family / "libraryfolder.vdf").read_bytes(), marker(contentid))
                self.assertEqual((self.machine.family / "libraryfolder.vdf").stat().st_mtime, 1_000_000_000)

    def test_with_no_marker_one_is_written_with_a_new_contentid(self):
        # The first number drawn is one a list has already.
        self.machine.add_game(100)
        module = self.machine.load()
        drawn = iter([int(OWN_CONTENTID) - 1, 41])
        with mock.patch.object(module.secrets, "randbelow", lambda limit: next(drawn)):
            code, lines = self.machine.run_with_stand_in("100", module=module)
        self.assertEqual((code, lines), (0, ["library added", "moved 100"]))
        self.assertEqual((self.machine.family / "libraryfolder.vdf").read_bytes(), marker("42"))
        self.assertEqual((self.machine.family / "libraryfolder.vdf").stat().st_mode & 0o004, 0o004)
        for name in LISTS:
            self.assertIn(cairn_entry(1, self.machine.family, "42", (("100", "8000"),)), self.machine.lists()[name])

    def test_a_list_that_names_the_family_folder_is_left_alone(self):
        self.machine.add_game(100)
        link = self.machine.root / "family-link"
        link.symlink_to(self.machine.family)
        for path in (str(self.machine.family), str(self.machine.family) + "/", str(link)):
            with self.subTest(path=path):
                entries = (library_entry(0, self.machine.steam, OWN_CONTENTID),
                           library_entry(1, path, FAMILY_CONTENTID))
                for name in LISTS:
                    self.machine.write_list(name, steam_list(*entries))
                before = self.machine.lists()
                module = self.machine.load()
                # Nothing moves, so that the same game can be tried again.
                with mock.patch.object(module, "move", lambda *arguments: "failed"):
                    self.assertEqual(self.machine.run_with_stand_in("100", module=module),
                                     (0, ["library had", "not-moved 100 failed"]))
                self.assertEqual(self.machine.lists(), before)
                self.assertFalse(os.path.lexists(self.machine.family / "libraryfolder.vdf"), "no marker")
        # One list names it and the other does not: only the other gets it.
        self.machine.write_list("config/libraryfolders.vdf", self.machine.own_list)
        (self.machine.family / "libraryfolder.vdf").write_bytes(marker(FAMILY_CONTENTID))
        before = self.machine.lists()
        self.assertEqual(self.machine.run_with_stand_in("100"), (0, ["library added", "moved 100"]))
        self.assertEqual(self.machine.lists()["steamapps/libraryfolders.vdf"], before["steamapps/libraryfolders.vdf"])
        self.assertEqual(self.machine.lists()["config/libraryfolders.vdf"], self.machine.own_list[:-2] + cairn_entry(
            1, self.machine.family, FAMILY_CONTENTID, (("100", "8000"),)) + b"}\n")

    def test_the_apps_map_holds_the_games_about_to_move(self):
        self.machine.add_game(100, "First")
        self.machine.add_game(200, "Second", data=manifest(200, "Second", size=None))
        self.machine.add_game(300, "Third", data=manifest(300, "Third", flags="6"))
        self.machine.add_game(400, "Fourth", data=manifest(400, "Fourth", size="12 GB"))
        code, lines = self.machine.run_with_stand_in("100", "200", "300", "400")
        self.assertEqual((code, lines), (0, ["not-moved 300 not-installed", "library added", "moved 100",
                                             "moved 200", "moved 400"]))
        folders = KIDSCAN_MODULE.parse_vdf(self.machine.lists()["config/libraryfolders.vdf"].decode())
        self.assertEqual(folders["libraryfolders"]["1"]["apps"], {"100": "8000", "200": "0", "400": "0"})

    def test_a_list_it_cannot_read_moves_nothing(self):
        self.machine.add_game(100)
        steam, family = str(self.machine.steam), str(self.machine.family)
        own = library_entry(0, steam, OWN_CONTENTID)
        good = self.machine.root / "good.vdf"
        good.write_bytes(self.machine.own_list)
        bad = {
            "fifo": lambda path: os.mkfifo(path),
            "link": lambda path: path.symlink_to(good),
            "folder": lambda path: path.mkdir(),
            "big": lambda path: path.write_bytes(self.machine.own_list + b" " * (1 << 20)),
            "not utf-8": lambda path: path.write_bytes(b"\xff" + self.machine.own_list),
            "not vdf": lambda path: path.write_bytes(self.machine.own_list[:-2]),
            "old bare paths": lambda path: path.write_bytes(f'"LibraryFolders"\n{{\n\t"1"\t\t"{steam}"\n}}\n'.encode()),
            "two root keys": lambda path: path.write_bytes(steam_list(own) + b'"other"\n{\n}\n'),
            "no entry 0": lambda path: path.write_bytes(steam_list(library_entry(1, steam, OWN_CONTENTID))),
            "text after the brace": lambda path: path.write_bytes(self.machine.own_list + b'"x"\n'),
            "a brace in a comment": lambda path: path.write_bytes(self.machine.own_list + b"// }\n"),
            "an entry that is not a block": lambda path: path.write_bytes(
                steam_list(own, f'\t"1"\t\t"{family}-old"\n')),
        }
        for name in LISTS:
            for kind, make in bad.items():
                with self.subTest(name=name, kind=kind):
                    path = self.machine.steam / name
                    path.unlink()
                    make(path)
                    before = self.everything(strict=False)
                    try:
                        self.assertEqual(self.machine.run_with_stand_in("100"),
                                         (0, ["not-moved 100 unreadable-config"]))
                        self.assertEqual(self.everything(strict=False), before)
                    finally:
                        path.rmdir() if path.is_dir() and not path.is_symlink() else path.unlink()
                        self.machine.write_list(name, self.machine.own_list)

    def test_a_marker_or_lists_it_cannot_read_moves_nothing(self):
        self.machine.add_game(100)
        path = self.machine.family / "libraryfolder.vdf"
        real = self.machine.root / "real.vdf"
        real.write_bytes(marker(FAMILY_CONTENTID))
        for kind, make in (("link", lambda: path.symlink_to(real)), ("fifo", lambda: os.mkfifo(path)),
                           ("not vdf", lambda: path.write_bytes(b'"libraryfolder"\n{\n')),
                           ("no number", lambda: path.write_bytes(marker("abc"))),
                           ("another block", lambda: path.write_bytes(b'"other"\n{\n\t"contentid"\t\t"1"\n}\n'))):
            with self.subTest(kind=kind):
                make()
                before = self.everything(strict=False)
                self.assertEqual(self.machine.run_with_stand_in("100"), (0, ["not-moved 100 unreadable-config"]))
                self.assertEqual(self.everything(strict=False), before, "never written over")
                path.unlink()
        # No list at all: the Guardian's Steam has never run.
        for name in LISTS:
            (self.machine.steam / name).unlink()
        self.assertEqual(self.machine.run_with_stand_in("100"), (0, ["not-moved 100 unreadable-config"]))

    def test_both_lists_are_checked_before_either_is_written(self):
        self.machine.add_game(100)
        self.machine.write_list("steamapps/libraryfolders.vdf",
                                steam_list(library_entry(1, self.machine.steam, OWN_CONTENTID)))
        before = self.everything(strict=False)
        self.assertEqual(self.machine.run_with_stand_in("100"), (0, ["not-moved 100 unreadable-config"]))
        self.assertEqual(self.everything(strict=False), before, "config/ is not written, nor the marker")

    def test_a_missing_list_is_passed_over(self):
        self.machine.add_game(100)
        (self.machine.steam / "config" / "libraryfolders.vdf").unlink()
        self.assertEqual(self.machine.run_with_stand_in("100"), (0, ["library added", "moved 100"]))
        self.assertFalse(os.path.lexists(self.machine.steam / "config" / "libraryfolders.vdf"))

    @needs_not_root
    def test_a_list_it_cannot_write_moves_nothing_and_the_next_run_completes_it(self):
        # config/ is written, then steamapps/ cannot be: the next run adds
        # the entry to steamapps/ only, with the marker's contentid.
        self.machine.add_game(100)
        self.machine.library.chmod(0o555)
        try:
            self.assertEqual(self.machine.run_with_stand_in("100"), (0, ["not-moved 100 unreadable-config"]))
        finally:
            self.machine.library.chmod(0o755)
        self.assertEqual(self.machine.lists()["steamapps/libraryfolders.vdf"], self.machine.own_list)
        self.assertEqual([path.name for path in self.machine.library.glob(".cairn-*")], [])
        self.assertOnlyInOwnLibrary()
        self.assertEqual(self.machine.run_with_stand_in("100"), (0, ["library added", "moved 100"]))
        contentid = KIDSCAN_MODULE.parse_vdf((self.machine.family / "libraryfolder.vdf").read_text())
        contentid = contentid["libraryfolder"]["contentid"]
        for name in LISTS:
            self.assertEqual(self.machine.lists()[name], self.machine.own_list[:-2] + cairn_entry(
                1, self.machine.family, contentid, (("100", "8000"),)) + b"}\n")

    def test_no_library_line_when_nothing_moves(self):
        self.machine.add_game(100, data=manifest(100, flags="6"))
        before = self.machine.lists()
        self.assertEqual(self.machine.run_with_stand_in("100", "200"),
                         (0, ["not-moved 100 not-installed", "not-moved 200 not-found"]))
        self.assertEqual(self.machine.lists(), before)
        self.assertFalse(os.path.lexists(self.machine.family / "libraryfolder.vdf"))

    def test_the_own_folder_under_its_real_path_is_read_once(self):
        # On Bazzite /home is a link to /var/home, and the lists name the
        # real path: the password database's home is the same library.
        real_home = self.machine.root / "var" / "home"
        (self.machine.root / "home").rename(real_home)
        (self.machine.root / "home").symlink_to(real_home)
        real_steam = real_home / "gina" / ".local" / "share" / "Steam"
        for name in LISTS:
            self.machine.write_list(name, steam_list(library_entry(0, real_steam, OWN_CONTENTID)))
        self.machine.add_game(100)
        self.assertEqual(self.machine.run_with_stand_in("100"), (0, ["library added", "moved 100"]))

    def test_a_list_without_a_newline_before_its_last_brace(self):
        self.machine.add_game(100)
        (self.machine.family / "libraryfolder.vdf").write_bytes(marker(FAMILY_CONTENTID))
        squeezed = self.machine.own_list[:-3] + b"}"
        for name in LISTS:
            self.machine.write_list(name, squeezed)
        self.assertEqual(self.machine.run_with_stand_in("100"), (0, ["library added", "moved 100"]))
        self.assertEqual(self.machine.lists()["config/libraryfolders.vdf"], squeezed[:-1] + b"\n" + cairn_entry(
            1, self.machine.family, FAMILY_CONTENTID, (("100", "8000"),)) + b"}")


class SteamOpenTest(MoveTestCase):
    """Nothing moves, and nothing is written, while a process of the
    Guardian's own Steam runs, by OwnSteam's rule."""

    def test_a_steam_of_this_account_holds_every_game(self):
        (self.machine.shared / "appmanifest_100.acf").write_bytes(manifest(100))
        self.machine.add_game(200, "Second")
        # Left by an earlier run, and not recovered meanwhile.
        (self.machine.shared / "common" / ".cairn-move-k2j4x9q1").mkdir()
        self.machine.steam_runs()
        before = self.everything()
        self.assertEqual(self.machine.run("100", "200", "300"),
                         ["shared 100", "not-moved 200 steam-open", "not-moved 300 steam-open"])
        self.assertEqual(self.everything(), before)

    def test_other_programs_and_other_accounts_steam_do_not_count(self):
        self.machine.add_game(100)
        for pid, comm in (("100", b"steamwebhelper\n"), ("101", b"bazzite-steam\n"), ("102", b"steam.sh\n"),
                          ("103", b"steam2\n"), ("self", b"steam\n")):
            self.machine.steam_runs(pid, comm)
        self.assertEqual(self.machine.run_with_stand_in("100"), (0, ["library added", "moved 100"]))
        # Another account's Steam: the folder's owner is not this account.
        self.machine.add_game(200, "Second")
        self.machine.steam_runs()
        module = self.machine.load()
        with mock.patch.object(module.os, "getuid", return_value=os.getuid() + 1):
            self.assertEqual(self.machine.run_with_stand_in("200", module=module), (0, ["library had", "moved 200"]))

    def test_steam_started_during_the_copy_throws_the_copy_away(self):
        # Found right before the set-aside, after the copy and its manifest.
        self.machine.add_game(100)

        def copy_while_steam_starts(source, target):
            copy_tree(source, target)
            if pathlib.Path(target).name == "100":
                self.machine.steam_runs()
            return True

        self.assertEqual(self.machine.run_with_stand_in("100", parts={"reflink_copy": copy_while_steam_starts}),
                         (0, ["library added", "not-moved 100 steam-open"]))
        self.assertOnlyInOwnLibrary()
        self.assertNoLeftovers()

    def test_steam_started_between_two_games_holds_the_second(self):
        # Found before the second game's copy, so it is not even copied.
        self.machine.add_game(100, "First")
        self.machine.add_game(200, "Second")
        module = self.machine.load()
        real_finish = module.finish
        copied = []

        def finish_while_steam_starts(game, family):
            real_finish(game, family)
            self.machine.steam_runs()

        def copy(source, target):
            copied.append(pathlib.Path(target).name)
            return copy_tree(source, target)

        code, lines = self.machine.run_with_stand_in("100", "200", module=module,
                                                      parts={"finish": finish_while_steam_starts,
                                                             "reflink_copy": copy})
        self.assertEqual((code, lines), (0, ["library added", "moved 100", "not-moved 200 steam-open"]))
        self.assertEqual(copied, ["probe", "100"])
        self.assertOnlyInOwnLibrary("200", "Second", manifest(200, "Second"))

    def test_steam_started_before_the_lists_are_written_holds_every_game(self):
        self.machine.add_game(100)

        def probe_while_steam_starts(source, target):
            self.machine.steam_runs()
            return copy_tree(source, target)

        before = self.machine.lists()
        self.assertEqual(self.machine.run_with_stand_in("100", parts={"reflink_copy": probe_while_steam_starts}),
                         (0, ["not-moved 100 steam-open"]))
        self.assertEqual(self.machine.lists(), before)
        self.assertFalse(os.path.lexists(self.machine.family / "libraryfolder.vdf"))
        self.assertOnlyInOwnLibrary()

    def test_a_process_list_it_cannot_read_counts_as_open(self):
        self.machine.add_game(100)
        self.machine.proc.rmdir()
        self.assertEqual(self.machine.run("100"), ["not-moved 100 steam-open"])
        self.assertOnlyInOwnLibrary()


class FailuresTest(MoveTestCase):
    """A step that fails leaves the game in exactly one library, and nothing
    of the attempt behind. Copies use a stand-in for cp."""

    def renames_failing(self, wanted):
        real_rename = os.rename

        def rename(source, target):
            if wanted(pathlib.Path(source), pathlib.Path(target)):
                raise OSError("disk full")
            real_rename(source, target)
        return rename

    def assertNothingInFamily(self):
        self.assertEqual(sorted(path.name for path in self.machine.shared.iterdir()), [".cairn-move-lock", "common"])
        self.assertEqual(list((self.machine.shared / "common").iterdir()), [])

    def test_a_copy_that_fails_leaves_nothing_half_done(self):
        self.machine.add_game(100)

        def half_a_copy(source, target):
            if pathlib.Path(target).name == "probe":
                return copy_tree(source, target)
            pathlib.Path(target).mkdir()
            (pathlib.Path(target) / "start.sh").write_bytes(b"half")
            return False

        self.assertEqual(self.machine.run_with_stand_in("100", parts={"reflink_copy": half_a_copy}),
                         (0, ["library added", "not-moved 100 failed"]))
        self.assertOnlyInOwnLibrary()
        self.assertNothingInFamily()

    def test_a_manifest_that_changed_during_the_copy(self):
        self.machine.add_game(100)

        def copy_while_steam_updates(source, target):
            copy_tree(source, target)
            if pathlib.Path(target).name == "100":
                (self.machine.library / "appmanifest_100.acf").write_bytes(manifest(100, flags="6"))
            return True

        self.assertEqual(self.machine.run_with_stand_in("100", parts={"reflink_copy": copy_while_steam_updates}),
                         (0, ["library added", "not-moved 100 failed"]))
        self.assertOnlyInOwnLibrary(data=manifest(100, flags="6"))
        self.assertNothingInFamily()

    def test_the_family_folder_got_the_game_during_the_copy(self):
        self.machine.add_game(100)

        def copy_while_another_steam_installs(source, target):
            copy_tree(source, target)
            if pathlib.Path(target).name == "100":
                (self.machine.shared / "common" / "A Game").mkdir()
            return True

        code, lines = self.machine.run_with_stand_in("100", parts={"reflink_copy": copy_while_another_steam_installs})
        self.assertEqual((code, lines), (0, ["library added", "not-moved 100 exists"]))
        self.assertEqual(list((self.machine.shared / "common").iterdir()), [self.machine.shared / "common" / "A Game"])
        self.assertFalse(os.path.lexists(self.machine.shared / ".cairn-move-manifest-100"))

    def test_a_temporary_manifest_that_cannot_be_written(self):
        self.machine.add_game(100)
        module = self.machine.load()
        real_write_new = module.write_new

        def write_new(path, *arguments, **keywords):
            if path.name.startswith(".cairn-move-manifest-"):
                raise OSError("disk full")
            real_write_new(path, *arguments, **keywords)

        self.assertEqual(self.machine.run_with_stand_in("100", module=module, parts={"write_new": write_new}),
                         (0, ["library added", "not-moved 100 failed"]))
        self.assertOnlyInOwnLibrary()
        self.assertNothingInFamily()

    def test_a_manifest_that_cannot_be_set_aside_changes_nothing(self):
        self.machine.add_game(100)
        rename = self.renames_failing(lambda source, target: target.name == ".cairn-move-aside-100")
        self.assertEqual(self.machine.run_with_stand_in("100", calls={"rename": rename}),
                         (0, ["library added", "not-moved 100 failed"]))
        self.assertOnlyInOwnLibrary()
        self.assertNothingInFamily()

    def test_a_copy_that_cannot_take_its_name_puts_the_manifest_back(self):
        self.machine.add_game(100)
        rename = self.renames_failing(lambda source, target: target == self.machine.shared / "common" / "A Game")
        self.assertEqual(self.machine.run_with_stand_in("100", calls={"rename": rename}),
                         (0, ["library added", "not-moved 100 failed"]))
        self.assertOnlyInOwnLibrary()
        self.assertNothingInFamily()
        self.assertNoLeftovers()

    def test_a_family_manifest_that_cannot_take_its_name_undoes_both(self):
        self.machine.add_game(100)
        rename = self.renames_failing(lambda source, target: target == self.machine.shared / "appmanifest_100.acf")
        self.assertEqual(self.machine.run_with_stand_in("100", calls={"rename": rename}),
                         (0, ["library added", "not-moved 100 failed"]))
        self.assertOnlyInOwnLibrary()
        self.assertNothingInFamily()
        self.assertNoLeftovers()

    def test_a_failed_undo_never_leaves_the_game_in_two_libraries(self):
        # The family manifest cannot take its name, and the copy cannot go
        # back into the run's own folder either: the temporary manifest goes
        # before the set-aside comes back, so no later run finishes it.
        self.machine.add_game(100)
        family_folder = self.machine.shared / "common" / "A Game"
        rename = self.renames_failing(lambda source, target: target == self.machine.shared / "appmanifest_100.acf"
                                      or source == family_folder)
        self.assertEqual(self.machine.run_with_stand_in("100", calls={"rename": rename}),
                         (0, ["library added", "not-moved 100 failed"]))
        self.assertEqual((self.machine.library / "appmanifest_100.acf").read_bytes(), manifest(100))
        self.assertFalse(os.path.lexists(self.machine.shared / "appmanifest_100.acf"))
        self.assertFalse(os.path.lexists(self.machine.shared / ".cairn-move-manifest-100"))
        # The copy stays without a manifest, and blocks the game with
        # "exists" until someone removes it.
        self.assertEqual(self.machine.run_with_stand_in(), (0, []))
        self.assertEqual((self.machine.library / "appmanifest_100.acf").read_bytes(), manifest(100))
        self.assertFalse(os.path.lexists(self.machine.shared / "appmanifest_100.acf"))
        self.assertEqual(self.machine.run_with_stand_in("100"), (0, ["not-moved 100 exists"]))

    def test_a_temporary_manifest_that_cannot_go_leaves_the_move_for_the_next_run(self):
        # As above, and the temporary manifest cannot be removed either:
        # everything stays as a kill would leave it, and the next run
        # finishes the move.
        self.machine.add_game(100)
        family_folder = self.machine.shared / "common" / "A Game"
        rename = self.renames_failing(lambda source, target: target == self.machine.shared / "appmanifest_100.acf"
                                      or source == family_folder)
        real_unlink = os.unlink

        def unlink(path, *arguments, **keywords):
            if pathlib.Path(path).name == ".cairn-move-manifest-100":
                raise OSError("read-only")
            real_unlink(path, *arguments, **keywords)

        self.assertEqual(self.machine.run_with_stand_in("100", calls={"rename": rename, "unlink": unlink}),
                         (0, ["library added", "not-moved 100 failed"]))
        self.assertFalse(os.path.lexists(self.machine.library / "appmanifest_100.acf"))
        self.assertFalse(os.path.lexists(self.machine.shared / "appmanifest_100.acf"))
        self.assertTrue((self.machine.library / ".cairn-move-aside-100").is_file())
        self.assertEqual(self.machine.run_with_stand_in(), (0, []))
        self.assertOnlyInFamily()
        self.assertNoLeftovers()

    def test_a_set_aside_manifest_that_cannot_come_back_comes_back_at_the_next_run(self):
        self.machine.add_game(100)
        rename = self.renames_failing(lambda source, target: target == self.machine.shared / "common" / "A Game"
                                      or target == self.machine.library / "appmanifest_100.acf")
        self.assertEqual(self.machine.run_with_stand_in("100", calls={"rename": rename}),
                         (0, ["library added", "not-moved 100 failed"]))
        self.assertTrue((self.machine.library / ".cairn-move-aside-100").is_file())
        self.assertNothingInFamily()
        self.assertEqual(self.machine.run_with_stand_in(), (0, []))
        self.assertOnlyInOwnLibrary()
        self.assertNoLeftovers()

    @needs_not_root
    def test_an_original_that_cannot_be_removed_is_still_moved(self):
        self.machine.add_game(100)
        (self.machine.library / "common").chmod(0o555)
        try:
            self.assertEqual(self.machine.run_with_stand_in("100"), (0, ["library added", "moved 100"]))
            self.assertTrue((self.machine.library / ".cairn-move-aside-100").is_file())
        finally:
            (self.machine.library / "common").chmod(0o755)
        self.assertEqual(self.machine.run_with_stand_in(), (0, []))
        self.assertOnlyInFamily()
        self.assertNoLeftovers()

    def test_another_drive(self):
        # The probe is asked once for each library: here the Guardian's own
        # reflinks, and the second does not.
        second = self.machine.second_library()
        self.machine.add_game(100, "First")
        self.machine.add_game(300, "Third")
        self.machine.add_game(200, "Second", library=second)
        probes = []

        def copy(source, target):
            if pathlib.Path(target).name == "probe":
                probes.append(pathlib.Path(source).parent)
                return pathlib.Path(source).parent == self.machine.library
            return copy_tree(source, target)

        code, lines = self.machine.run_with_stand_in("100", "200", "300", parts={"reflink_copy": copy})
        self.assertEqual((code, lines), (0, ["not-moved 200 other-drive", "library added", "moved 100", "moved 300"]))
        self.assertEqual(probes, [self.machine.library, second])
        self.assertTrue((second / "appmanifest_200.acf").is_file())


class ConditionsTest(MoveTestCase):
    def test_refuses_to_run_as_root(self):
        self.machine.add_game(100)
        (self.machine.shared / "common" / ".cairn-move-k2j4x9q1").mkdir()
        module = self.machine.load()
        before = self.everything()
        for name in ("geteuid", "getuid"):
            with self.subTest(name=name), mock.patch.object(module.os, name, return_value=0):
                self.assertEqual(self.machine.run_in_process(module, "100"), (2, ["refused root"]))
        self.assertEqual(self.everything(), before, "nothing changed, nothing recovered")

    def test_an_appid_that_is_not_a_plain_number_changes_nothing(self):
        self.machine.add_game(100)
        (self.machine.shared / "common" / ".cairn-move-k2j4x9q1").mkdir()
        before = self.everything()
        for bad in ("10a", "0100", "-5", "", " 100", "100/..", "../100", "12345678901", "١٢"):
            with self.subTest(appid=bad):
                self.assertEqual(self.machine.run("100", bad), ["refused appid"])
                self.assertEqual(self.machine.code, 2)
                self.assertEqual(self.everything(), before, "not even its leftovers go")

    def test_no_appids_is_not_a_mistake(self):
        self.machine.add_game(100)
        self.assertEqual(self.machine.run(), [])
        self.assertEqual(self.machine.code, 0)
        self.assertOnlyInOwnLibrary()

    def test_each_appid_once_in_number_order(self):
        self.assertEqual(self.machine.run("300", "100", "100", "20"),
                         ["not-moved 20 not-found", "not-moved 100 not-found", "not-moved 300 not-found"])

    def test_no_family_folder(self):
        self.machine.add_game(100)
        real = self.machine.shared
        linked = self.machine.root / "linked" / "steamapps"
        linked.parent.mkdir()
        linked.symlink_to(real)
        for shared in (self.machine.root / "missing" / "steamapps", linked):
            with self.subTest(shared=shared.parent.name):
                self.machine.shared = shared
                self.assertEqual(self.machine.run("100"), ["not-moved 100 no-shared"])
                self.assertEqual(self.machine.code, 0)
        self.machine.shared = real
        self.assertOnlyInOwnLibrary()

    @needs_not_root
    def test_a_family_folder_this_account_cannot_change(self):
        # A grown-up who is not in cairn-guardian.
        self.machine.add_game(100)
        for folder in (self.machine.shared, self.machine.shared / "common"):
            with self.subTest(folder=folder.name):
                folder.chmod(0o555)
                try:
                    self.assertEqual(self.machine.run("100"), ["not-moved 100 no-shared"])
                finally:
                    folder.chmod(0o755)
        self.assertOnlyInOwnLibrary()

    def test_no_steam_library_at_all(self):
        shutil.rmtree(self.machine.steam)
        self.assertEqual(self.machine.run("100"), ["not-moved 100 not-found"])
        self.assertEqual(self.machine.code, 0)

    def test_the_family_folder_in_the_lists_is_not_a_library_to_move_from(self):
        # As on the test VM, after the maintainer's Storage step; and a
        # library inside the family folder is not one either.
        self.machine.name_family()
        inside = self.machine.family / "inside" / "steamapps"
        (inside / "common").mkdir(parents=True)
        for name in LISTS:
            self.machine.write_list(name, self.machine.lists()[name][:-2] + library_entry(
                2, inside.parent, "6000000000000000002").encode() + b"}\n")
        self.machine.add_game(200, library=inside)
        (self.machine.shared / "appmanifest_100.acf").write_bytes(manifest(100))
        (self.machine.shared / "common" / "A Game").mkdir()
        before = self.everything()
        self.assertEqual(self.machine.run_with_stand_in("100", "200"), (0, ["shared 100", "not-moved 200 not-found"]))
        self.assertEqual(self.everything(), before)

    def test_a_listed_steamapps_or_common_that_is_a_link_is_passed_over(self):
        second = self.machine.root / "drives" / "second" / "steamapps"
        (second / "common").mkdir(parents=True)
        self.machine.add_game(100, library=second)
        linked = self.machine.root / "drives" / "linked"
        linked.mkdir()
        (linked / "steamapps").symlink_to(second)
        third = self.machine.root / "drives" / "third"
        (third / "steamapps").mkdir(parents=True)
        (third / "steamapps" / "common").symlink_to(second / "common")
        shutil.copy(second / "appmanifest_100.acf", third / "steamapps")
        for name in LISTS:
            self.machine.write_list(name, steam_list(library_entry(0, self.machine.steam, OWN_CONTENTID),
                                                     library_entry(1, linked, "1"), library_entry(2, third, "2")))
        self.assertEqual(self.machine.run_with_stand_in("100"), (0, ["not-moved 100 not-found"]))

    def test_the_home_comes_from_the_password_database(self):
        self.machine.add_game(100)
        module = self.machine.load()
        account = mock.Mock(pw_dir=str(self.machine.home))
        with (mock.patch.object(module.pwd, "getpwuid", return_value=account),
              mock.patch.dict(os.environ, {"HOME": str(self.machine.root / "elsewhere")}),
              mock.patch.object(module, "reflink_copy", copy_tree)):
            code, lines = self.machine.run_in_process(module, "100", home=False)
        self.assertEqual((code, lines), (0, ["library added", "moved 100"]))

    def test_anything_unexpected_says_failed_for_each_game_left(self):
        self.machine.add_game(300)
        module = self.machine.load()
        real_decide = module.decide

        def decide(appid, family, found):
            if appid == "200":
                raise RuntimeError(str(self.machine.home))
            return real_decide(appid, family, found)

        code, lines = self.machine.run_with_stand_in("100", "200", "300", module=module, parts={"decide": decide})
        self.assertEqual((code, lines), (1, ["not-moved 100 not-found", "not-moved 200 failed",
                                             "not-moved 300 failed"]))
        self.assertOnlyGrammar(lines)
        self.assertOnlyInOwnLibrary("300", data=manifest(300))

    def test_common_is_made_in_the_family_folder_when_missing(self):
        (self.machine.shared / "common").rmdir()
        self.machine.add_game(100)
        self.assertEqual(self.machine.run_with_stand_in("100"), (0, ["library added", "moved 100"]))
        self.assertOnlyInFamily()

    def test_a_second_run_waits_for_the_first(self):
        self.machine.add_game(100)
        lock = os.open(self.machine.lock, os.O_RDONLY)
        try:
            fcntl.flock(lock, fcntl.LOCK_EX)
            process = subprocess.Popen(self.machine.command("100"), stdin=subprocess.DEVNULL,
                                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            try:
                with self.assertRaises(subprocess.TimeoutExpired):
                    process.wait(timeout=1)
                self.assertTrue((self.machine.library / "appmanifest_100.acf").is_file(), "nothing moved meanwhile")
            finally:
                fcntl.flock(lock, fcntl.LOCK_UN)
            out, err = process.communicate(timeout=60)
        finally:
            os.close(lock)
        self.assertEqual(process.returncode, 0, err)
        self.assertIn(out.splitlines()[-1], ("moved 100", "not-moved 100 other-drive"))

    def test_a_run_holds_its_lock_from_recovery_to_the_last_removal(self):
        # A lock belongs to one opening of the file, so a second opening, in
        # this process too, cannot take it while the run holds it.
        self.machine.add_game(100)
        module = self.machine.load()
        seen = []

        def look_at_the_lock(step):
            descriptor = os.open(self.machine.lock, os.O_RDONLY)
            try:
                fcntl.flock(descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB)
                fcntl.flock(descriptor, fcntl.LOCK_UN)
                seen.append((step, "free"))
            except BlockingIOError:
                seen.append((step, "held"))
            finally:
                os.close(descriptor)

        def watched(step, real):
            def part(*arguments):
                look_at_the_lock(step)
                return real(*arguments)
            return part

        steps = ("recover_family_side", "recover_library_side", "put_in_place", "finish")
        parts = {step: watched(step, getattr(module, step)) for step in steps}
        parts["reflink_copy"] = watched("reflink_copy", copy_tree)
        self.assertEqual(self.machine.run_with_stand_in("100", module=module, parts=parts),
                         (0, ["library added", "moved 100"]))
        # The probe, then the game's copy.
        self.assertEqual(seen, [(step, "held") for step in ("recover_family_side", "recover_library_side",
                                                             "reflink_copy", "reflink_copy", "put_in_place",
                                                             "finish")])

    def test_the_lock_is_a_file_only_guardians_can_open(self):
        # Every account can open the family folder's folders, and a lock
        # needs only an opened file, so the lock is a file of its own that
        # other accounts cannot open: no child can hold it and keep a Save
        # waiting. The first run makes it, and every later run takes it.
        for image in (False, True):
            with self.subTest(image=image):
                self.fresh_machine()
                self.machine.lock.unlink()
                if image and shutil.which("setfacl"):
                    # The image's default ACL, which takes the umask's place.
                    subprocess.run(["setfacl", "-m", "d:u::rwx,d:g::rwx,d:m::rwx,d:o::r-x", str(self.machine.shared)],
                                   capture_output=True, check=False)
                self.assertEqual(self.machine.run(), [])
                made = self.machine.lock.lstat()
                self.assertTrue(stat.S_ISREG(made.st_mode))
                self.assertEqual(made.st_mode & 0o007, 0, "other accounts cannot open it")
                self.assertEqual(made.st_mode & 0o040, 0o040, "every Guardian can open it")
                self.assertEqual(self.machine.run(), [])
                self.assertEqual(self.machine.lock.lstat().st_ino, made.st_ino, "made once")


class PlantedLinksTest(MoveTestCase):
    """Any Guardian can make entries in the family folder. A link under a
    name the program uses is never followed, and never sends a write into a
    home: it is removed, or, at the lock's name, the run stops. Copies use a
    stand-in for cp."""

    def setUp(self):
        super().setUp()
        self.bashrc = self.machine.home / ".bashrc"
        self.bashrc.write_bytes(b"the Guardian's own\n")
        self.notes = self.machine.home / "notes"
        self.notes.mkdir()
        (self.notes / "keep").write_bytes(b"mine")
        self.machine.add_game(100)

    def assertHomeUntouched(self):
        self.assertEqual(self.bashrc.read_bytes(), b"the Guardian's own\n")
        self.assertEqual(files_of(self.notes), {"keep": b"mine"})

    def test_links_under_its_names_are_removed_and_never_followed(self):
        planted = [self.machine.shared / ".cairn-move-manifest-100", self.machine.shared / "common" / ".cairn-move-x",
                   self.machine.family / ".cairn-marker-new", self.machine.steam / "config" / ".cairn-list-new"]
        for path, target in zip(planted, (self.bashrc, self.notes, self.bashrc, self.bashrc)):
            path.symlink_to(target)
        self.assertEqual(self.machine.run_with_stand_in("100"), (0, ["library added", "moved 100"]))
        for path in planted:
            self.assertFalse(os.path.lexists(path), path.name)
        self.assertHomeUntouched()

    def test_a_link_planted_after_the_clean_up_is_never_written_through(self):
        # As if each came just after recovery removed what it found.
        for place, line in ((lambda: self.machine.shared / ".cairn-move-manifest-100", "not-moved 100 failed"),
                            (lambda: self.machine.family / ".cairn-marker-new", "not-moved 100 unreadable-config"),
                            (lambda: self.machine.steam / "config" / ".cairn-list-new",
                             "not-moved 100 unreadable-config")):
            self.fresh_machine()
            path = place()
            with self.subTest(name=path.name):
                path.symlink_to(self.bashrc)
                code, lines = self.machine.run_with_stand_in("100", parts={"recover": lambda *arguments: None})
                self.assertEqual((code, lines[-1]), (0, line))
                self.assertTrue(path.is_symlink(), "never written through, nor taken for a file")
                self.assertHomeUntouched()
                self.assertOnlyInOwnLibrary()

    def test_a_lock_that_is_not_a_plain_file_moves_nothing(self):
        # A link there is never followed, so nothing is made where it
        # points, and a pipe never keeps the run waiting.
        lock = self.machine.lock
        made_through_a_link = self.machine.home / "made-through-a-link"
        for kind, make in (("link", lambda: lock.symlink_to(made_through_a_link)), ("folder", lock.mkdir),
                           ("pipe", lambda: os.mkfifo(lock))):
            with self.subTest(kind=kind):
                lock.rmdir() if lock.is_dir() and not lock.is_symlink() else lock.unlink()
                make()
                before = self.everything()
                self.assertEqual(self.machine.run("100"), ["not-moved 100 failed"])
                self.assertEqual(self.machine.code, 1)
                self.assertEqual(self.everything(), before)
                self.assertFalse(os.path.lexists(made_through_a_link))
                self.assertHomeUntouched()

    def test_a_marker_that_is_a_link_moves_nothing(self):
        real = self.machine.root / "real.vdf"
        real.write_bytes(marker(FAMILY_CONTENTID))
        (self.machine.family / "libraryfolder.vdf").symlink_to(real)
        self.assertEqual(self.machine.run_with_stand_in("100"), (0, ["not-moved 100 unreadable-config"]))
        self.assertEqual(real.read_bytes(), marker(FAMILY_CONTENTID))


class NoReflinkTest(unittest.TestCase):
    """On tmpfs, which cannot reflink, a game stays where it is."""

    @unittest.skipUnless(pathlib.Path("/dev/shm").is_dir(), "no /dev/shm")
    def test_other_drive_without_reflink(self):
        with tempfile.TemporaryDirectory(dir="/dev/shm") as root:
            if reflinks(root):
                self.skipTest("/dev/shm can reflink here")
            machine = Machine(root)
            machine.add_game(100)
            before = tree(machine.root / "home"), tree(machine.family, {machine.shared / "common"})
            self.assertEqual(machine.run("100"), ["not-moved 100 other-drive"], machine.stderr)
            self.assertEqual(machine.code, 0)
            self.assertEqual((tree(machine.root / "home"), tree(machine.family, {machine.shared / "common"})), before)


if __name__ == "__main__":
    if REFLINK_PLACE is None:
        print(f"note: the tests that move by reflink are skipped: {NO_REFLINK}", file=sys.stderr)
    unittest.main()
