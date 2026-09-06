#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Checks that the kiosk labwc config still says what the README promises.

Standard library only, like the rest of the Python here. The compositor
itself is checked by hand under nested labwc (launcher/README.md); what
those checks found is in docs/research/kiosk-containment.md.
"""

import pathlib
import unittest
import xml.etree.ElementTree as ET

LABWC_DIR = pathlib.Path(__file__).resolve().parent.parent / "labwc"
CONFIG = LABWC_DIR / "rc.xml"
ENVIRONMENT = LABWC_DIR / "environment"


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

    def test_every_window_is_undecorated_and_cannot_ask_for_focus(self):
        # X11 windows got a titlebar and an activation request took focus
        # from the launcher until this rule existed (kiosk-containment.md).
        rules = self.root.findall("./windowRules/windowRule")
        catch_all = [rule for rule in rules if rule.get("identifier") == "*"]
        self.assertEqual(len(catch_all), 1, "exactly one rule for every window")
        rule = catch_all[0]
        self.assertEqual(rule.get("serverDecoration"), "no")
        self.assertEqual(rule.get("ignoreFocusRequest"), "yes")
        # Later rules win in labwc, so the catch-all must come first.
        self.assertIs(rules[0], rule)

    def test_vt_switching_keysyms_are_removed_from_the_keymap(self):
        # labwc switches VTs on XF86Switch_VT keysyms in code, with no rc.xml
        # option; the XKB option takes the keysyms out of the keymap instead.
        settings = {}
        for line in ENVIRONMENT.read_text().splitlines():
            line = line.strip()
            if line and not line.startswith("#"):
                name, _, value = line.partition("=")
                settings[name] = value
        options = settings.get("XKB_DEFAULT_OPTIONS", "").split(",")
        self.assertIn("srvrkeys:none", options)


if __name__ == "__main__":
    unittest.main()
