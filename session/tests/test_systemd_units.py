#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Checks the copy of a child's waiting games at their logout (ADR-0041):
session/systemd/cairn-finish-games@.service, and the drop-in on systemd's
user-runtime-dir@.service that starts it.

Standard library only. systemd-analyze verify checks both where systemd
is installed, with cairn-family's path changed to a program that is there,
as it is on the image. The real run is in the VM (ADR-0041, step 6).
"""

import configparser
import pathlib
import shutil
import subprocess
import tempfile
import unittest

SESSION = pathlib.Path(__file__).resolve().parent.parent
REPO = SESSION.parent
UNIT = SESSION / "systemd" / "cairn-finish-games@.service"
DROP_IN = SESSION / "systemd" / "user-runtime-dir@.service.d" / "50-cairn-games.conf"
TMPFILES = REPO / "system_files" / "usr" / "lib" / "tmpfiles.d" / "cairn.conf"
SYSTEM_RUNTIME_DIR = pathlib.Path("/usr/lib/systemd/system/user-runtime-dir@.service")


def read_unit(path):
    # systemd keys are case-sensitive, and a key may come more than once.
    parser = configparser.ConfigParser(strict=False, interpolation=None)
    parser.optionxform = str
    parser.read(path)
    return parser


def tmpfiles_folders():
    folders = {}
    for line in TMPFILES.read_text().splitlines():
        fields = line.split()
        if fields and fields[0] == "d":
            folders[fields[1]] = fields[2:5]
    return folders


class FinishGamesUnit(unittest.TestCase):
    def setUp(self):
        self.unit = read_unit(UNIT)

    def test_runs_finish_games_for_the_instance_uid(self):
        # The provisioning path; the image's build changes it to its own.
        self.assertEqual(self.unit["Service"]["ExecStart"],
                         "/usr/local/libexec/cairn/cairn-family finish-games %i")
        self.assertEqual(self.unit["Service"]["Type"], "oneshot")

    def test_never_restarts(self):
        self.assertNotIn("Restart", self.unit["Service"])

    def test_starts_only_while_a_child_waits(self):
        folder = self.unit["Unit"]["ConditionDirectoryNotEmpty"]
        self.assertEqual(folder, "/var/lib/cairn/games-waiting")
        # The folder systemd-tmpfiles makes, root's, which only root writes.
        self.assertEqual(tmpfiles_folders().get(folder), ["0755", "root", "root"])

    def test_the_finished_folder_is_made_too(self):
        self.assertEqual(tmpfiles_folders().get("/var/lib/cairn/games-finished"),
                         ["0755", "root", "root"])

    def test_has_no_install_section(self):
        # Only the drop-in starts it; nothing enables it.
        self.assertNotIn("Install", self.unit)


class RuntimeDirDropIn(unittest.TestCase):
    def setUp(self):
        self.drop_in = read_unit(DROP_IN)

    def test_starts_the_unit_without_waiting_and_never_fails_a_logout(self):
        line = self.drop_in["Service"]["ExecStopPost"]
        self.assertEqual(line,
                         "-/usr/bin/systemctl start --no-block cairn-finish-games@%i.service")
        self.assertTrue(line.startswith("-"))
        self.assertIn("--no-block", line.split())

    def test_changes_nothing_else(self):
        self.assertEqual(list(self.drop_in.sections()), ["Service"])
        self.assertEqual(list(self.drop_in["Service"]), ["ExecStopPost"])


@unittest.skipUnless(shutil.which("systemd-analyze"), "systemd-analyze is not installed")
class Verify(unittest.TestCase):
    def verify(self, folder, unit):
        result = subprocess.run(["systemd-analyze", "verify", "--man=no", str(folder / unit)],
                                capture_output=True, text=True, check=False)
        self.assertEqual(result.returncode, 0, result.stderr)
        # It warns about an unknown key without failing.
        self.assertEqual(result.stderr, "")

    def test_the_unit(self):
        with tempfile.TemporaryDirectory() as name:
            folder = pathlib.Path(name)
            text = UNIT.read_text().replace("/usr/local/libexec/cairn/cairn-family",
                                            shutil.which("true"))
            (folder / UNIT.name).write_text(text)
            self.verify(folder, "cairn-finish-games@1002.service")

    @unittest.skipUnless(SYSTEM_RUNTIME_DIR.exists(), "systemd's user-runtime-dir@ is not here")
    def test_the_drop_in_on_systemds_own_unit(self):
        with tempfile.TemporaryDirectory() as name:
            folder = pathlib.Path(name)
            shutil.copy(SYSTEM_RUNTIME_DIR, folder)
            (folder / DROP_IN.parent.name).mkdir()
            shutil.copy(DROP_IN, folder / DROP_IN.parent.name)
            self.verify(folder, "user-runtime-dir@1002.service")


if __name__ == "__main__":
    unittest.main()
