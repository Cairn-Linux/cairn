#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Tests for kidscan against a synthetic Steam library. Standard library only.

Run:  python3 tools/kidscan/tests/test_kidscan.py
"""

import io
import json
import os
import stat
import sys
import tempfile
import textwrap
import time
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import kidscan  # noqa: E402


def acf(appid, name, installdir):
    return textwrap.dedent(f'''\
        "AppState"
        {{
        \t"appid"\t\t"{appid}"
        \t"Universe"\t\t"1"
        \t"name"\t\t"{name}"
        \t"StateFlags"\t\t"4"
        \t"installdir"\t\t"{installdir}"
        }}
        ''')


def write_fake_scummvm(path):
    """A stand-in for `scummvm --detect --path=<dir>` that knows Freddi Fish 1."""
    path.write_text(textwrap.dedent('''\
        #!/bin/sh
        # Mimics `scummvm --detect --path=<dir>`: a three-column table where a
        # long description runs into the Full Path column with one space.
        # Like the real binary, it refuses to start without a video device.
        if [ "$SDL_VIDEODRIVER" != dummy ]; then
          echo "Could not initialize SDL: No available video device!" >&2
          exit 1
        fi
        p=""
        for a in "$@"; do case "$a" in --path=*) p="${a#--path=}";; esac; done
        case "$p" in
          *"Freddi Fish 1")
            echo "GameID                         Description                                        Full Path"
            echo "------------------------------ -------------------------------------------------- ---------"
            echo "scumm:freddi                   Freddi Fish and the Case of the Missing Kelp Seeds $p"
            echo "scumm:freddi-demo              Freddi Fish (Demo)                                 $p/demo"
            ;;
          *) exit 0 ;;
        esac
        '''))
    path.chmod(path.stat().st_mode | stat.S_IEXEC)
    return path


def raw_acf(appid='"100"', name='"A Game"', flags='"4"', installdir='"A Game"', extra=""):
    """A manifest from VDF tokens, so a test can put a block where a string
    belongs."""
    return (
        '"AppState"\n{\n'
        f'\t"appid"\t\t{appid}\n'
        f'\t"name"\t\t{name}\n'
        f'\t"StateFlags"\t\t{flags}\n'
        f'\t"installdir"\t\t{installdir}\n'
        f"{extra}"
        "}\n"
    )


class FakeLibrary:
    """Two Steam library folders under one root, populated with edge cases."""

    def __init__(self, base: Path):
        self.root = base / "Steam"
        self.lib2 = base / "drive2" / "SteamLibrary"
        for lib in (self.root, self.lib2):
            (lib / "steamapps" / "common").mkdir(parents=True)

        (self.root / "steamapps" / "libraryfolders.vdf").write_text(textwrap.dedent(f'''\
            "libraryfolders"
            {{
            \t"contentstatsid"\t\t"-123456789"
            \t"0"
            \t{{
            \t\t"path"\t\t"{self.root}"
            \t\t"label"\t\t""
            \t}}
            \t"1"
            \t{{
            \t\t"path"\t\t"{self.lib2}"
            \t}}
            }}
            '''))

        # A real game with an escaped quote in its name and a space in its directory.
        self.add(self.root, "294660", 'Freddi Fish \\"Kelp\\" Seeds', "Freddi Fish 1")
        # Denylisted by appid, directory present.
        self.add(self.root, "228980", "Steamworks Common Redistributables", "Steamworks Shared")
        # Denylisted by name pattern only.
        self.add(self.root, "2805730", "Proton 9.0 (Beta)", "Proton 9.0 (Beta)")
        # Manifest present, install directory missing: must be skipped.
        (self.root / "steamapps" / "appmanifest_999.acf").write_text(acf("999", "Ghost", "Ghost"))
        # Same title in both libraries: must appear once.
        self.add(self.root, "400", "Portal", "Portal")
        self.add(self.lib2, "400", "Portal", "Portal")
        # Only in the second library.
        self.add(self.lib2, "620", "Portal 2", "Portal 2")

    @staticmethod
    def add(lib, appid, name, installdir):
        (lib / "steamapps" / f"appmanifest_{appid}.acf").write_text(acf(appid, name, installdir))
        (lib / "steamapps" / "common" / installdir).mkdir(parents=True, exist_ok=True)


class KidscanTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.base = Path(self.tmp.name)
        self.lib = FakeLibrary(self.base)
        self._roots = kidscan.STEAM_ROOTS
        kidscan.STEAM_ROOTS = []  # never touch the real home directory

    def tearDown(self):
        kidscan.STEAM_ROOTS = self._roots
        self.tmp.cleanup()

    # --- VDF ---------------------------------------------------------------

    def test_parse_vdf_handles_nesting_comments_and_escapes(self):
        data = kidscan.parse_vdf('''
            "AppState" // trailing comment
            {
                "name"  "Putt-Putt \\"Deluxe\\""
                "nested" { "k" "v" "k" "last wins" }
            }
        ''')
        self.assertEqual(data["AppState"]["name"], 'Putt-Putt \\"Deluxe\\"')
        self.assertEqual(data["AppState"]["nested"]["k"], "last wins")

    def test_parse_vdf_rejects_unbalanced(self):
        with self.assertRaises(ValueError):
            kidscan.parse_vdf('"a" { "b" "c"')
        with self.assertRaises(ValueError):
            kidscan.parse_vdf('{ "b" "c" }')

    def test_parse_vdf_refuses_a_string_that_never_closes(self):
        for text in ('"a" "b', '"a" "b\\', '"a" { "b" "c\\"', '"'):
            with self.subTest(text=text), self.assertRaises(ValueError):
                kidscan.parse_vdf(text)

    def test_parse_vdf_takes_a_backslash_before_any_character(self):
        # A backslash at the end of a line keeps the line break in the string.
        self.assertEqual(kidscan.parse_vdf('"a" "b\\\nc"'), {"a": "b\\\nc"})

    def test_parse_vdf_ends_quickly_on_a_hostile_file(self):
        # Root reads the shared folder's manifests, up to MANIFEST_LIMIT, and
        # every Guardian can write there. A tokenizer that searches again
        # from every quote takes about an hour on the first of these.
        half = kidscan.MANIFEST_LIMIT // 2
        for text in ('"' + '\\"' * half, '"' + '\\\n' * half, '"a"\n' + '"' + '\\"' * (half - 3) + "\\"):
            with self.subTest(text=text[:8]):
                started = time.monotonic()
                with self.assertRaises(ValueError):
                    kidscan.parse_vdf(text)
                self.assertLess(time.monotonic() - started, 0.5)

    def test_ci_get_is_case_insensitive(self):
        self.assertEqual(kidscan.ci_get({"AppID": "1"}, "appid"), "1")
        self.assertIsNone(kidscan.ci_get({}, "appid"))

    # --- discovery ---------------------------------------------------------

    def test_find_steam_roots_dedupes_symlinks_and_ignores_missing(self):
        link = self.base / "steam-link"
        link.symlink_to(self.lib.root)
        roots = kidscan.find_steam_roots([str(link), str(self.lib.root), str(self.base / "nope")])
        self.assertEqual(roots, [self.lib.root.resolve()])

    def test_library_paths_follows_libraryfolders(self):
        paths = kidscan.library_paths(self.lib.root)
        self.assertEqual(paths, [self.lib.root / "steamapps", self.lib.lib2 / "steamapps"])

    def test_installed_apps_filters_denylist_proton_and_missing_dirs(self):
        apps = {a["appid"]: a for a in kidscan.installed_apps(self.lib.root / "steamapps")}
        self.assertEqual(set(apps), {"294660", "400"})
        self.assertEqual(apps["294660"]["path"], self.lib.root / "steamapps" / "common" / "Freddi Fish 1")

    # --- manifest ----------------------------------------------------------

    def test_build_entries_without_scummvm_routes_through_steam_and_dedupes(self):
        entries = kidscan.build_entries([self.lib.root], None, {"400": "puzzle"})
        titles = [e["title"] for e in entries]
        self.assertEqual(titles, ['Freddi Fish \\"Kelp\\" Seeds', "Portal", "Portal 2"])
        portal = next(e for e in entries if e["title"] == "Portal")
        self.assertEqual(portal["exec"], ["steam", "-applaunch", "400"])
        self.assertEqual(portal["category"], "puzzle")
        portal_2 = next(e for e in entries if e["title"] == "Portal 2")
        self.assertEqual(portal_2["category"], "games")
        self.assertTrue(portal["needs_steam_running"])
        self.assertTrue(all(e["engine"] == "steam" for e in entries))

    def test_scummvm_detect_parses_table_and_yields_one_tile_per_target(self):
        fake = write_fake_scummvm(self.base / "scummvm")
        freddi = self.lib.root / "steamapps" / "common" / "Freddi Fish 1"

        found = kidscan.scummvm_detect(freddi, str(fake))
        self.assertEqual([t for t, _ in found], ["scumm:freddi", "scumm:freddi-demo"])

        entries = kidscan.build_entries([self.lib.root], str(fake), {})
        native = {e["exec"][-1]: e for e in entries if e["engine"] == "scummvm"}
        self.assertEqual(set(native), {"scumm:freddi", "scumm:freddi-demo"})
        full = native["scumm:freddi"]
        # The whole argv: ScummVM on Linux exits at once on an option it does
        # not know, such as the Windows-only --no-console (#97).
        self.assertEqual(full["exec"], [str(fake), "--fullscreen", f"--path={freddi}", "scumm:freddi"])
        self.assertEqual(full["category"], "games")
        self.assertFalse(full["needs_steam_running"])
        self.assertEqual(full["source"], {"store": "steam", "appid": "294660"})
        # Multiple detections in one Steam app use the detected description as the title,
        # and entries sort by title, so the demo comes first.
        self.assertTrue(full["title"].startswith("Freddi Fish and the Case"))
        self.assertEqual([e["title"] for e in entries][:2],
                         ["Freddi Fish (Demo)", "Freddi Fish and the Case of the Missing Kelp Seeds"])

    def test_scummvm_detect_warns_and_routes_through_steam_when_detect_fails(self):
        fake = self.base / "scummvm"
        fake.write_text(textwrap.dedent('''\
            #!/bin/sh
            echo "Could not initialize SDL: No available video device!" >&2
            exit 1
            '''))
        fake.chmod(fake.stat().st_mode | stat.S_IEXEC)

        err = io.StringIO()
        with redirect_stderr(err):
            found = kidscan.scummvm_detect(self.lib.root / "steamapps" / "common" / "Freddi Fish 1", str(fake))
            entries = kidscan.build_entries([self.lib.root], str(fake), {})
        self.assertEqual(found, [])
        self.assertTrue(all(e["engine"] == "steam" for e in entries))
        self.assertIn("scummvm --detect exited 1", err.getvalue())
        self.assertIn("No available video device", err.getvalue())

    def test_write_desktop_files_quotes_paths_with_spaces(self):
        outdir = self.base / "apps"
        entry = {
            "id": "freddi", "title": "Freddi Fish", "category": "games", "engine": "scummvm",
            "exec": ["/usr/bin/scummvm", "--path=/games/Freddi Fish 1", "scumm:freddi"],
        }
        written = kidscan.write_desktop_files([entry], outdir)
        self.assertEqual([p.name for p in written], ["kid-freddi.desktop"])
        text = written[0].read_text()
        self.assertIn('Exec=/usr/bin/scummvm "--path=/games/Freddi Fish 1" scumm:freddi', text)
        self.assertIn("X-Kid-Engine=scummvm", text)

    # --- CLI ---------------------------------------------------------------

    def test_main_end_to_end(self):
        out = self.base / "out" / "games.json"
        desktop = self.base / "desktop"
        stdout, stderr = io.StringIO(), io.StringIO()
        with redirect_stdout(stdout), redirect_stderr(stderr):
            rc = kidscan.main([
                "--steam-root", str(self.lib.root), "--no-scummvm",
                "-o", str(out), "--desktop-dir", str(desktop),
            ])
        self.assertEqual(rc, 0, stderr.getvalue())
        manifest = json.loads(out.read_text())
        self.assertEqual(manifest["version"], kidscan.MANIFEST_VERSION)
        self.assertIsNone(manifest["scummvm"])
        self.assertEqual(len(manifest["entries"]), 3)
        self.assertEqual(len(list(desktop.glob("kid-*.desktop"))), 3)
        self.assertIn("3 tiles (0 native ScummVM, 3 via Steam)", stderr.getvalue())

    def test_main_returns_2_when_no_steam(self):
        with redirect_stderr(io.StringIO()):
            self.assertEqual(kidscan.main(["--steam-root", str(self.base / "nothing")]), 2)

    # --- ids (#24) ---------------------------------------------------------

    def test_ids_come_from_the_appid_and_the_scummvm_target(self):
        fake = write_fake_scummvm(self.base / "scummvm")
        entries = kidscan.build_entries([self.lib.root], str(fake), {})
        self.assertEqual(sorted(e["id"] for e in entries), [
            "steam-294660-scumm-freddi", "steam-294660-scumm-freddi-demo", "steam-400", "steam-620",
        ])
        through_steam = kidscan.build_entries([self.lib.root], None, {})
        self.assertEqual(sorted(e["id"] for e in through_steam), ["steam-294660", "steam-400", "steam-620"])

    def test_two_games_with_one_title_keep_their_own_ids(self):
        FakeLibrary.add(self.lib.lib2, "4000", "Portal", "Portal Again")
        entries = kidscan.build_entries([self.lib.root], None, {})
        ids = [e["id"] for e in entries]
        self.assertEqual(len(ids), len(set(ids)))
        self.assertIn("steam-4000", ids)

    # --- --library ---------------------------------------------------------

    def run_main(self, *arguments):
        stdout, stderr = io.StringIO(), io.StringIO()
        with redirect_stdout(stdout), redirect_stderr(stderr):
            code = kidscan.main(["--no-scummvm", *arguments])
        self.assertEqual(code, 0, stderr.getvalue())
        return json.loads(stdout.getvalue())

    def test_library_reads_only_the_folders_named(self):
        # A Steam in this HOME, which --library must not read.
        home_steam = self.base / "home" / ".local" / "share" / "Steam"
        (home_steam / "steamapps").mkdir(parents=True)
        FakeLibrary.add(home_steam, "777", "Home Game", "Home Game")
        kidscan.STEAM_ROOTS = [str(home_steam)]
        without = self.run_main("--steam-root", str(self.lib.lib2))
        self.assertIn("777", [e["source"]["appid"] for e in without["entries"]])
        manifest = self.run_main("--library", str(self.lib.lib2))
        self.assertEqual(sorted(e["source"]["appid"] for e in manifest["entries"]), ["400", "620"])
        self.assertEqual(manifest["steam_roots"], [str(self.lib.lib2.resolve())])

    def test_library_does_not_follow_libraryfolders_vdf(self):
        # The root's libraryfolders.vdf lists lib2, which holds Portal 2.
        manifest = self.run_main("--library", str(self.lib.root))
        self.assertEqual(sorted(e["source"]["appid"] for e in manifest["entries"]), ["294660", "400"])

    def test_library_is_repeatable(self):
        manifest = self.run_main("--library", str(self.lib.root), "--library", str(self.lib.lib2))
        self.assertEqual(sorted(e["source"]["appid"] for e in manifest["entries"]), ["294660", "400", "620"])

    def test_library_and_steam_root_together_are_refused(self):
        stderr = io.StringIO()
        with redirect_stderr(stderr), self.assertRaises(SystemExit) as refused:
            kidscan.main(["--library", str(self.lib.root), "--steam-root", str(self.lib.lib2)])
        self.assertEqual(refused.exception.code, 2)
        self.assertIn("not allowed with", stderr.getvalue())

    def test_a_library_without_steamapps_is_no_steam(self):
        with redirect_stderr(io.StringIO()):
            self.assertEqual(kidscan.main(["--library", str(self.base / "nothing")]), 2)

    def link_steamapps(self, library):
        """library's steamapps moved aside, and a link to it in its place."""
        steamapps = library / "steamapps"
        moved = self.base / "moved-steamapps"
        steamapps.rename(moved)
        steamapps.symlink_to(moved)

    def test_a_library_whose_steamapps_is_a_link_lists_no_games(self):
        self.link_steamapps(self.lib.lib2)
        stdout, stderr = io.StringIO(), io.StringIO()
        with redirect_stdout(stdout), redirect_stderr(stderr):
            code = kidscan.main(["--no-scummvm", "--library", str(self.lib.root), "--library", str(self.lib.lib2)])
        self.assertEqual(code, 2)
        self.assertEqual(stdout.getvalue(), "")
        self.assertIn("steamapps is a link, so it is not read; no games are listed", stderr.getvalue())

    def test_a_linked_library_writes_no_file(self):
        self.link_steamapps(self.lib.lib2)
        out = self.base / "games.json"
        out.write_text("the last run's\n")
        with redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
            code = kidscan.main(["--no-scummvm", "--library", str(self.lib.lib2), "-o", str(out)])
        self.assertEqual(code, 2)
        self.assertEqual(out.read_text(), "the last run's\n")

    def test_a_steam_root_whose_steamapps_is_a_link_is_still_read(self):
        # A family's own Steam may keep its games elsewhere; only --library,
        # which provisioning runs as root, refuses the link.
        self.link_steamapps(self.lib.lib2)
        manifest = self.run_main("--steam-root", str(self.lib.lib2))
        self.assertEqual(sorted(e["source"]["appid"] for e in manifest["entries"]), ["400", "620"])

    # --- --pictures --------------------------------------------------------

    def test_a_picture_is_named_only_when_it_is_a_plain_file(self):
        pictures = self.base / "pictures"
        pictures.mkdir()
        (pictures / "400.jpg").write_bytes(b"\xff\xd8\xff stand-in")
        elsewhere = self.base / "elsewhere.jpg"
        elsewhere.write_bytes(b"\xff\xd8\xff stand-in")
        (pictures / "620.jpg").symlink_to(elsewhere)
        (pictures / "294660.jpg").mkdir()  # a folder is not a picture either
        manifest = self.run_main("--steam-root", str(self.lib.root), "--pictures", str(pictures))
        named = {e["source"]["appid"]: e.get("picture") for e in manifest["entries"]}
        self.assertEqual(named, {"294660": None, "400": str(pictures / "400.jpg"), "620": None})
        self.assertNotIn("picture", next(e for e in manifest["entries"] if e["source"]["appid"] == "620"))

    def test_a_picture_path_is_absolute(self):
        pictures = self.base / "pictures"
        pictures.mkdir()
        (pictures / "400.jpg").write_bytes(b"\xff\xd8\xff stand-in")
        here = os.getcwd()
        os.chdir(self.base)
        try:
            manifest = self.run_main("--library", str(self.lib.root), "--pictures", "pictures")
        finally:
            os.chdir(here)
        portal = next(e for e in manifest["entries"] if e["source"]["appid"] == "400")
        self.assertEqual(portal["picture"], str(pictures / "400.jpg"))

    def test_without_pictures_no_entry_has_one(self):
        manifest = self.run_main("--steam-root", str(self.lib.root))
        self.assertFalse(any("picture" in e for e in manifest["entries"]))

    # --- -o ----------------------------------------------------------------

    def write_output(self, out):
        with redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
            return kidscan.main(["--steam-root", str(self.lib.root), "--no-scummvm", "-o", str(out)])

    def test_output_is_mode_0644_whatever_the_umask(self):
        out = self.base / "games.json"
        old = os.umask(0o077)
        try:
            self.assertEqual(self.write_output(out), 0)
        finally:
            os.umask(old)
        self.assertEqual(stat.S_IMODE(out.stat().st_mode), 0o644)
        self.assertEqual(json.loads(out.read_text())["version"], kidscan.MANIFEST_VERSION)

    def test_output_that_cannot_be_put_in_place_leaves_the_old_file(self):
        out = self.base / "games.json"
        out.write_text("the last run's\n")
        with mock.patch.object(kidscan.os, "replace", side_effect=OSError("no room")):
            with self.assertRaises(OSError):
                self.write_output(out)
        self.assertEqual(out.read_text(), "the last run's\n")
        self.assertEqual([p.name for p in self.base.iterdir() if p.name.startswith(".")], [])

    def test_output_replaces_a_link_rather_than_writing_through_it(self):
        target = self.base / "someone-elses.txt"
        target.write_text("untouched\n")
        out = self.base / "games.json"
        out.symlink_to(target)
        self.assertEqual(self.write_output(out), 0)
        self.assertFalse(out.is_symlink())
        self.assertEqual(target.read_text(), "untouched\n")
        self.assertEqual([p.name for p in self.base.iterdir() if p.name.startswith(".")], [])


