#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Checks that the polkit rules still say what the README promises.

polkit's engine is not available here, so most of this reads the file as
text. Where a test needs the rule's answer it runs the rule with `node` and a
stand-in for polkit, and is skipped where there is no `node`. The real check
is `pkcheck` as the child in the VM, recorded in
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
# Moving the screen to another session, and the lock that stops the power
# button's hold: open to anyone at the console by logind's policy, found by
# ADR-0049's research rather than the P0-4 run.
SCREEN_AND_POWER_KEY = [
    "org.freedesktop.login1.chvt",
    "org.freedesktop.login1.inhibit-handle-power-key",
]
# The one action systemd asks polkit about for any unit, and the three
# units the base image's updater package, uupd, ships (#265).
MANAGE_UNITS = "org.freedesktop.systemd1.manage-units"
UPDATER_UNITS = ["uupd.service", "uupd-manual.service", "uupd.timer"]


def without_comments(text):
    """The rule's text with its // comments taken out, so that a name
    commented out, or quoted in a comment, does not count."""
    return re.sub(r"//[^\n]*", "", text)


def quoted_strings(text):
    return set(re.findall(r'"([^"]*)"', without_comments(text)))


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

    def test_uses_no_javascript_that_polkits_engine_refuses(self):
        # polkit drops a rule file it cannot read, and every refusal in the
        # file goes with it. The tests below run the rule with node, which
        # takes all of these; polkit 127's engine, Duktape, was seen to
        # refuse each one (kiosk-containment.md, 2026-11-11).
        code = re.sub(r'"[^"]*"', '""', without_comments(self.text))
        refused = {
            "an arrow function": r"=>",
            "let": r"\blet\b",
            "a template string": "`",
            "for ... of": r"\bof\b",
            "includes, which a list does not have": r"\.includes\(",
            "a default argument": r"function[^(]*\([^)]*=",
        }
        for form, pattern in refused.items():
            with self.subTest(form=form):
                self.assertIsNone(re.search(pattern, code))


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
# and answers for one action and one account's groups. Some programs send
# details with the action, as systemd sends the unit; the rule reads one
# with action.lookup, which gives nothing for a detail that was not sent.
# polkit runs every rule file in one context, so the text of a file read
# later can be given too, and runs after the rule in the same one.
RULE_RUNNER = """
var rules = [];
var polkit = {
    Result: {NO: "no", NOT_HANDLED: "not-handled", YES: "yes"},
    addRule: function (rule) { rules.push(rule); },
};
var input = JSON.parse(process.argv[process.argv.length - 1]);
eval(require("fs").readFileSync(input.rules, "utf8"));
if (input.later) { eval(input.later); }
var subject = {isInGroup: function (group) { return input.groups.indexOf(group) >= 0; }};
var details = input.details || {};
var action = {id: input.action, lookup: function (name) { return details[name]; }};
process.stdout.write(rules[0](action, subject));
"""


class FamilyActionTest(unittest.TestCase):
    """cairn-family's actions, one per subcommand (ADR-0039, ADR-0041,
    ADR-0043): the levels rule refuses each to a child and lets an
    administrator through to the password prompt, with no change of its
    own."""

    SUBCOMMANDS = {"create-child", "set-level", "save-games", "log-out", "set-own-password"}
    # Asks for the grown-up's own password, the one being changed.
    OWN = {"set-own-password"}

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
        # A look for new games is save-games with no child named, and pkexec
        # looks no further than the first argument, so one message is true
        # for a look and for a Save (#220). KDE's password box shows the
        # description too, under its details.
        self.assertEqual(messages["save-games"],
                         "A grown-up's password is needed to look for new games, "
                         "or to change which games a child has.")
        self.assertEqual(self.by_subcommand()["save-games"].findtext("description"),
                         "Look for new games, or change which games a child has")
        # Logging a child out is its own password, never folded into a Save.
        self.assertEqual(messages["log-out"], "A grown-up's password is needed to log a child out.")
        for subcommand in ("create-child", "set-level"):
            self.assertEqual(messages[subcommand],
                             "A grown-up's password is needed to change a child's account.")
        self.assertEqual(messages["set-own-password"], "Type the password you use now to change it.")

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
        # auth_admin, never auth_admin_keep: each change asks again. A
        # grown-up's own password asks for their own, auth_self, never kept
        # either (ADR-0043); cairn-family checks they are a grown-up.
        for subcommand, action in self.by_subcommand().items():
            with self.subTest(action=action.get("id")):
                defaults = {child.tag: child.text for child in action.find("defaults")}
                asks = "auth_self" if subcommand in self.OWN else "auth_admin"
                self.assertEqual(defaults, {"allow_any": "no", "allow_inactive": "no",
                                            "allow_active": asks})

    def test_pkexec_finds_them_by_the_commands_path(self):
        # The build changes /usr/local to its own prefix (cmake/CairnInstall.cmake).
        for action in self.actions:
            with self.subTest(action=action.get("id")):
                self.assertEqual(self.annotation(action, "path"),
                                 ["/usr/local/libexec/cairn/cairn-family"])

    def test_has_the_licence_line(self):
        self.assertEqual(self.text.splitlines()[1], "<!-- SPDX-License-Identifier: Apache-2.0 -->")


