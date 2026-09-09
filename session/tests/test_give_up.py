#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Checks that cairn-give-up still says what the README and ADR-0018 promise.

The helper is exercised for real in the VM (docs/research/kiosk-containment.md);
this reads it as text, the way the polkit rule is tested.
"""

import pathlib
import re
import unittest

HELPER = pathlib.Path(__file__).resolve().parent.parent / "bin" / "cairn-give-up"


class GiveUpHelperTest(unittest.TestCase):
    def setUp(self):
        self.text = HELPER.read_text()

    def test_has_the_licence_line(self):
        self.assertTrue(self.text.startswith("#!/bin/bash\n# SPDX-License-Identifier: Apache-2.0"))

    def test_matches_the_sandbox_roots_and_not_the_frame(self):
        # bwrap is every Flatpak app; reaper is every Steam game. The
        # launcher, the compositor and the Steam client are none of these.
        self.assertIn("bwrap", self.text)
        self.assertIn("reaper SteamLaunch", self.text)
        for safe in ("cairn-launcher", "labwc", "steamwebhelper", "pipewire"):
            self.assertNotIn(safe, self.text, f"{safe} must never be a target")

    def test_continues_a_stopped_process_before_killing_it(self):
        # A stopped process cannot act on TERM or KILL until it is continued,
        # so CONT has to come first, and KILL has to come after TERM.
        order = [m.group(1) for m in re.finditer(r"--signal (CONT|TERM|KILL)", self.text)]
        self.assertEqual(order, ["CONT", "TERM", "KILL"])

    def test_only_ever_signals_the_childs_own_processes(self):
        # Never another user's; --uid on every pkill.
        for line in self.text.splitlines():
            if "pkill" in line:
                self.assertIn("--uid", line, f"pkill without --uid: {line.strip()}")


if __name__ == "__main__":
    unittest.main()
