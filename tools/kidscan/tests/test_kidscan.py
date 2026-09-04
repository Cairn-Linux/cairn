#!/usr/bin/env python3
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
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path

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
        self.assertTrue(portal["needs_steam_running"])
        self.assertTrue(all(e["engine"] == "steam" for e in entries))

    def test_scummvm_detect_parses_table_and_yields_one_tile_per_target(self):
        fake = self.base / "scummvm"
        fake.write_text(textwrap.dedent('''\
            #!/bin/sh
            # Mimics `scummvm --detect --path=<dir>`: a three-column table where a
            # long description runs into the Full Path column with one space.
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
        fake.chmod(fake.stat().st_mode | stat.S_IEXEC)

        found = kidscan.scummvm_detect(self.lib.root / "steamapps" / "common" / "Freddi Fish 1", str(fake))
        self.assertEqual([t for t, _ in found], ["scumm:freddi", "scumm:freddi-demo"])

        entries = kidscan.build_entries([self.lib.root], str(fake), {})
        native = {e["exec"][-1]: e for e in entries if e["engine"] == "scummvm"}
        self.assertEqual(set(native), {"scumm:freddi", "scumm:freddi-demo"})
        full = native["scumm:freddi"]
        self.assertEqual(full["exec"][0], str(fake))
        self.assertIn("--fullscreen", full["exec"])
        self.assertFalse(full["needs_steam_running"])
        self.assertEqual(full["source"], {"store": "steam", "appid": "294660"})
        # Multiple detections in one Steam app use the detected description as the title,
        # and entries sort by title, so the demo comes first.
        self.assertTrue(full["title"].startswith("Freddi Fish and the Case"))
        self.assertEqual([e["title"] for e in entries][:2],
                         ["Freddi Fish (Demo)", "Freddi Fish and the Case of the Missing Kelp Seeds"])

    def test_write_desktop_files_quotes_paths_with_spaces(self):
        outdir = self.base / "apps"
        entry = {
            "id": "freddi", "title": "Freddi Fish", "category": "play", "engine": "scummvm",
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


if __name__ == "__main__":
    unittest.main(verbosity=2)