class UpdaterUnitsTest(unittest.TestCase):
    """The system updater's units (#265): Universal Blue's uupd.rules says
    yes to any account that asks systemd to start or stop the updater, so
    the levels rule refuses a child first, at every level and whatever is
    asked of the unit. An administrator, an account Cairn did not make and
    every other unit are left to the rule files that come after."""

    CHILDREN = [("ada", "cairn-l1"), ("ben", "cairn-l2"), ("cy", "cairn-l3"), ("ada", "cairn-l4")]
    # Guardian first, even from a level group (ADR-0023).
    ADMINISTRATORS = [
        ("guardian", "cairn-guardian"),
        ("guardian", "wheel"),
        ("guardian", "wheel", "cairn-guardian"),
        ("guardian", "cairn-guardian", "cairn-l1"),
        ("guardian", "cairn-guardian", "cairn-l4"),
        ("guardian", "wheel", "cairn-l2"),
        ("guardian", "wheel", "cairn-l3"),
    ]
    NOT_CAIRNS = [(), ("users",)]

    def setUp(self):
        self.rules = RULES.read_text()
        self.node = shutil.which("node")

    def needs_node(self):
        """Called first by each test that runs the rule, so that a machine
        without node skips the test once and not once for every case."""
        if self.node is None:
            self.skipTest("no node here to run the rule with")

    def answer(self, groups, unit, verb="start", action=MANAGE_UNITS):
        """What the rule says to an account in these groups when systemd
        asks about this unit. systemd sends the unit and the verb as
        details of the action; None leaves one out."""
        details = {name: value for name, value in (("unit", unit), ("verb", verb))
                   if value is not None}
        given = json.dumps({"rules": str(RULES), "action": action, "groups": list(groups),
                            "details": details})
        result = subprocess.run([self.node, "-e", RULE_RUNNER, given], capture_output=True,
                                text=True, check=True)
        return result.stdout

    def test_the_list_is_the_updaters_three_units(self):
        lists = re.findall(r"var updaterUnits = \[(.*?)\];", self.rules, re.S)
        self.assertEqual([quoted_strings(units) for units in lists], [set(UPDATER_UNITS)])
        self.assertIn(f'var manageUnits = "{MANAGE_UNITS}";', self.rules)

    def test_the_unit_is_the_only_detail_the_rule_reads(self):
        # Never the verb: uupd.rules says yes whatever the verb, so a rule
        # that picked verbs would leave open the ones it did not name.
        read = re.findall(r'\.lookup\("([^"]*)"\)', without_comments(self.rules))
        self.assertEqual(read, ["unit"])

    def test_a_child_at_every_level_is_refused_for_every_updater_unit(self):
        self.needs_node()
        for groups in self.CHILDREN:
            for unit in UPDATER_UNITS:
                for verb in ("start", "stop"):
                    with self.subTest(groups=groups, unit=unit, verb=verb):
                        self.assertEqual(self.answer(groups, unit, verb), "no")

    def test_whatever_is_asked_of_the_unit(self):
        # Verbs systemd 262 was seen to send, one it does not have, and none.
        # It sends cancel when a job of the unit's is cancelled over its own
        # socket, /run/systemd/io.systemd.Manager, which any account can open.
        self.needs_node()
        verbs = ("restart", "try-restart", "kill", "reset-failed", "set-property", "ref",
                 "cancel", "a-verb-systemd-adds-later", None)
        for groups in (("ada", "cairn-l1"), ("cy", "cairn-l3")):
            for verb in verbs:
                with self.subTest(groups=groups, verb=verb):
                    self.assertEqual(self.answer(groups, "uupd.service", verb), "no")

    def test_the_same_child_asking_about_another_unit_is_not_answered(self):
        self.needs_node()
        for groups in self.CHILDREN:
            for unit in ("bluetooth.service", "cups.service"):
                with self.subTest(groups=groups, unit=unit):
                    self.assertEqual(self.answer(groups, unit), "not-handled")

    def test_a_unit_is_matched_by_its_whole_name(self):
        # None is a request that names no unit, as when every unit's failed
        # state is cleared at once.
        self.needs_node()
        near = ("uupd", "uupd.service.d", "not-uupd.service", "uupd-manual.timer",
                "uupd.services", "UUPD.service", "", None)
        for unit in near:
            with self.subTest(unit=unit):
                self.assertEqual(self.answer(("ben", "cairn-l2"), unit), "not-handled")

    def test_only_the_one_action_is_answered(self):
        # polkit runs the rules again for manage-unit-files, with the same
        # unit, because that action implies manage-units. Nothing says yes
        # to it, and the levels rule leaves it and the rest of systemd's
        # actions to systemd's default, a grown-up's password.
        self.needs_node()
        others = ("org.freedesktop.systemd1.manage-unit-files",
                  "org.freedesktop.systemd1.reload-daemon",
                  "org.freedesktop.systemd1.set-environment")
        for action in others:
            for unit in UPDATER_UNITS:
                with self.subTest(action=action, unit=unit):
                    self.assertEqual(self.answer(("ada", "cairn-l1"), unit, action=action),
                                     "not-handled")

    def test_an_administrator_falls_through_even_from_a_level_group(self):
        self.needs_node()
        for groups in self.ADMINISTRATORS:
            for unit in UPDATER_UNITS:
                with self.subTest(groups=groups, unit=unit):
                    self.assertEqual(self.answer(groups, unit), "not-handled")

    def test_an_account_cairn_did_not_make_falls_through(self):
        self.needs_node()
        for groups in self.NOT_CAIRNS:
            for unit in UPDATER_UNITS:
                with self.subTest(groups=groups, unit=unit):
                    self.assertEqual(self.answer(groups, unit), "not-handled")


