#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Checks the PAM files a grown-up's Steam visit depends on (ADR-0032).

PAM itself is not available here, so this reads the files as text. The
real check refused a listed child at the VM's login screen, recorded in
docs/research/steam-visit.md.
"""

import pathlib
import unittest

from test_pam_sddm import REFUSAL, stack_lines

PAM_DIR = pathlib.Path(__file__).resolve().parent.parent / "pam.d"
LOGIN = PAM_DIR / "login"
VISIT = PAM_DIR / "cairn-steam-visit"

# Fedora 44's /etc/pam.d/login from util-linux 2.41, without its comments.
STOCK_LOGIN = """\
auth       substack     system-auth
auth       include      postlogin
account    required     pam_nologin.so
account    include      system-auth
password   include      system-auth
session    required     pam_selinux.so close
session    required     pam_loginuid.so
session    required     pam_selinux.so open
session    required     pam_namespace.so
session    optional     pam_keyinit.so force revoke
session    include      system-auth
session    include      postlogin
-session   optional     pam_ck_connector.so
"""


def rules(text):
    return [line for line in text.splitlines() if line and not line.startswith("#")]


class ConsoleLoginTest(unittest.TestCase):
    def setUp(self):
        self.text = LOGIN.read_text()

    def test_is_fedoras_file_with_the_one_line_added(self):
        added = [line for line in rules(self.text) if line not in STOCK_LOGIN.splitlines()]
        self.assertEqual([line.split() for line in added], [REFUSAL])
        self.assertEqual(
            [line for line in rules(self.text) if line.split() != REFUSAL],
            STOCK_LOGIN.splitlines(),
        )

    def test_a_child_on_a_steam_visit_is_refused_first(self):
        self.assertEqual(stack_lines(self.text, "account")[0], REFUSAL)

    def test_has_the_licence_line(self):
        self.assertEqual(self.text.splitlines()[1], "# SPDX-License-Identifier: Apache-2.0")


class VisitSessionTest(unittest.TestCase):
    def setUp(self):
        self.text = VISIT.read_text()

    def test_never_refuses_the_visit_itself(self):
        self.assertNotIn("pam_listfile", self.text)

    def test_nobody_logs_in_through_it(self):
        self.assertEqual(stack_lines(self.text, "auth"), [["auth", "required", "pam_deny.so"]])
        self.assertEqual(stack_lines(self.text, "password"), [["password", "required", "pam_deny.so"]])

    def test_is_a_session_of_the_childs_own(self):
        # system-auth brings pam_systemd: the child's runtime directory and
        # a logind session, which the close ends.
        session = stack_lines(self.text, "session")
        self.assertIn(["session", "include", "system-auth"], session)
        self.assertIn(["session", "required", "pam_selinux.so", "open"], session)

    def test_the_helper_asks_for_this_service(self):
        helper = (PAM_DIR.parent / "bin" / "cairn-steam-visit").read_text()
        self.assertIn("--property=PAMName=cairn-steam-visit", helper)
        self.assertEqual(VISIT.name, "cairn-steam-visit")

    def test_has_the_licence_line(self):
        self.assertEqual(self.text.splitlines()[1], "# SPDX-License-Identifier: Apache-2.0")


if __name__ == "__main__":
    unittest.main()
