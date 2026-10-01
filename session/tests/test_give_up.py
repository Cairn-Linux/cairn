#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Checks that cairn-give-up does what the README, ADR-0018 and ADR-0025
promise.

Most of it is read as text, the way the polkit rule is tested. One test runs
the helper for real against a frozen stand-in in a systemd user scope, when
the machine running the tests has a user manager; the whole helper is also
run in the VM (docs/research/kiosk-containment.md).
"""

import os
import pathlib
import re
import signal
import subprocess
import tempfile
import time
import unittest
import uuid

HELPER = pathlib.Path(__file__).resolve().parent.parent / "bin" / "cairn-give-up"


class GiveUpHelperTest(unittest.TestCase):
    def setUp(self):
        self.text = HELPER.read_text()

    def test_has_the_licence_line(self):
        self.assertTrue(self.text.startswith("#!/bin/bash\n# SPDX-License-Identifier: Apache-2.0"))

    def test_matches_the_scopes_and_sandbox_roots_and_not_the_frame(self):
        # The launcher's scopes hold native programs (#98); bwrap is every
        # Flatpak app; reaper is every Steam game. The launcher, the
        # compositor and the Steam client are none of these.
        self.assertIn("cairn-app-*.scope", self.text)
        self.assertIn("bwrap", self.text)
        self.assertIn("reaper SteamLaunch", self.text)
        for safe in ("cairn-launcher", "labwc", "steamwebhelper", "pipewire"):
            self.assertNotIn(safe, self.text, f"{safe} must never be a target")

    def test_continues_a_stopped_process_before_killing_it(self):
        # A stopped process cannot act on TERM or KILL until it is continued,
        # so every CONT comes first, and every KILL after every TERM.
        order = [m.group(1) for m in re.finditer(r"--signal (CONT|TERM|KILL)", self.text)]
        self.assertEqual(order, ["CONT", "CONT", "TERM", "TERM", "KILL", "KILL"])

    def test_tells_the_launcher_last(self):
        # The launcher goes back to the tiles when this file changes
        # (ADR-0026), so it is written after every signal has been sent.
        told = self.text.index('> "$told"')
        self.assertGreater(told, self.text.rindex("--signal KILL"))
        self.assertIn("/cairn/give-up", self.text)

    def test_only_ever_signals_the_childs_own_processes(self):
        # Never another user's: --uid on every pkill, and only this user's
        # own systemd manager.
        for line in self.text.splitlines():
            if line.startswith("pkill"):
                self.assertIn("--uid", line, f"pkill without --uid: {line.strip()}")
            if line.startswith("systemctl"):
                self.assertIn("--user", line, f"systemctl without --user: {line.strip()}")


def user_manager_available():
    try:
        result = subprocess.run(
            ["systemd-run", "--user", "--scope", "--quiet", "--collect", "true"],
            capture_output=True,
            timeout=10,
            check=False,
        )
    except (OSError, subprocess.TimeoutExpired):
        return False
    return result.returncode == 0


def wait_for(condition, seconds=5):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        if condition():
            return True
        time.sleep(0.05)
    return condition()


def state(pid):
    for line in pathlib.Path(f"/proc/{pid}/status").read_text().splitlines():
        if line.startswith("State:"):
            return line.split()[1]
    return ""


class GiveUpWithNoLauncherTest(unittest.TestCase):
    def test_makes_no_file_when_no_launcher_listens(self):
        with tempfile.TemporaryDirectory() as bin_dir:
            for name in ("pkill", "systemctl"):
                stub = pathlib.Path(bin_dir, name)
                stub.write_text("#!/bin/sh\nexit 1\n")
                stub.chmod(0o755)
            told = pathlib.Path(bin_dir, "cairn", "give-up")
            env = dict(os.environ, PATH=f"{bin_dir}:/usr/bin:/bin", CAIRN_GIVE_UP_FILE=str(told))
            result = subprocess.run(
                [str(HELPER)], env=env, capture_output=True, text=True, timeout=30, check=False
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertFalse(told.exists())


@unittest.skipUnless(user_manager_available(), "needs a systemd user manager")
class GiveUpEndsAScopedProgramTest(unittest.TestCase):
    def test_a_frozen_native_program_ends_and_everything_else_stays(self):
        tag = f"cairn-test-{os.getpid()}-{uuid.uuid4().hex[:8]}"
        with tempfile.TemporaryDirectory() as bin_dir:
            # The sandbox roots on the machine running the test are someone's
            # real apps, so pkill is a stand-in that matches nothing.
            stub = pathlib.Path(bin_dir, "pkill")
            stub.write_text("#!/bin/sh\nexit 1\n")
            stub.chmod(0o755)
            # A native program as the launcher starts it, and a program in no
            # scope of ours standing in for the frame.
            app = subprocess.Popen(
                ["systemd-run", "--user", "--scope", "--quiet", "--collect",
                 f"--unit={tag}-app", "--", "sleep", "300"]
            )
            frame = subprocess.Popen(["sleep", "300"])
            try:
                self.assertTrue(wait_for(lambda: tag in pathlib.Path(f"/proc/{app.pid}/cgroup").read_text()))
                os.kill(app.pid, signal.SIGSTOP)
                self.assertTrue(wait_for(lambda: state(app.pid) == "T"), "the stand-in should be frozen")

                # The file the launcher would have made and watches.
                told = pathlib.Path(bin_dir, "give-up")
                told.touch()
                env = dict(
                    os.environ,
                    PATH=f"{bin_dir}:/usr/bin:/bin",
                    CAIRN_APP_UNITS=f"{tag}-*.scope",
                    CAIRN_GIVE_UP_FILE=str(told),
                )
                result = subprocess.run(
                    [str(HELPER)], env=env, capture_output=True, text=True, timeout=30, check=False
                )
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertTrue(told.read_text().strip().isdigit(), "the launcher is told")

                app.wait(timeout=10)
                self.assertIn(app.returncode, (-signal.SIGTERM, -signal.SIGKILL))
                self.assertIsNone(frame.poll(), "a program outside the scopes must keep running")
            finally:
                for process in (app, frame):
                    if process.poll() is None:
                        os.kill(process.pid, signal.SIGCONT)
                        process.kill()
                        process.wait()


if __name__ == "__main__":
    unittest.main()
