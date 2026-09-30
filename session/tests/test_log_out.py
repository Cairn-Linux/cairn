#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Runs bin/cairn-log-out with stand-ins on PATH (ADR-0021).

Standard library only. `id` prints a uid, `pgrep` says whether Steam is
running, and `steam` and `loginctl` write what they were asked to a log
instead of doing it. The real run is in the VM
(docs/research/kiosk-containment.md).
"""

import os
import pathlib
import subprocess
import tempfile
import unittest

HELPER = pathlib.Path(__file__).resolve().parent.parent / "bin" / "cairn-log-out"


def run_log_out(steam_running):
    with tempfile.TemporaryDirectory() as bin_dir:
        log = pathlib.Path(bin_dir, "calls")
        # pgrep finds Steam until it has been told to shut down.
        pgrep = (
            f'#!/bin/sh\necho "pgrep $*" >> {log}\n'
            f'grep -q "^steam -shutdown" {log} && exit 1\n'
            f'exit {0 if steam_running else 1}\n'
        )
        stubs = {
            "id": "#!/bin/sh\necho 1002\n",
            "pgrep": pgrep,
            "steam": f'#!/bin/sh\necho "steam $*" >> {log}\n',
            "loginctl": f'#!/bin/sh\necho "loginctl $*" >> {log}\n',
        }
        for name, body in stubs.items():
            stub = pathlib.Path(bin_dir, name)
            stub.write_text(body)
            stub.chmod(0o755)
        env = {"PATH": f"{bin_dir}:/usr/bin:/bin"}
        result = subprocess.run(
            [str(HELPER)], env=env, capture_output=True, text=True, check=False
        )
        calls = log.read_text().splitlines() if log.exists() else []
        return result.returncode, calls


class LogOutHelperTest(unittest.TestCase):
    def test_has_the_licence_line(self):
        text = HELPER.read_text()
        self.assertTrue(text.startswith("#!/bin/bash\n# SPDX-License-Identifier: Apache-2.0"))

    def test_ends_the_users_own_account_and_nothing_else(self):
        code, calls = run_log_out(steam_running=False)
        self.assertEqual(code, 0)
        self.assertEqual(calls[-1], "loginctl terminate-user 1002")
        self.assertEqual([c for c in calls if c.startswith("loginctl")], ["loginctl terminate-user 1002"])

    def test_steam_not_running_is_never_started(self):
        # `steam -shutdown` starts a client that is not running.
        code, calls = run_log_out(steam_running=False)
        self.assertEqual(code, 0)
        self.assertFalse([c for c in calls if c.startswith("steam")])

    def test_a_running_steam_is_asked_to_shut_down_first(self):
        code, calls = run_log_out(steam_running=True)
        self.assertEqual(code, 0)
        self.assertIn("steam -shutdown", calls)
        self.assertLess(calls.index("steam -shutdown"), calls.index("loginctl terminate-user 1002"))

    def test_only_looks_for_the_users_own_steam(self):
        _, calls = run_log_out(steam_running=True)
        for call in calls:
            if call.startswith("pgrep"):
                self.assertIn("--uid 1002", call)


if __name__ == "__main__":
    unittest.main()
