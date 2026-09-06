#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Runs bin/cairn-session with stand-ins on PATH and checks which session it
picks for each level group (ADR-0012).

Standard library only. The stand-ins are tiny shell scripts: `id` prints
the groups a test asks for, and `labwc` and `startplasma-wayland` print
their name and arguments instead of starting anything.
"""

import os
import pathlib
import subprocess
import tempfile
import unittest

DISPATCHER = pathlib.Path(__file__).resolve().parent.parent / "bin" / "cairn-session"


def run_dispatcher(groups):
    with tempfile.TemporaryDirectory() as bin_dir:
        stubs = {
            "id": f'#!/bin/sh\necho "{groups}"\n',
            "labwc": '#!/bin/sh\necho "labwc $*"\n',
            "startplasma-wayland": '#!/bin/sh\necho "startplasma-wayland $*"\n',
        }
        for name, body in stubs.items():
            stub = pathlib.Path(bin_dir, name)
            stub.write_text(body)
            stub.chmod(0o755)
        env = {"PATH": f"{bin_dir}:/usr/bin:/bin", "CAIRN_LABWC_CONFIG": "/kiosk"}
        result = subprocess.run(
            [str(DISPATCHER)], env=env, capture_output=True, text=True, check=False
        )
        return result.returncode, result.stdout.strip()


class CairnSessionTest(unittest.TestCase):
    def test_l1_gets_the_kiosk_with_the_launcher(self):
        code, out = run_dispatcher("ada cairn-l1")
        self.assertEqual(code, 0)
        self.assertEqual(out, "labwc -C /kiosk -S cairn-launcher")

    def test_l2_gets_the_kiosk_too(self):
        code, out = run_dispatcher("ben cairn-l2")
        self.assertEqual(code, 0)
        self.assertEqual(out, "labwc -C /kiosk -S cairn-launcher")

    def test_l3_l4_and_guardian_get_plasma(self):
        for groups in ("cat cairn-l3", "dan cairn-l4", "guardian wheel cairn-guardian"):
            with self.subTest(groups=groups):
                code, out = run_dispatcher(groups)
                self.assertEqual(code, 0)
                self.assertEqual(out, "startplasma-wayland")

    def test_an_account_with_no_level_group_gets_plasma(self):
        code, out = run_dispatcher("bazzite wheel")
        self.assertEqual(code, 0)
        self.assertEqual(out, "startplasma-wayland")

    def test_two_level_groups_means_the_more_restricted_one(self):
        code, out = run_dispatcher("odd cairn-l3 cairn-l1")
        self.assertEqual(code, 0)
        self.assertEqual(out, "labwc -C /kiosk -S cairn-launcher")

    def test_a_group_name_must_match_whole(self):
        # "cairn-l1x" or "xcairn-l1" is not the L1 group.
        code, out = run_dispatcher("eve cairn-l1x xcairn-l1 cairn-l3")
        self.assertEqual(code, 0)
        self.assertEqual(out, "startplasma-wayland")


if __name__ == "__main__":
    unittest.main()
