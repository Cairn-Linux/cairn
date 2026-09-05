#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Checks that the kiosk labwc config still says what the README promises.

Standard library only, like the rest of the Python here. The compositor
itself is checked by hand under nested labwc (launcher/README.md).
"""

import pathlib
import unittest
import xml.etree.ElementTree as ET

CONFIG = pathlib.Path(__file__).resolve().parent.parent / "labwc" / "rc.xml"


class LabwcConfigTest(unittest.TestCase):
    def setUp(self):
        self.root = ET.parse(CONFIG).getroot()

    def test_is_a_labwc_config(self):
        self.assertEqual(self.root.tag, "labwc_config")

    def test_launcher_rule_makes_it_fullscreen_without_decorations(self):
        rules = [
            rule
            for rule in self.root.findall("./windowRules/windowRule")
            if rule.get("identifier") == "cairn-launcher"
        ]
        self.assertEqual(len(rules), 1, "exactly one rule for the launcher")
        rule = rules[0]
        self.assertEqual(rule.get("serverDecoration"), "no")
        actions = [action.get("name") for action in rule.findall("action")]
        self.assertIn("ToggleFullscreen", actions)

    def test_default_keybinds_are_turned_off(self):
        # labwc loads its defaults when a config has no keybind at all.
        keybinds = self.root.findall("./keyboard/keybind")
        self.assertTrue(keybinds, "at least one keybind, or the defaults load")
        self.assertIsNone(self.root.find("./keyboard/default"))

    def test_default_mousebinds_are_turned_off(self):
        mousebinds = self.root.findall("./mouse/context/mousebind")
        self.assertTrue(mousebinds, "at least one mousebind, or the defaults load")
        self.assertIsNone(self.root.find("./mouse/default"))


if __name__ == "__main__":
    unittest.main()
