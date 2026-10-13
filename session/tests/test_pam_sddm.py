#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Checks that the greeter's PAM service lets only children in without a
password, and never an administrator (ADR-0016, ADR-0023).

PAM itself is not available here, so this reads the file as text. The real
check ran the sddm service as each kind of account in the VM, recorded in
ADR-0023; behavioural harnesses are #102.
"""

import pathlib
import unittest

PAM = pathlib.Path(__file__).resolve().parent.parent / "sddm" / "pam.d" / "sddm"


def stack_lines(text, stack):
    """One PAM stack, one list of words per line, comments dropped."""
    lines = []
    for line in text.splitlines():
        words = line.split()
        if words and words[0].lstrip("-") == stack:
            lines.append(words)
    return lines


def auth_lines(text):
    return stack_lines(text, "auth")


# The list of children whose Steam a grown-up has open (ADR-0032), as the
# helper writes it.
VISITS = "file=/run/cairn/steam-visits"
REFUSAL = ["account", "requisite", "pam_listfile.so", "item=user", "sense=deny", "onerr=succeed", VISITS]


class PamSddmTest(unittest.TestCase):
    def setUp(self):
        self.text = PAM.read_text()
        self.auth = auth_lines(self.text)

    def passwordless(self):
        return [words for words in self.auth if "pam_succeed_if.so" in words]

    def test_only_l1_and_l2_are_let_in_without_a_password(self):
        levels = [words[words.index("ingroup") + 1] for words in self.passwordless()]
        self.assertEqual(levels, ["cairn-l1", "cairn-l2"])

    def test_an_administrator_is_never_let_in_without_one(self):
        # pam_succeed_if needs every condition on the line to hold (#101).
        for words in self.passwordless():
            with self.subTest(line=" ".join(words)):
                self.assertEqual(words[1], "sufficient")
                conditions = words[words.index("pam_succeed_if.so") + 1 :]
                self.assertIn("user notingroup cairn-guardian:wheel", " ".join(conditions))

    def test_the_rule_comes_before_the_password(self):
        modules = [words[2] for words in self.auth]
        self.assertLess(
            max(i for i, m in enumerate(modules) if m == "pam_succeed_if.so"),
            modules.index("password-auth"),
        )

    def test_a_child_on_a_steam_visit_is_refused_first(self):
        # First in the account stack, and requisite, so nothing after it
        # can let the child in (ADR-0032).
        account = stack_lines(self.text, "account")
        self.assertEqual(account[0], REFUSAL)

    def test_the_list_is_the_one_the_helper_writes(self):
        helper = (PAM.parents[2] / "bin" / "cairn-steam-visit").read_text()
        self.assertIn('RUN_DIR=/run/cairn\nVISITS="$RUN_DIR/steam-visits"\n', helper)

    def test_has_the_licence_line(self):
        self.assertEqual(self.text.splitlines()[1], "# SPDX-License-Identifier: Apache-2.0")


if __name__ == "__main__":
    unittest.main()
