#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Runs bin/cairn-session with stand-ins on PATH and checks which session it
picks for each level group (ADR-0012).

Standard library only. The stand-ins are tiny shell scripts: `id` prints
the groups a test asks for, and `labwc`, `startplasma-wayland`, `systemctl`
and `systemd-cat` print their name and arguments instead of starting
anything.
"""

import os
import pathlib
import subprocess
import tempfile
import unittest

DISPATCHER = pathlib.Path(__file__).resolve().parent.parent / "bin" / "cairn-session"


def run_dispatcher(groups, systemctl_fails=False, journal_missing=False):
    with tempfile.TemporaryDirectory() as bin_dir:
        stubs = {
            "id": f'#!/bin/sh\necho "{groups}"\n',
            "labwc": '#!/bin/sh\necho "labwc $*"\n',
            "startplasma-wayland": '#!/bin/sh\necho "startplasma-wayland $*"\n',
            "systemctl": f'#!/bin/sh\necho "systemctl $*"\nexit {1 if systemctl_fails else 0}\n',
            "systemd-cat": f'#!/bin/sh\necho "systemd-cat $*"\nexit {1 if journal_missing else 0}\n',
        }
        for name, body in stubs.items():
            stub = pathlib.Path(bin_dir, name)
            stub.write_text(body)
            stub.chmod(0o755)
        env = {"PATH": f"{bin_dir}:/usr/bin:/bin", "CAIRN_LABWC_CONFIG": "/kiosk"}
        result = subprocess.run(
            [str(DISPATCHER)], env=env, capture_output=True, text=True, check=False
        )
        lines = result.stdout.strip().splitlines()
        # The session itself is the last thing started; anything before it
        # is returned apart.
        return result.returncode, lines[-1] if lines else "", lines[:-1]


# Everything the kiosk prints goes to the journal (#122).
KIOSK = "systemd-cat --identifier=cairn-kiosk labwc -C /kiosk -S cairn-launcher"
AUDIO = "systemctl --user start pipewire-pulse.service"


class CairnSessionTest(unittest.TestCase):
    def test_the_kiosk_starts_sound_before_the_launcher(self):
        # The first app hung while PipeWire started on demand (#87).
        for groups in ("ada cairn-l1", "ben cairn-l2"):
            with self.subTest(groups=groups):
                code, out, before = run_dispatcher(groups)
                self.assertEqual(code, 0)
                self.assertEqual(out, KIOSK)
                self.assertEqual(before, [AUDIO])

    def test_no_journal_still_logs_the_child_in(self):
        # Without a journal to write to, the kiosk starts as it always did and
        # its output goes where SDDM sends it.
        code, out, before = run_dispatcher("ada cairn-l1", journal_missing=True)
        self.assertEqual(code, 0)
        self.assertEqual(out, "labwc -C /kiosk -S cairn-launcher")
        self.assertEqual(before, [AUDIO])

    def test_no_sound_still_logs_the_child_in(self):
        code, out, before = run_dispatcher("ada cairn-l1", systemctl_fails=True)
        self.assertEqual(code, 0)
        self.assertEqual(out, KIOSK)
        self.assertEqual(before, [AUDIO])

    def test_plasma_starts_its_own_sound(self):
        for groups in ("cat cairn-l3", "guardian wheel cairn-guardian", "bazzite wheel"):
            with self.subTest(groups=groups):
                _, out, before = run_dispatcher(groups)
                self.assertEqual(out, "startplasma-wayland")
                self.assertEqual(before, [])

    def test_l1_gets_the_kiosk_with_the_launcher(self):
        code, out, _ = run_dispatcher("ada cairn-l1")
        self.assertEqual(code, 0)
        self.assertEqual(out, KIOSK)

    def test_l2_gets_the_kiosk_too(self):
        code, out, _ = run_dispatcher("ben cairn-l2")
        self.assertEqual(code, 0)
        self.assertEqual(out, KIOSK)

    def test_l3_l4_and_guardian_get_plasma(self):
        for groups in ("cat cairn-l3", "dan cairn-l4", "guardian wheel cairn-guardian"):
            with self.subTest(groups=groups):
                code, out, _ = run_dispatcher(groups)
                self.assertEqual(code, 0)
                self.assertEqual(out, "startplasma-wayland")

    def test_an_administrator_gets_plasma_whatever_else_they_are_in(self):
        # PAM asked them for a password, so they are never a child (#101).
        for groups in (
            "bazzite wheel",
            "dad cairn-guardian cairn-l1",
            "mum wheel cairn-l2",
            "gran cairn-guardian cairn-l3",
        ):
            with self.subTest(groups=groups):
                code, out, before = run_dispatcher(groups)
                self.assertEqual(code, 0)
                self.assertEqual(out, "startplasma-wayland")
                self.assertEqual(before, [])

    def test_an_account_cairn_did_not_make_gets_plasma(self):
        # Made in System Settings, say. Adopting it is the Guardian tool's job.
        code, out, _ = run_dispatcher("grandpa")
        self.assertEqual(code, 0)
        self.assertEqual(out, "startplasma-wayland")

    def test_two_level_groups_means_the_more_restricted_one(self):
        code, out, _ = run_dispatcher("odd cairn-l3 cairn-l1")
        self.assertEqual(code, 0)
        self.assertEqual(out, KIOSK)

    def test_a_group_name_must_match_whole(self):
        # "cairn-l1x" or "xcairn-l1" is not the L1 group.
        code, out, _ = run_dispatcher("eve cairn-l1x xcairn-l1 cairn-l3")
        self.assertEqual(code, 0)
        self.assertEqual(out, "startplasma-wayland")


if __name__ == "__main__":
    unittest.main()
