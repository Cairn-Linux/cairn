#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Checks that the lock screen's fingerprint service never counts a missing
reader as a wrong try, and still counts each wrong finger (#283, ADR-0051).

This reads the file as text. The image's build runs the real stack
(build_files/check-lock-screen-pam.py), and ADR-0051 lists the VM checks.
"""

import pathlib
import unittest

PAM = pathlib.Path(__file__).resolve().parent.parent / "pam.d" / "kde-fingerprint"

# The whole auth block, so a line slipped in between the fingerprint and the
# count fails here too.
AUTH = [
    ["auth", "required", "pam_env.so"],
    ["auth", "required", "pam_faillock.so", "preauth", "silent"],
    ["auth", "[success=done authinfo_unavail=die default=bad]", "pam_fprintd.so", "max-tries=1"],
    ["auth", "required", "pam_faillock.so", "authfail"],
    ["auth", "required", "pam_deny.so"],
]

# Fedora's plasma-workspace file below its auth lines, which Cairn keeps.
FEDORA_REST = """account     required      pam_nologin.so
account     include       fingerprint-auth

password    include       fingerprint-auth

session     required      pam_selinux.so close
session     required      pam_loginuid.so
session     required      pam_selinux.so open
session     optional      pam_keyinit.so force revoke
session     required      pam_namespace.so
session     include       fingerprint-auth
session     include       postlogin
"""


def auth_lines(text):
    lines = []
    for line in text.splitlines():
        if line.startswith("auth"):
            control_start = line.find("[")
            if control_start >= 0:
                control_end = line.index("]") + 1
                lines.append(["auth", line[control_start:control_end], *line[control_end:].split()])
            else:
                lines.append(line.split())
    return lines


class PamKdeFingerprintTest(unittest.TestCase):
    def setUp(self):
        self.text = PAM.read_text()
        self.auth = auth_lines(self.text)

    def test_the_auth_lines_are_exactly_these(self):
        self.assertEqual(self.auth, AUTH)

    def test_no_reader_ends_the_stack_before_the_count(self):
        fprintd = next(i for i, words in enumerate(self.auth) if words[2] == "pam_fprintd.so")
        authfail = next(i for i, words in enumerate(self.auth) if words[3:] == ["authfail"])
        self.assertIn("authinfo_unavail=die", self.auth[fprintd][1])
        self.assertLess(fprintd, authfail)

    def test_no_reader_never_lets_anyone_in(self):
        control = self.auth[2][1].strip("[]").split()
        self.assertEqual([word for word in control if word.endswith(("=ok", "=done", "=ignore"))],
                         ["success=done"])

    def test_each_wrong_finger_counts(self):
        self.assertIn("max-tries=1", self.auth[2])
        self.assertEqual(self.auth[-2][1:], ["required", "pam_faillock.so", "authfail"])
        self.assertEqual(self.auth[-1][1:], ["required", "pam_deny.so"])

    def test_a_locked_account_is_refused_first(self):
        self.assertEqual(self.auth[1][1:], ["required", "pam_faillock.so", "preauth", "silent"])

    def test_auth_does_not_run_fingerprint_auth(self):
        self.assertNotIn("fingerprint-auth", [words[2] for words in self.auth])

    def test_the_other_stacks_are_fedoras(self):
        self.assertTrue(self.text.endswith(FEDORA_REST))

    def test_has_the_licence_line(self):
        self.assertEqual(self.text.splitlines()[1], "# SPDX-License-Identifier: Apache-2.0")


if __name__ == "__main__":
    unittest.main()
