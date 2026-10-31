#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""The launcher's wrapper as CMake made it, run with stand-ins (ADR-0041).

Standard library only. Run by CTest with the made wrapper's path. The
launcher's path in it is replaced by a stand-in that writes its arguments,
and `id` and `systemd-cat` are stand-ins on PATH, so nothing real starts.
"""

import pathlib
import re
import subprocess
import sys
import tempfile
import unittest

WRAPPER = pathlib.Path(sys.argv.pop(1)) if len(sys.argv) > 1 else None


class WrapperTest(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.state = pathlib.Path(self.folder.name)
        self.bin = self.state / "bin"
        self.bin.mkdir()
        self.record = self.state / "arguments"
        launcher = self.state / "cairn-launcher"
        launcher.write_text(f'#!/bin/bash\nprintf "%s\\n" "$@" > {self.record}\n')
        launcher.chmod(0o755)
        text = WRAPPER.read_text()
        made = re.search(r"launcher=\((\S+)/cairn-launcher ", text)
        self.assertIsNotNone(made, "the wrapper names the launcher by its full path")
        self.wrapper = self.state / "wrapper"
        self.wrapper.write_text(text.replace(made.group(1) + "/cairn-launcher", str(launcher)))
        self.stub("systemd-cat", "exit 1")

    def tearDown(self):
        self.folder.cleanup()

    def stub(self, name, body):
        path = self.bin / name
        path.write_text(f"#!/bin/bash\n{body}\n")
        path.chmod(0o755)

    def run_as(self, login_line, *arguments):
        self.stub("id", login_line)
        environment = {"PATH": f"{self.bin}:/usr/bin:/bin", "USER": "ben", "LOGNAME": "ben"}
        subprocess.run(["bash", str(self.wrapper), *arguments], env=environment, check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        return self.record.read_text().splitlines()

    def test_a_child_reads_their_own_list(self):
        arguments = self.run_as('[ "$1" = -un ] && echo ada')
        index = arguments.index("--games")
        self.assertEqual(arguments[index + 1], "/var/lib/cairn/games/ada.json")
        self.assertNotIn("/var/lib/cairn/games.json", arguments)
        self.assertIn("--start-steam", arguments)

    def test_the_login_comes_from_the_account_database_not_the_environment(self):
        # USER and LOGNAME say ben; id says ada.
        arguments = self.run_as('[ "$1" = -un ] && echo ada')
        self.assertIn("/var/lib/cairn/games/ada.json", arguments)

    def test_a_login_outside_the_rule_reads_no_list(self):
        for login in ("../ada", "Ada", "a/b", "-ada", "ada ben", "", "a" * 33):
            with self.subTest(login=login):
                arguments = self.run_as(f"printf '%s\\n' {login!r}")
                self.assertNotIn("--games", arguments)
                self.assertIn("--manifest", arguments)

    def test_no_login_still_starts_the_launcher(self):
        arguments = self.run_as("exit 1")
        self.assertNotIn("--games", arguments)
        self.assertIn("--manifest", arguments)

    def test_every_game_shows_the_picture_steps_copy(self):
        # The launcher finds each game's picture there by its app id
        # (ADR-0047), whoever logs in.
        for login_line in ('[ "$1" = -un ] && echo ada', "exit 1"):
            with self.subTest(login_line=login_line):
                arguments = self.run_as(login_line)
                index = arguments.index("--pictures")
                self.assertEqual(arguments[index + 1], "/var/lib/cairn/game-pictures")

    def test_the_sessions_own_arguments_come_last(self):
        arguments = self.run_as('[ "$1" = -un ] && echo ada', "--extra")
        self.assertEqual(arguments[-1], "--extra")


if __name__ == "__main__":
    if WRAPPER is None:
        sys.exit("usage: test_wrapper.py WRAPPER")
    unittest.main()
