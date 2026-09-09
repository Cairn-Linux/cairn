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

    def test_steam_windows_are_iconified_but_still_reported(self):
        # The sign-in window came up centred over the grown-up screen until
        # this rule existed (steam-containment.md). labwc matches the
        # WM_CLASS instance, so "steamwebhelper" is the one that hides the
        # client's windows; "steam" covers the bootstrapper.
        rules = self.root.findall("./windowRules/windowRule")
        by_id = {rule.get("identifier"): rule for rule in rules}
        for identifier in ("steamwebhelper", "steam"):
            rule = by_id.get(identifier)
            self.assertIsNotNone(rule, f"a rule for {identifier}")
            actions = [action.get("name") for action in rule.findall("action")]
            self.assertEqual(actions, ["Iconify"])
            # Dropping the foreign-toplevel handle would blind the launcher.
            self.assertIsNone(rule.get("skipTaskbar"))
            # The catch-all comes first, so these must come after it.
            self.assertGreater(rules.index(rule), rules.index(by_id["*"]))

    def test_the_grown_up_combination_runs_the_give_up_helper(self):
        # ADR-0018: Ctrl-Alt-Home runs cairn-give-up, which ends a stuck app.
        keybinds = {k.get("key"): k for k in self.root.findall("./keyboard/keybind")}
        bind = keybinds.get("C-A-Home")
        self.assertIsNotNone(bind, "a C-A-Home keybind")
        commands = [a.get("command") for a in bind.findall("./action[@name='Execute']")]
        self.assertEqual(commands, ["cairn-give-up"])

    def test_the_power_button_is_a_tap_to_ignore_and_a_hold_to_power_off(self):
        # ADR-0018, in logind's hands rather than the compositor's.
        conf = (LABWC_DIR.parent / "logind.conf.d" / "10-cairn-power.conf").read_text()
        settings = {}
        for line in conf.splitlines():
            line = line.strip()
            if line and not line.startswith(("#", "[")):
                name, _, value = line.partition("=")
                settings[name] = value
        self.assertEqual(settings.get("HandlePowerKey"), "ignore")
        self.assertEqual(settings.get("HandlePowerKeyLongPress"), "poweroff")
        # No power-button binding in the kiosk either, so a tap is nothing.
        keys = [k.get("key") for k in self.root.findall("./keyboard/keybind")]
        self.assertNotIn("XF86PowerOff", keys)

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