class ManifestRulesTests(unittest.TestCase):
    """What an appmanifest must say for its game to get a tile, in every
    mode (ADR-0033). One library, one game folder, one manifest at a time."""

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.steamapps = Path(self.tmp.name) / "Steam" / "steamapps"
        for folder in ("A Game", "a/b"):
            (self.steamapps / "common" / folder).mkdir(parents=True)
        self._roots = kidscan.STEAM_ROOTS
        kidscan.STEAM_ROOTS = []

    def tearDown(self):
        kidscan.STEAM_ROOTS = self._roots
        self.tmp.cleanup()

    def tiles(self, text, file_appid="100"):
        """The appids that get a tile from one manifest."""
        manifest = self.steamapps / f"appmanifest_{file_appid}.acf"
        manifest.write_text(text)
        try:
            return [app["appid"] for app in kidscan.installed_apps(self.steamapps)]
        finally:
            manifest.unlink()

    def test_a_plain_installed_game_gets_a_tile(self):
        self.assertEqual(self.tiles(raw_acf()), ["100"])

    def test_state_flags_need_fully_installed(self):
        # 6 and 1030 are installed with an update waiting: the game still runs.
        for flags, kept in (("4", True), ("6", True), ("1030", True), ("1026", False), ("2", False),
                            ("0", False), ("abc", False), ("", False), (" 4", False), ("-4", False)):
            with self.subTest(flags=flags):
                self.assertEqual(self.tiles(raw_acf(flags=f'"{flags}"')), ["100"] if kept else [])

    def test_a_manifest_that_is_not_a_plain_file_is_skipped(self):
        # Root reads the shared library's manifests: a link must not make it
        # read another file, and a FIFO must not make it wait.
        target = Path(self.tmp.name) / "elsewhere.acf"
        target.write_text(raw_acf())
        link = self.steamapps / "appmanifest_100.acf"
        link.symlink_to(target)
        self.assertEqual(list(kidscan.installed_apps(self.steamapps)), [])
        link.unlink()
        os.mkfifo(link)
        self.assertEqual(list(kidscan.installed_apps(self.steamapps)), [])
        link.unlink()

    def test_a_manifest_over_the_limit_is_skipped(self):
        padding = "\t// " + "x" * kidscan.MANIFEST_LIMIT + "\n"
        self.assertEqual(self.tiles(raw_acf(extra=padding)), [])

    def test_state_flags_missing_or_a_block(self):
        self.assertEqual(self.tiles(raw_acf(flags='{ "a" "b" }')), [])
        text = raw_acf().replace('\t"StateFlags"\t\t"4"\n', "")
        self.assertEqual(self.tiles(text), [])

    def test_an_appid_that_is_not_a_number(self):
        for appid in ("abc", "10a", "0100", "0", "", "12345678901"):
            with self.subTest(appid=appid):
                self.assertEqual(self.tiles(raw_acf(appid=f'"{appid}"'), file_appid=appid or "x"), [])

    def test_an_appid_that_is_not_its_files(self):
        self.assertEqual(self.tiles(raw_acf(appid='"200"'), file_appid="100"), [])

    def test_an_appid_that_is_a_block_leaves_the_game_out_and_the_scan_goes_on(self):
        (self.steamapps / "appmanifest_100.acf").write_text(raw_acf(appid='{ "a" "b" }'))
        (self.steamapps / "appmanifest_200.acf").write_text(raw_acf(appid='"200"'))
        self.assertEqual([app["appid"] for app in kidscan.installed_apps(self.steamapps)], ["200"])

    def test_any_value_that_is_a_block_leaves_the_game_out(self):
        for field in ("name", "installdir"):
            with self.subTest(field=field):
                self.assertEqual(self.tiles(raw_acf(**{field: '{ "a" "b" }'})), [])
        self.assertEqual(self.tiles('"AppState" "a string"\n'), [])
        self.assertEqual(self.tiles('"Other" { "appid" "100" }\n'), [])

    def test_an_installdir_that_is_not_one_plain_folder_name(self):
        # The folder is there, so only the name rule can leave the game out.
        (self.steamapps / "common" / ".cairn-copy-100").mkdir()
        for installdir in ("..", ".", "a/b", "", ".cairn-copy-100"):
            with self.subTest(installdir=installdir):
                self.assertEqual(self.tiles(raw_acf(installdir=f'"{installdir}"')), [])

    def test_a_game_on_a_beta_branch(self):
        for block in ("UserConfig", "MountedConfig"):
            with self.subTest(block=block):
                beta = f'\t"{block}"\n\t{{\n\t\t"BetaKey"\t\t"beta"\n\t}}\n'
                self.assertEqual(self.tiles(raw_acf(extra=beta)), [])
                public = f'\t"{block}"\n\t{{\n\t\t"BetaKey"\t\t"public"\n\t}}\n'
                self.assertEqual(self.tiles(raw_acf(extra=public)), ["100"])
                odd = f'\t"{block}"\t\t"not a block"\n'
                self.assertEqual(self.tiles(raw_acf(extra=odd)), [])

    def test_the_rules_are_the_copy_steps_own(self):
        # The copy step reads these from here, so the two cannot drift.
        self.assertTrue(kidscan.APPID.fullmatch("294660"))
        self.assertFalse(kidscan.APPID.fullmatch("0294660"))
        self.assertFalse(kidscan.plain_folder_name(".."))
        self.assertTrue(kidscan.on_beta_branch({"UserConfig": {"BetaKey": "beta"}}))

    def test_a_block_appid_never_stops_main(self):
        (self.steamapps / "appmanifest_100.acf").write_text(raw_acf(appid='{ "a" "b" }'))
        (self.steamapps / "appmanifest_200.acf").write_text(raw_acf(appid='"200"'))
        stdout = io.StringIO()
        with redirect_stdout(stdout), redirect_stderr(io.StringIO()):
            code = kidscan.main(["--no-scummvm", "--library", str(self.steamapps.parent)])
        self.assertEqual(code, 0)
        self.assertEqual([e["id"] for e in json.loads(stdout.getvalue())["entries"]], ["steam-200"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
