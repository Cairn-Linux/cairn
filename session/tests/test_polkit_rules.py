#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Checks that the polkit rules still say what the README promises.

polkit's engine is not available here, so this reads the file as text. The
real check is `pkcheck` as the child in the VM, recorded in
docs/research/kiosk-containment.md.
"""

import json
import pathlib
import re
import shutil
import subprocess
import unittest
import xml.etree.ElementTree as ElementTree

POLKIT = pathlib.Path(__file__).resolve().parent.parent / "polkit"
RULES = POLKIT / "rules.d" / "10-cairn-levels.rules"
VISIT_ACTION = POLKIT / "actions" / "com.cairnlinux.steam-visit.policy"
FAMILY_ACTION = POLKIT / "actions" / "com.cairnlinux.family.policy"
PROVISION = POLKIT.parents[1] / "provision" / "cairn-provision.sh"

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

    def test_keys_off_every_child_level(self):
        for group in ("cairn-l1", "cairn-l2", "cairn-l3", "cairn-l4"):
            self.assertIn(group, self.strings, f"a rule for {group}")

    def test_an_administrator_steps_aside_first(self):
        # The rule's first statement, before any NO: Guardian first, even
        # from a level group (ADR-0023, #101).
        self.assertRegex(
            self.text,
            r"polkit\.addRule\(function \(action, subject\) \{\s*"
            r'if \(subject\.isInGroup\("cairn-guardian"\) \|\| subject\.isInGroup\("wheel"\)\) \{\s*'
            r"return polkit\.Result\.NOT_HANDLED;",
        )

    def test_cairns_own_actions_are_refused_to_everyone_else_next(self):
        # Right after the administrator steps aside, before the level groups
        # are read, so an account Cairn did not make is refused too (ADR-0032).
        self.assertRegex(
            self.text,
            r"return polkit\.Result\.NOT_HANDLED;\s*\}\s*"
            r"if \(startsWith\(action\.id, cairnActions\)\) \{\s*"
            r"return polkit\.Result\.NO;",
        )
        self.assertIn('var cairnActions = "com.cairnlinux.";', self.text)

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



class SteamVisitActionTest(unittest.TestCase):
    def setUp(self):
        self.text = VISIT_ACTION.read_text()
        self.action = ElementTree.fromstring(self.text.encode()).find("action")

    def test_is_one_of_cairns_own_actions(self):
        self.assertTrue(self.action.get("id").startswith("com.cairnlinux."))

    def test_asks_an_administrator_every_time_and_only_at_the_machine(self):
        # auth_admin, never auth_admin_keep: each visit asks again.
        defaults = {child.tag: child.text for child in self.action.find("defaults")}
        self.assertEqual(defaults, {"allow_any": "no", "allow_inactive": "no", "allow_active": "auth_admin"})

    def test_pkexec_finds_it_by_where_provisioning_puts_the_helper(self):
        [path] = [a.text for a in self.action.findall("annotate") if a.get("key") == "org.freedesktop.policykit.exec.path"]
        self.assertEqual(path, "/usr/local/libexec/cairn/cairn-steam-visit")
        provision = PROVISION.read_text()
        self.assertIn('"$PREFIX/libexec/cairn/cairn-steam-visit"', provision)
        self.assertIn("PREFIX=/usr/local\n", provision)
        self.assertIn("/etc/polkit-1/actions/com.cairnlinux.steam-visit.policy", provision)

    def test_is_found_by_the_path_alone(self):
        # No first argument: every visit is one action, and it never names
        # cairn-family, whose copies the visit's action must not start.
        keys = [a.get("key") for a in self.action.findall("annotate")]
        self.assertNotIn("org.freedesktop.policykit.exec.argv1", keys)
        self.assertNotIn("cairn-family", self.text)

    def test_has_the_licence_line(self):
        self.assertEqual(self.text.splitlines()[1], "<!-- SPDX-License-Identifier: Apache-2.0 -->")


# A stand-in for polkit's engine, enough to run the rule: it loads the file
# and answers for one action and one account's groups.
RULE_RUNNER = """
var rules = [];
var polkit = {
    Result: {NO: "no", NOT_HANDLED: "not-handled", YES: "yes"},
    addRule: function (rule) { rules.push(rule); },
};
var input = JSON.parse(process.argv[process.argv.length - 1]);
eval(require("fs").readFileSync(input.rules, "utf8"));
var subject = {isInGroup: function (group) { return input.groups.indexOf(group) >= 0; }};
process.stdout.write(rules[0]({id: input.action}, subject));
"""


class FamilyActionTest(unittest.TestCase):
    """cairn-family's actions, one per subcommand (ADR-0039, ADR-0041): the
    levels rule refuses each to a child and lets an administrator through
    to the password prompt, with no change of its own."""

    SUBCOMMANDS = {"create-child", "set-level", "save-games", "log-out"}

    def setUp(self):
        self.text = FAMILY_ACTION.read_text()
        self.actions = ElementTree.fromstring(self.text.encode()).findall("action")
        self.rules = RULES.read_text()

    @staticmethod
    def annotation(action, key):
        return [a.text for a in action.findall("annotate")
                if a.get("key") == "org.freedesktop.policykit.exec." + key]

    def by_subcommand(self):
        return {self.annotation(action, "argv1")[0]: action for action in self.actions}

    def test_one_action_for_each_subcommand(self):
        # pkexec picks the first action whose path matches, and one without
        # a first argument matches any; with one on every action the order
        # polkit lists them in cannot matter.
        for action in self.actions:
            with self.subTest(action=action.get("id")):
                self.assertEqual(len(self.annotation(action, "argv1")), 1)
        self.assertEqual(set(self.by_subcommand()), self.SUBCOMMANDS)
        self.assertEqual(len(self.actions), len(self.SUBCOMMANDS))

    def test_each_is_one_of_cairns_own_actions(self):
        [prefix] = re.findall(r'var cairnActions = "([^"]+)";', self.rules)
        for subcommand, action in self.by_subcommand().items():
            with self.subTest(subcommand=subcommand):
                self.assertEqual(action.get("id"), "com.cairnlinux.family." + subcommand)
                self.assertTrue(action.get("id").startswith(prefix))

    def test_saving_games_says_so(self):
        messages = {subcommand: action.findtext("message")
                    for subcommand, action in self.by_subcommand().items()}
        self.assertEqual(messages["save-games"],
                         "A grown-up's password is needed to change which games a child has.")
        # Logging a child out is its own password, never folded into a Save.
        self.assertEqual(messages["log-out"], "A grown-up's password is needed to log a child out.")
        for subcommand in ("create-child", "set-level"):
            self.assertEqual(messages[subcommand],
                             "A grown-up's password is needed to change a child's account.")

    def test_a_child_is_refused_and_a_guardian_falls_through(self):
        # The rule steps aside for an administrator first and then refuses
        # every Cairn action, before it reads any level (PolkitRulesTest),
        # so this holds for any com.cairnlinux. action.
        self.assertRegex(
            self.rules,
            r'if \(subject\.isInGroup\("cairn-guardian"\) \|\| subject\.isInGroup\("wheel"\)\) \{\s*'
            r"return polkit\.Result\.NOT_HANDLED;\s*\}\s*"
            r"if \(startsWith\(action\.id, cairnActions\)\) \{\s*"
            r"return polkit\.Result\.NO;",
        )

    def test_the_rule_itself_answers_so(self):
        node = shutil.which("node")
        if node is None:
            self.skipTest("no node here to run the rule with")
        cases = {
            ("ada", "cairn-l1"): "no",
            ("ben", "cairn-l2"): "no",
            ("ada", "cairn-l3"): "no",
            ("lodger",): "no",
            ("gina", "wheel", "cairn-guardian"): "not-handled",
            ("dad", "cairn-guardian", "cairn-l1"): "not-handled",
            ("bazzite", "wheel"): "not-handled",
        }
        for action in self.actions:
            for groups, answer in cases.items():
                with self.subTest(action=action.get("id"), groups=groups):
                    given = json.dumps({"rules": str(RULES), "action": action.get("id"),
                                        "groups": list(groups)})
                    result = subprocess.run([node, "-e", RULE_RUNNER, given], capture_output=True,
                                            text=True, check=True)
                    self.assertEqual(result.stdout, answer)

    def test_asks_an_administrator_for_every_change_and_only_at_the_machine(self):
        # auth_admin, never auth_admin_keep: each change asks again.
        for action in self.actions:
            with self.subTest(action=action.get("id")):
                defaults = {child.tag: child.text for child in action.find("defaults")}
                self.assertEqual(defaults, {"allow_any": "no", "allow_inactive": "no",
                                            "allow_active": "auth_admin"})

    def test_pkexec_finds_them_by_the_commands_path(self):
        # The build changes /usr/local to its own prefix (cmake/CairnInstall.cmake).
        for action in self.actions:
            with self.subTest(action=action.get("id")):
                self.assertEqual(self.annotation(action, "path"),
                                 ["/usr/local/libexec/cairn/cairn-family"])

    def test_has_the_licence_line(self):
        self.assertEqual(self.text.splitlines()[1], "<!-- SPDX-License-Identifier: Apache-2.0 -->")


if __name__ == "__main__":
    unittest.main()
