#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Checks that the polkit rules still say what the README promises.

polkit's engine is not available here, so this reads the file as text. The
real check is `pkcheck` as the child in the VM, recorded in
docs/research/kiosk-containment.md.
"""

import pathlib
import re
import unittest

RULES = pathlib.Path(__file__).resolve().parent.parent / "polkit" / "rules.d" / "10-cairn-levels.rules"

# The action families the P0-4 run found open to a child (kiosk-containment.md).
GUARDIAN_ONLY = [
    "org.freedesktop.NetworkManager.",
    "org.freedesktop.Flatpak.",
    "org.freedesktop.packagekit.",
    "org.projectatomic.rpmostree1.",
    "com.endlessm.ParentalControls.AppFilter.Change",
    "com.endlessm.ParentalControls.SessionLimits.Change",
    "org.freedesktop.Malcontent.WebFilter.Change",
    "org.freedesktop.MalcontentControl.",
]
NOT_IN_THE_KIOSK = [
    "org.freedesktop.login1.power-off",
    "org.freedesktop.login1.reboot",
    # Allowed set-reboot-* implies allowed reboot (org.freedesktop.login1.policy).
    "org.freedesktop.login1.set-reboot-",
    "org.freedesktop.login1.suspend",
    "org.freedesktop.login1.hibernate",
    "org.freedesktop.udisks2.",
]


def quoted_strings(text):
    return set(re.findall(r'"([^"]*)"', text))


class PolkitRulesTest(unittest.TestCase):
    def setUp(self):
        self.text = RULES.read_text()
        self.strings = quoted_strings(self.text)

    def test_sorts_before_anything_the_base_image_ships(self):
        # First answer wins, in file name order across /etc and /usr/share.
        self.assertTrue(RULES.name.startswith("10-"))

    def test_keys_off_every_child_level_and_never_the_guardian(self):
        for group in ("cairn-l1", "cairn-l2", "cairn-l3", "cairn-l4"):
            self.assertIn(group, self.strings, f"a rule for {group}")
        self.assertNotIn("cairn-guardian", self.strings)

    def test_names_every_action_family_the_containment_run_found_open(self):
        for prefix in GUARDIAN_ONLY + NOT_IN_THE_KIOSK:
            self.assertIn(prefix, self.strings, f"a rule for {prefix}")

    def test_only_ever_says_no_or_steps_aside(self):
        # A YES here would widen what a child can do, which no rule of ours
        # may do; polkit's defaults stay the ceiling.
        results = set(re.findall(r"polkit\.Result\.(\w+)", self.text))
        self.assertEqual(results, {"NO", "NOT_HANDLED"})

    def test_has_the_licence_line(self):
        self.assertTrue(self.text.startswith("// SPDX-License-Identifier: Apache-2.0"))


if __name__ == "__main__":
    unittest.main()
