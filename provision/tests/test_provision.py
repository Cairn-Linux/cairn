#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Loads cairn-provision.sh's steps with stand-ins for the system's commands
and checks the ones that guard secrets and accounts (#103).

Standard library only. The script only runs its steps from main when
executed, so sourcing it here defines them without touching the machine.
`id` answers from a table of accounts; `useradd`, `gpasswd`, `passwd` and
`chpasswd` write what they were asked to a log. The whole script runs for
real in the VM (provision/README.md).
"""

import os
import pathlib
import shlex
import subprocess
import tempfile
import unittest

SCRIPT = pathlib.Path(__file__).resolve().parent.parent / "cairn-provision.sh"
SECRET = "Fake-Secret-7Q2x"


def id_stub(accounts):
    """An `id` that knows `accounts`: name -> (uid, [groups])."""
    lines = ["#!/bin/bash", 'user="${@: -1}"', 'case "$user" in']
    for name, (uid, groups) in accounts.items():
        lines += [
            f"    {name})",
            f'        case "$1" in -u) echo {uid} ;; -nG) echo "{" ".join(groups)}" ;; esac',
            "        exit 0 ;;",
        ]
    lines += ['    *) echo "id: $user: no such user" >&2; exit 1 ;;', "esac"]
    return "\n".join(lines) + "\n"


def run_steps(snippet, accounts=None, env_extra=None):
    """Sources the script, sets names, runs `snippet`. Returns (code, output, log)."""
    with tempfile.TemporaryDirectory() as work:
        bin_dir = pathlib.Path(work, "bin")
        bin_dir.mkdir()
        log = pathlib.Path(work, "calls")
        stubs = {"id": id_stub(accounts or {})}
        for name in ("useradd", "gpasswd", "passwd"):
            stubs[name] = f'#!/bin/sh\necho "{name} $*" >> {log}\n'
        stubs["chpasswd"] = f'#!/bin/sh\nprintf "chpasswd stdin: " >> {log}\ncat >> {log}\n'
        for name, body in stubs.items():
            stub = bin_dir / name
            stub.write_text(body)
            stub.chmod(0o755)
        env = {"PATH": f"{bin_dir}:/usr/bin:/bin", "HOME": work}
        env.update(env_extra or {})
        command = f"source {shlex.quote(str(SCRIPT))}\n{snippet}"
        # No terminal on stdin, whoever runs the tests, so passwd is never
        # asked to prompt.
        result = subprocess.run(
            ["bash", "-c", command],
            env=env,
            stdin=subprocess.DEVNULL,
            capture_output=True,
            text=True,
            check=False,
        )
        calls = log.read_text() if log.exists() else ""
        return result.returncode, result.stdout + result.stderr, calls


NEW_GUARDIAN = "guardian=gina; child=ada; ensure_guardian"


class PasswordTest(unittest.TestCase):
    def test_the_password_is_never_printed(self):
        # xtrace is on for the whole script; the password must still not show.
        code, output, calls = run_steps(NEW_GUARDIAN, env_extra={"CAIRN_GUARDIAN_PASSWORD": SECRET})
        self.assertEqual(code, 0, output)
        self.assertIn("+ useradd", output, "tracing should be on around the step")
        self.assertNotIn(SECRET, output)
        self.assertIn(f"chpasswd stdin: gina:{SECRET}", calls)

    def test_the_password_is_never_an_argument(self):
        # Only chpasswd's stdin carries it; no command got it as an argument.
        _, _, calls = run_steps(NEW_GUARDIAN, env_extra={"CAIRN_GUARDIAN_PASSWORD": SECRET})
        for line in calls.splitlines():
            if not line.startswith("chpasswd stdin:"):
                self.assertNotIn(SECRET, line)

    def test_tracing_comes_back_and_the_variable_goes(self):
        snippet = NEW_GUARDIAN + '\necho "left: ${CAIRN_GUARDIAN_PASSWORD-unset}"\ntrue after'
        code, output, _ = run_steps(snippet, env_extra={"CAIRN_GUARDIAN_PASSWORD": SECRET})
        self.assertEqual(code, 0, output)
        self.assertIn("left: unset", output)
        self.assertIn("+ true after", output)

    def test_no_password_says_how_to_set_one(self):
        code, output, calls = run_steps(NEW_GUARDIAN)
        self.assertEqual(code, 0, output)
        self.assertIn("note: gina has no password yet", output)
        self.assertNotIn("chpasswd", calls)

    def test_without_a_terminal_passwd_is_not_run(self):
        # The tests have no terminal on stdin, as a piped or scripted run.
        _, output, calls = run_steps(NEW_GUARDIAN)
        self.assertNotIn("passwd gina", calls)
        self.assertIn("note: gina has no password yet", output)

    def test_an_existing_guardian_keeps_their_password(self):
        accounts = {"gina": (1001, ["gina", "wheel", "cairn-guardian"])}
        _, output, calls = run_steps(
            NEW_GUARDIAN, accounts, env_extra={"CAIRN_GUARDIAN_PASSWORD": SECRET}
        )
        self.assertNotIn("chpasswd", calls)
        self.assertNotIn(SECRET, output)


def check(guardian, child, accounts=None):
    return run_steps(f"guardian={shlex.quote(guardian)}; child={shlex.quote(child)}; check_accounts", accounts)


class AccountCheckTest(unittest.TestCase):
    def test_new_accounts_pass(self):
        code, output, _ = check("gina", "ada")
        self.assertEqual(code, 0, output)

    def test_an_existing_plain_child_passes(self):
        code, output, _ = check("gina", "ada", {"ada": (1002, ["ada", "cairn-l1"])})
        self.assertEqual(code, 0, output)

    def test_refusals(self):
        cases = {
            "child in wheel": ("gina", "ada", {"ada": (1002, ["ada", "wheel"])}),
            "child is a Guardian": ("gina", "ada", {"ada": (1002, ["ada", "cairn-guardian"])}),
            "child is a system account": ("gina", "sddm", {"sddm": (952, ["sddm"])}),
            "child is root": ("gina", "root", {"root": (0, ["root"])}),
            "guardian is a system account": ("root", "ada", {"root": (0, ["root"])}),
            "Guardian is a child": ("ada", "ben", {"ada": (1002, ["ada", "cairn-l1"])}),
            "Guardian is an older child": ("cy", "ben", {"cy": (1003, ["cy", "cairn-l3"])}),
            "same account twice": ("ada", "ada", None),
            "not a login name": ("gina", "ada;rm -rf", None),
            "capital letters": ("gina", "Ada", None),
            "an option in disguise": ("--help", "ada", None),
        }
        for label, (guardian, child, accounts) in cases.items():
            with self.subTest(label):
                code, output, calls = check(guardian, child, accounts)
                self.assertEqual(code, 1, output)
                self.assertIn("refused:", output)
                self.assertEqual(calls, "", "nothing may change before the checks pass")


def move(groups, wanted):
    return run_steps(f"set_level_group ada {wanted}", {"ada": (1002, ["ada", *groups])})


class LevelGroupTest(unittest.TestCase):
    def test_the_new_level_is_added_before_the_old_one_goes(self):
        # Cut off in between, the child is in two levels and gets the more
        # restricted session, never in none (#100).
        for old, new in (("cairn-l1", "cairn-l3"), ("cairn-l3", "cairn-l1")):
            with self.subTest(old=old, new=new):
                code, output, calls = move([old], new)
                self.assertEqual(code, 0, output)
                self.assertEqual(calls.splitlines(), [f"gpasswd -a ada {new}", f"gpasswd -d ada {old}"])

    def test_already_there_changes_nothing(self):
        code, output, calls = move(["cairn-l1"], "cairn-l1")
        self.assertEqual(code, 0, output)
        self.assertEqual(calls, "")


def launcher_wrapper():
    """The wrapper install_launcher writes, as the file it installs."""
    text = SCRIPT.read_text()
    start = text.index("<<'WRAPPER'\n") + len("<<'WRAPPER'\n")
    return text[start:text.index("\nWRAPPER\n", start) + 1]


class LauncherWrapperTest(unittest.TestCase):
    # The launcher's notes for a grown-up go to the journal under their own
    # name (#122), with every option the kiosk needs and any it is given.
    def test_starts_the_launcher_under_its_own_journal_name(self):
        with tempfile.TemporaryDirectory() as work:
            wrapper = pathlib.Path(work, "cairn-launcher")
            wrapper.write_text(launcher_wrapper())
            stub = pathlib.Path(work, "systemd-cat")
            stub.write_text('#!/bin/sh\necho "systemd-cat $*"\n')
            stub.chmod(0o755)
            result = subprocess.run(
                ["bash", str(wrapper), "--extra"],
                env={"PATH": f"{work}:/usr/bin:/bin"},
                capture_output=True,
                text=True,
                check=False,
            )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(
            result.stdout.strip(),
            "systemd-cat --identifier=cairn-launcher /usr/local/libexec/cairn/cairn-launcher"
            " --manifest /usr/local/share/cairn/manifest.json"
            " --log-out /usr/local/bin/cairn-log-out --scope-apps --start-steam --extra",
        )

    def test_no_journal_still_starts_the_launcher(self):
        # The fallback runs the launcher itself, after the journal is tried.
        lines = launcher_wrapper().splitlines()
        self.assertEqual(lines[-1], 'exec "${launcher[@]}"')
        self.assertLess(
            next(i for i, line in enumerate(lines) if "systemd-cat --identifier=cairn-launcher true" in line),
            len(lines) - 1,
        )


class InstallFileTest(unittest.TestCase):
    def install(self, make_source):
        with tempfile.TemporaryDirectory() as work:
            repo = pathlib.Path(work, "repo")
            repo.mkdir()
            outside = pathlib.Path(work, "outside")
            outside.write_text("not ours\n")
            source = make_source(repo, outside)
            target = pathlib.Path(work, "installed")
            snippet = (
                f"REPO={shlex.quote(str(repo))}; changed() {{ :; }}\n"
                f"install_file 644 {shlex.quote(str(source))} {shlex.quote(str(target))}"
            )
            code, output, _ = run_steps(snippet)
            return code, output, target.exists()

    def test_a_file_in_the_checkout_installs(self):
        def make(repo, _):
            (repo / "file").write_text("ours\n")
            return repo / "file"

        code, output, installed = self.install(make)
        self.assertEqual(code, 0, output)
        self.assertTrue(installed)

    def test_a_link_within_the_checkout_installs(self):
        def make(repo, _):
            (repo / "file").write_text("ours\n")
            (repo / "link").symlink_to("file")
            return repo / "link"

        code, output, installed = self.install(make)
        self.assertEqual(code, 0, output)
        self.assertTrue(installed)

    def test_a_link_out_of_the_checkout_is_refused(self):
        def make(repo, outside):
            (repo / "link").symlink_to(outside)
            return repo / "link"

        code, output, installed = self.install(make)
        self.assertEqual(code, 1, output)
        self.assertIn("leads outside the checkout", output)
        self.assertFalse(installed)

    def test_a_missing_file_is_refused(self):
        code, output, installed = self.install(lambda repo, _: repo / "missing")
        self.assertEqual(code, 1, output)
        self.assertFalse(installed)


class ScriptTextTest(unittest.TestCase):
    def setUp(self):
        self.text = SCRIPT.read_text()

    def test_no_fixed_paths_in_tmp(self):
        # A fixed name in a world-writable directory can be planted in advance.
        self.assertNotIn("/tmp/", self.text)

    def test_a_downloaded_package_is_verified_before_it_is_installed(self):
        body = self.text.split("layer_backgrounds_compat() {", 1)[1].split("\n}\n", 1)[0]
        self.assertIn('rpm -K "$dir"', body)
        self.assertLess(body.index("rpm -K"), body.index("rpm-ostree install"))


if __name__ == "__main__":
    unittest.main()