class LaterRuleFileTest(unittest.TestCase):
    """polkit runs every rule file in one JavaScript context, so a file
    read later that names a function or a list as the levels rule does
    would take that name over. The rule keeps its names inside one
    function, so its answers are the same whatever a later file names."""

    # Who asks, for what, with which details, and the rule's answer.
    CASES = [
        (("ada", "cairn-l1"), MANAGE_UNITS, {"unit": "uupd.service", "verb": "start"}, "no"),
        (("cy", "cairn-l3"), "org.projectatomic.rpmostree1.upgrade", {}, "no"),
        (("ben", "cairn-l2"), "org.freedesktop.login1.power-off", {}, "no"),
        (("cy", "cairn-l3"), "com.cairnlinux.family.set-level", {}, "no"),
        (("ada", "cairn-l1"), MANAGE_UNITS, {"unit": "bluetooth.service", "verb": "start"},
         "not-handled"),
        (("guardian", "wheel", "cairn-guardian"), MANAGE_UNITS, {"unit": "uupd.service"},
         "not-handled"),
    ]

    def setUp(self):
        code = without_comments(RULES.read_text())
        self.functions = sorted(set(re.findall(r"\bfunction (\w+)\(", code)))
        self.lists = sorted(set(re.findall(r"\bvar (\w+) =", code)))

    def test_the_rule_names_functions_and_lists_to_take_over(self):
        # Or the test below would pass by finding nothing to try.
        self.assertIn("isOneOf", self.functions)
        self.assertIn("updaterUnits", self.lists)

    def test_a_later_file_with_the_same_names_changes_no_answer(self):
        node = shutil.which("node")
        if node is None:
            self.skipTest("no node here to run the rule with")
        later_files = {
            "none": "",
            "the same functions": "".join(f"function {name}() {{ return false; }}\n"
                                          for name in self.functions),
            "the same lists": "".join(f'var {name} = ["its own"];\n' for name in self.lists),
        }
        for later, text in later_files.items():
            for groups, action, details, answer in self.CASES:
                with self.subTest(later=later, groups=groups, action=action, details=details):
                    given = json.dumps({"rules": str(RULES), "action": action,
                                        "groups": list(groups), "details": details,
                                        "later": text})
                    result = subprocess.run([node, "-e", RULE_RUNNER, given], capture_output=True,
                                            text=True, check=True)
                    self.assertEqual(result.stdout, answer)


class KioskScreenAndPowerKeyTest(unittest.TestCase):
    """logind's chvt and the power key's lock (ADR-0049): refused at L1 and
    L2 before any prompt, and left to polkit's defaults for an
    administrator, L3, L4 and an account Cairn did not make."""

    def setUp(self):
        self.rules = RULES.read_text()

    def test_both_are_in_the_kiosks_list(self):
        # The kiosk's list, not the Guardian's, which refuses L3 and L4 too.
        [kiosk] = re.findall(r"var notInTheKiosk = \[(.*?)\];", self.rules, re.S)
        entries = quoted_strings(kiosk)
        for action in SCREEN_AND_POWER_KEY:
            with self.subTest(action=action):
                self.assertIn(action, entries)

    def test_the_rule_itself_answers_so(self):
        node = shutil.which("node")
        if node is None:
            self.skipTest("no node here to run the rule with")
        cases = {
            ("ada", "cairn-l1"): "no",
            ("ben", "cairn-l2"): "no",
            ("ada", "cairn-l3"): "not-handled",
            ("ben", "cairn-l4"): "not-handled",
            ("gina", "wheel", "cairn-guardian"): "not-handled",
            ("dad", "cairn-guardian", "cairn-l1"): "not-handled",
            ("bazzite", "wheel"): "not-handled",
            ("lodger",): "not-handled",
        }
        for action in SCREEN_AND_POWER_KEY:
            for groups, answer in cases.items():
                with self.subTest(action=action, groups=groups):
                    given = json.dumps({"rules": str(RULES), "action": action,
                                        "groups": list(groups)})
                    result = subprocess.run([node, "-e", RULE_RUNNER, given], capture_output=True,
                                            text=True, check=True)
                    self.assertEqual(result.stdout, answer)


if __name__ == "__main__":
    unittest.main()
