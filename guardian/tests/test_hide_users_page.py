#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""build_files/hide-users-page.sh (ADR-0043): Plasma's Users page hidden
with KDE Kiosk in /etc/xdg/kdeglobals, merged into the file Bazzite ships
rather than written over it."""

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parents[2] / "build_files" / "hide-users-page.sh"

# /etc/xdg/kdeglobals as Bazzite ships it, read in the cairn-iso VM on
# 2026-10-29.
BAZZITE = """[KDE]
LookAndFeelPackage=com.valve.vapor.desktop
SingleClick=false

[KDE Control Module Restrictions][$i]
kcm_updates=false

[General]
fixed=Fira Code,10,-1,5,50,0,0,0,0,0
font=Noto Sans,10,-1,5,50,0,0,0,0,0
"""


class HideUsersPageTest(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.file = Path(self.folder.name) / "kdeglobals"

    def tearDown(self):
        self.folder.cleanup()

    def run_script(self):
        subprocess.run(["bash", str(SCRIPT), str(self.file)], check=True,
                       capture_output=True)
        return self.file.read_text()

    def read_key(self):
        """What KDE's own reader makes of the file, when it is here."""
        reader = shutil.which("kreadconfig6")
        if reader is None:
            return None
        return subprocess.run(
            [reader, "--file", str(self.file), "--group", "KDE Control Module Restrictions",
             "--key", "kcm_users"], check=True, capture_output=True, text=True).stdout.strip()

    def test_joins_bazzites_own_group_and_keeps_every_other_line(self):
        self.file.write_text(BAZZITE)
        after = self.run_script()
        self.assertEqual(after, BAZZITE.replace(
            "[KDE Control Module Restrictions][$i]\n",
            "[KDE Control Module Restrictions][$i]\nkcm_users=false\n"))
        self.assertIn(self.read_key(), ("false", None))

    def test_a_second_run_changes_nothing(self):
        self.file.write_text(BAZZITE)
        once = self.run_script()
        self.assertEqual(self.run_script(), once)
        self.assertEqual(once.count("kcm_users=false"), 1)

    def test_adds_an_immutable_group_when_there_is_none(self):
        self.file.write_text("[KDE]\nSingleClick=false")
        after = self.run_script()
        self.assertEqual(after, "[KDE]\nSingleClick=false\n\n"
                                "[KDE Control Module Restrictions][$i]\nkcm_users=false\n")
        self.assertIn(self.read_key(), ("false", None))

    def test_makes_the_file_when_there_is_none(self):
        after = self.run_script()
        self.assertEqual(after, "\n[KDE Control Module Restrictions][$i]\nkcm_users=false\n")
        self.assertIn(self.read_key(), ("false", None))

    def test_joins_a_group_without_the_immutable_mark(self):
        self.file.write_text("[KDE Control Module Restrictions]\nkcm_updates=false\n")
        self.assertEqual(self.run_script(), "[KDE Control Module Restrictions]\n"
                                            "kcm_users=false\nkcm_updates=false\n")

    def test_the_key_in_another_group_does_not_count(self):
        self.file.write_text("[Other]\nkcm_users=false\n")
        after = self.run_script()
        self.assertTrue(after.endswith("[KDE Control Module Restrictions][$i]\nkcm_users=false\n"))
        self.assertIn(self.read_key(), ("false", None))

    def test_has_the_licence_line(self):
        self.assertEqual(SCRIPT.read_text().splitlines()[1],
                         "# SPDX-License-Identifier: Apache-2.0")


if __name__ == "__main__":
    unittest.main()
