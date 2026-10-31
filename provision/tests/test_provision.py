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

import json
import os
import pathlib
import shlex
import shutil
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


def getent_stub(gecos=None, groups=None):
    """A `getent` for passwd lines, with `gecos`: name -> comment field, and
    group lines, with `groups`: name -> [members]."""
    lines = ["#!/bin/bash", 'case "$1 $2" in']
    for name, comment in (gecos or {}).items():
        lines.append(f'    "passwd {name}") echo "{name}:x:1002:1002:{comment}:/home/{name}:/bin/bash" ;;')
    for name, members in (groups or {}).items():
        lines.append(f'    "group {name}") echo "{name}:x:900:{",".join(members)}" ;;')
    lines += ["    *) exit 2 ;;", "esac"]
    return "\n".join(lines) + "\n"


def run_steps(snippet, accounts=None, env_extra=None, extra_stubs=None, script=SCRIPT):
    """Sources the script, sets names, runs `snippet`. Returns (code, output, log).
    No test may see the computer it runs on as one that runs the image, so
    IMAGE_MARK names a file that is never there unless `snippet` sets it."""
    with tempfile.TemporaryDirectory() as work:
        bin_dir = pathlib.Path(work, "bin")
        bin_dir.mkdir()
        log = pathlib.Path(work, "calls")
        stubs = {"id": id_stub(accounts or {}), **(extra_stubs or {})}
        for name in ("useradd", "gpasswd", "passwd"):
            stubs[name] = f'#!/bin/sh\necho "{name} $*" >> {log}\n'
        stubs["chpasswd"] = f'#!/bin/sh\nprintf "chpasswd stdin: " >> {log}\ncat >> {log}\n'
        for name, body in stubs.items():
            stub = bin_dir / name
            stub.write_text(body)
            stub.chmod(0o755)
        env = {"PATH": f"{bin_dir}:/usr/bin:/bin", "HOME": work}
        env.update(env_extra or {})
        missing = shlex.quote(str(pathlib.Path(work, "no-image")))
        command = f"source {shlex.quote(str(script))}\nIMAGE_MARK={missing}\n{snippet}"
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


NEW_GUARDIAN = "guardian=gina; children=(ada); levels=(1); ensure_guardian"


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


def check(guardian, children, accounts=None):
    """check_accounts for a Guardian and one child, or a list of children."""
    names = [children] if isinstance(children, str) else children
    quoted = " ".join(shlex.quote(name) for name in names)
    return run_steps(f"guardian={shlex.quote(guardian)}; children=({quoted}); check_accounts", accounts)


class AccountCheckTest(unittest.TestCase):
    def test_new_accounts_pass(self):
        code, output, _ = check("gina", "ada")
        self.assertEqual(code, 0, output)

    def test_an_existing_plain_child_passes(self):
        code, output, _ = check("gina", "ada", {"ada": (1002, ["ada", "cairn-l1"])})
        self.assertEqual(code, 0, output)

    def test_two_children_pass(self):
        code, output, _ = check("gina", ["ada", "ben"], {"ben": (1003, ["ben", "cairn-l1"])})
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
            "the same child twice": ("gina", ["ada", "ada"], None),
            "a second child that is the Guardian": ("gina", ["ada", "gina"], None),
        }
        for label, (guardian, child, accounts) in cases.items():
            with self.subTest(label):
                code, output, calls = check(guardian, child, accounts)
                self.assertEqual(code, 1, output)
                self.assertIn("refused:", output)
                self.assertEqual(calls, "", "nothing may change before the checks pass")


def parse(*arguments):
    """parse_arguments, then what it set: the children and their levels."""
    quoted = " ".join(shlex.quote(a) for a in arguments)
    return run_steps(f'parse_arguments {quoted}; echo "${{children[*]}}|${{levels[*]}}|$guardian"')


class ArgumentsTest(unittest.TestCase):
    # Two cousins at two levels, for the first test on a laptop.
    def test_children_at_level_1_unless_a_level_follows(self):
        code, output, _ = parse("--guardian", "gina", "--child", "ada", "--child", "ben:2")
        self.assertEqual(code, 0, output)
        # The script traces every command, so the echo's own line is looked for.
        self.assertIn("ada ben|1 2|gina", output.splitlines())

    def test_only_the_kiosk_levels_are_given(self):
        for level in ("3", "4", "0", "two", ""):
            with self.subTest(level=level):
                code, output, _ = parse("--guardian", "gina", "--child", f"ben:{level}")
                self.assertEqual(code, 1, output)
                self.assertIn("refused:", output)

    def test_a_guardian_and_a_child_are_needed(self):
        for arguments in (("--guardian", "gina"), ("--child", "ada"), ()):
            with self.subTest(arguments=arguments):
                code, output, _ = parse(*arguments)
                self.assertEqual(code, 2, output)
                self.assertIn("usage:", output)


class ChildrenTest(unittest.TestCase):
    def test_each_child_is_made_at_its_own_level(self):
        code, output, calls = run_steps(
            "changed() { :; }; children=(ada ben); levels=(1 2); ensure_children"
        )
        self.assertEqual(code, 0, output)
        lines = calls.splitlines()
        for expected in ("useradd --create-home ada", "passwd --lock ada", "gpasswd -a ada cairn-l1",
                         "useradd --create-home ben", "passwd --lock ben", "gpasswd -a ben cairn-l2"):
            self.assertIn(expected, lines)


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
            " --manifest /usr/local/share/cairn/manifest.json --games /var/lib/cairn/games.json"
            " --pictures /var/lib/cairn/game-pictures"
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


class SteamForEntryTest(unittest.TestCase):
    # "Steam for <child>" in a Guardian's menu (ADR-0032): one entry for every
    # child account, never for an administrator in a level group.
    def test_every_child_and_no_administrator(self):
        accounts = {
            "ada": (1002, ["ada", "cairn-l1"]),
            "ben": (1003, ["ben", "cairn-l3"]),
            "bazzite": (1000, ["bazzite", "wheel", "cairn-l1"]),
            "gina": (1001, ["gina", "cairn-guardian", "cairn-l2"]),
        }
        groups = {"cairn-l1": ["ada", "bazzite"], "cairn-l2": ["gina"], "cairn-l3": ["ben"], "cairn-l4": []}
        code, output, _ = run_steps(
            "child_accounts", accounts, extra_stubs={"getent": getent_stub(groups=groups)}
        )
        self.assertEqual(code, 0, output)
        # Without the trace, which starts each of its lines with a +.
        self.assertEqual([line for line in output.splitlines() if not line.startswith("+")], ["ada", "ben"])

    def test_the_entry_runs_the_window_for_that_child(self):
        code, output, _ = run_steps(
            "steam_entry ada", extra_stubs={"getent": getent_stub(gecos={"ada": ""})}
        )
        self.assertEqual(code, 0, output)
        lines = output.splitlines()
        self.assertIn("Name=Steam for ada", lines)
        self.assertIn("Exec=/usr/local/bin/cairn-steam-for ada", lines)
        # Plasma leaves the entry out for an account that cannot run this.
        self.assertIn("TryExec=/usr/local/bin/cairn-steam-for", lines)

    def test_a_full_name_is_the_name_shown(self):
        code, output, _ = run_steps(
            "steam_entry ada", extra_stubs={"getent": getent_stub(gecos={"ada": "Ada Lovelace,,,"})}
        )
        self.assertEqual(code, 0, output)
        self.assertIn("Name=Steam for Ada Lovelace", output.splitlines())
        self.assertIn("Exec=/usr/local/bin/cairn-steam-for ada", output.splitlines())


class SteamEntriesTest(unittest.TestCase):
    """make_steam_entries writes into $APPS, the machine's own folder, and
    removes the entry of an account that is no longer a child."""

    def run_step(self, prefix=None):
        with tempfile.TemporaryDirectory() as work:
            apps = pathlib.Path(work, "applications")
            apps.mkdir()
            (apps / "cairn-steam-for-cy.desktop").write_text("old\n")
            (apps / "other.desktop").write_text("keep\n")
            accounts = {"ada": (1002, ["ada", "cairn-l1"]), "ben": (1003, ["ben", "cairn-l2"])}
            groups = {"cairn-l1": ["ada"], "cairn-l2": ["ben"], "cairn-l3": [], "cairn-l4": []}
            stubs = {
                "getent": getent_stub(gecos={"ada": "Ada", "ben": ""}, groups=groups),
                "chgrp": '#!/bin/sh\necho "chgrp $*" >> "$HOME/calls"\n',
                "chmod": '#!/bin/sh\necho "chmod $*" >> "$HOME/calls"\n',
            }
            set_prefix = f"PREFIX={prefix}\n" if prefix else ""
            code, output, _ = run_steps(
                f"{set_prefix}APPS={shlex.quote(str(apps))}\nmake_steam_entries",
                accounts, extra_stubs=stubs,
            )
            files = {path.name: path.read_text() for path in apps.iterdir()}
            calls = pathlib.Path(work, "calls")
            return code, output, files, calls.exists()

    def test_one_entry_per_child_in_the_machines_folder(self):
        code, output, files, chmodded = self.run_step()
        self.assertEqual(code, 0, output)
        self.assertEqual(sorted(files), [
            "cairn-steam-for-ada.desktop", "cairn-steam-for-ben.desktop", "other.desktop"])
        self.assertIn("Name=Steam for Ada\n", files["cairn-steam-for-ada.desktop"])
        self.assertIn("Exec=/usr/local/bin/cairn-steam-for ben\n", files["cairn-steam-for-ben.desktop"])
        self.assertIn("removed", output)
        self.assertFalse(chmodded, "the entries never change the window")

    def test_on_the_image_the_entries_name_its_window(self):
        code, output, files, chmodded = self.run_step(prefix="/usr")
        self.assertEqual(code, 0, output)
        entry = files["cairn-steam-for-ada.desktop"]
        self.assertIn("Exec=/usr/bin/cairn-steam-for ada\n", entry)
        self.assertIn("TryExec=/usr/bin/cairn-steam-for\n", entry)
        self.assertNotIn("cairn-steam-for-cy.desktop", files)
        self.assertFalse(chmodded)

    def test_the_folder_is_the_one_plasma_reads(self):
        self.assertIn("\nAPPS=/usr/local/share/applications\n", SCRIPT.read_text())


def window_wrapper():
    """The wrapper install_steam_for writes, as the file it installs."""
    text = SCRIPT.read_text()
    start = text.index("<<'WINDOW'\n") + len("<<'WINDOW'\n")
    return text[start:text.index("\nWINDOW\n", start) + 1]


class WindowWrapperTest(unittest.TestCase):
    # The window's notes and the root helper's trace go to the journal under
    # their own name, and systemd-cat runs the window in its own place.
    def test_starts_the_window_under_its_own_journal_name(self):
        with tempfile.TemporaryDirectory() as work:
            wrapper = pathlib.Path(work, "cairn-steam-for")
            wrapper.write_text(window_wrapper())
            stub = pathlib.Path(work, "systemd-cat")
            stub.write_text('#!/bin/sh\necho "systemd-cat $*"\n')
            stub.chmod(0o755)
            result = subprocess.run(
                ["bash", str(wrapper), "ada"],
                env={"PATH": f"{work}:/usr/bin:/bin"},
                capture_output=True,
                text=True,
                check=False,
            )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(
            result.stdout.strip(),
            "systemd-cat --identifier=cairn-steam-for /usr/local/libexec/cairn/cairn-steam-for ada",
        )


class BuildCheckTest(unittest.TestCase):
    """Provisioning installs only a build configured for its /usr/local."""

    def check(self, cache):
        with tempfile.TemporaryDirectory() as work:
            if cache is not None:
                build = pathlib.Path(work, "build/release")
                build.mkdir(parents=True)
                (build / "CMakeCache.txt").write_text(cache)
            return run_steps(f"REPO={shlex.quote(work)}; check_build")[:2]

    def test_the_release_preset_passes(self):
        code, output = self.check("CMAKE_BUILD_TYPE:STRING=Release\nCMAKE_INSTALL_PREFIX:PATH=/usr/local\n")
        self.assertEqual(code, 0, output)

    def test_a_build_for_the_image_is_refused(self):
        code, output = self.check("CMAKE_INSTALL_PREFIX:PATH=/usr\n")
        self.assertEqual(code, 1, output)
        self.assertIn("refused: build/release is not configured for /usr/local", output)
        self.assertIn("cmake --preset release", output)

    def test_no_build_is_refused(self):
        code, output = self.check(None)
        self.assertEqual(code, 1, output)
        self.assertIn("cmake --preset release", output)


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


class SharedLibraryTest(unittest.TestCase):
    """The folder a Guardian's Steam installs the children's games into
    (ADR-0033): stat and getfacl answer from the scenario, and chown, chmod
    and setfacl write what they were asked to a log."""

    WANTED_ACL = "\n".join([
        "user::rwx", "group::rwx", "group:cairn-guardian:rwx", "mask::rwx", "other::r-x",
        "default:user::rwx", "default:group::rwx", "default:group:cairn-guardian:rwx",
        "default:mask::rwx", "default:other::r-x",
    ])

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.state = pathlib.Path(self.directory.name)
        self.library = self.state / "steam-library"
        self.stubs = {
            "stat": '#!/bin/bash\ncat "$STATE/stat-answer" 2> /dev/null || echo root:root:755\n',
            "getfacl": '#!/bin/bash\ncat "$STATE/acl-answer" 2> /dev/null\n',
        }
        for name in ("chown", "chmod", "setfacl"):
            self.stubs[name] = f'#!/bin/bash\necho "{name} $*" >> "$STATE/calls"\n'

    def tearDown(self):
        self.directory.cleanup()

    def run_step(self):
        snippet = f"SHARED_LIBRARY={shlex.quote(str(self.library))}\nmake_shared_library\necho after"
        code, output, _ = run_steps(snippet, env_extra={"STATE": str(self.state)}, extra_stubs=self.stubs)
        calls = self.state / "calls"
        return code, output, calls.read_text().splitlines() if calls.exists() else []

    def test_makes_both_folders_for_guardians_only(self):
        code, output, calls = self.run_step()
        self.assertEqual(code, 0, output)
        steamapps = self.library / "steamapps"
        self.assertTrue(steamapps.is_dir())
        self.assertEqual(oct(self.library.stat().st_mode & 0o7777), oct(0o2775))
        acl = ("u::rwx,g::rwx,g:cairn-guardian:rwx,m::rwx,o::r-x,"
               "d:u::rwx,d:g::rwx,d:g:cairn-guardian:rwx,d:m::rwx,d:o::r-x")
        for folder in (self.library, steamapps):
            self.assertIn(f"chown root:cairn-guardian {folder}", calls)
            self.assertIn(f"chmod 2775 {folder}", calls)
            self.assertIn(f"setfacl --set {acl} -- {folder}", calls)
            self.assertIn(f"changed: {folder}", output.splitlines())
        self.assertIn("after", output.splitlines())

    def test_a_second_run_changes_nothing(self):
        (self.library / "steamapps").mkdir(parents=True)
        (self.state / "stat-answer").write_text("root:cairn-guardian:2775\n")
        (self.state / "acl-answer").write_text(self.WANTED_ACL + "\n\n")
        code, output, calls = self.run_step()
        self.assertEqual(code, 0, output)
        self.assertEqual(calls, [])
        self.assertNotIn("changed:", output)

    def test_never_walks_the_games_inside(self):
        (self.library / "steamapps" / "common" / "A Game").mkdir(parents=True)
        code, output, calls = self.run_step()
        self.assertEqual(code, 0, output)
        folders = {str(self.library), str(self.library / "steamapps")}
        for call in calls:
            self.assertNotIn(" -R", call)
            self.assertIn(call.split()[-1], folders)

    def test_a_link_is_refused(self):
        elsewhere = self.state / "elsewhere"
        elsewhere.mkdir()
        self.library.symlink_to(elsewhere)
        code, output, calls = self.run_step()
        self.assertEqual(code, 1, output)
        self.assertIn("refused:", output)
        self.assertEqual(calls, [])


# A stand-in for the visit helper's copy mode: each child's lines come from
# the scenario, and so does how it ends.
COPY_HELPER = r"""#!/bin/bash
[ "$1" = --copy-games ] && [ "$3" = --all ] && [ "$#" -eq 3 ] || exit 2
touch "$STATE/$2-ran"
cat "$STATE/$2-says" 2> /dev/null
exit "$(cat "$STATE/$2-status" 2> /dev/null || echo 0)"
"""

MANIFEST = """"AppState"
{
	"appid"		"%s"
	"name"		"%s"
	"installdir"		"A Game"
	"StateFlags"		"4"
}
"""


class CopyGamesStepTest(unittest.TestCase):
    """How provisioning turns the helper's copy lines into its own sentences
    (ADR-0033)."""

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.state = pathlib.Path(self.directory.name)
        libexec = self.state / "prefix" / "libexec" / "cairn"
        libexec.mkdir(parents=True)
        helper = libexec / "cairn-steam-visit"
        helper.write_text(COPY_HELPER)
        helper.chmod(0o755)
        kidscan = SCRIPT.parents[1] / "tools" / "kidscan" / "kidscan.py"
        (libexec / "kidscan.py").write_text(kidscan.read_text())
        self.steamapps = self.state / "steam-library" / "steamapps"
        self.steamapps.mkdir(parents=True)
        self.game(294690, "Pajama Sam")

    def tearDown(self):
        self.directory.cleanup()

    def game(self, appid, name):
        (self.steamapps / f"appmanifest_{appid}.acf").write_text(MANIFEST % (appid, name))

    def says(self, child, *lines, status=0):
        (self.state / f"{child}-says").write_text("".join(line + "\n" for line in lines))
        (self.state / f"{child}-status").write_text(f"{status}\n")

    def run_step(self, children=("ada",)):
        snippet = (
            f"PREFIX={shlex.quote(str(self.state / 'prefix'))}\n"
            f"SHARED_LIBRARY={shlex.quote(str(self.state / 'steam-library'))}\n"
            f"child_accounts() {{ printf '%s\\n' {' '.join(children)}; }}\n"
            "copy_games\ntrue after\necho \"changes: $changes\""
        )
        code, output, _ = run_steps(snippet, env_extra={"STATE": str(self.state)})
        self.assertEqual(code, 0, output)
        return output

    def said(self, output):
        """What the step printed, without the trace."""
        return [line for line in output.splitlines() if not line.startswith("+")]

    def test_a_copied_game_is_a_change_named_from_the_shared_folder(self):
        self.says("ada", "copied 294690", "finished")
        output = self.run_step()
        self.assertEqual(self.said(output), ["changed: ada's own copy of Pajama Sam", "changes: 1"])

    def test_what_the_child_has_already_is_not_mentioned(self):
        self.says("ada", "have 294690", "finished")
        self.assertEqual(self.said(self.run_step()), ["changes: 0"])

    def test_a_name_loses_what_a_terminal_would_act_on(self):
        self.game(12, "Bad\x1b]0;title\x07 game\u202e")
        self.says("ada", "copied 12")
        output = self.run_step()
        self.assertIn("changed: ada's own copy of Bad]0;title game", self.said(output))
        self.assertNotIn("\x1b", output)
        self.assertNotIn("\x07", output)
        self.assertNotIn("\u202e", output)

    def test_a_game_without_a_manifest_is_named_by_its_number(self):
        self.says("ada", "copied 7")
        self.assertIn("changed: ada's own copy of game 7", self.said(self.run_step()))

    def name_of(self, appid):
        """What game_name says, from a run that cannot hang the tests: one
        that waits is stopped after a while, and says so."""
        snippet = (
            f"PREFIX={shlex.quote(str(self.state / 'prefix'))}\n"
            f"SHARED_LIBRARY={shlex.quote(str(self.state / 'steam-library'))}\n"
            "export PREFIX SHARED_LIBRARY\nexport -f game_name\n"
            f"timeout 20 bash -c 'game_name {appid}' || echo 'timed out'"
        )
        code, output, _ = run_steps(snippet)
        self.assertEqual(code, 0, output)
        return self.said(output)

    def test_a_name_comes_from_the_manifest(self):
        self.assertEqual(self.name_of(294690), ["Pajama Sam"])

    def test_a_name_is_never_read_through_a_link(self):
        # Root runs this, and every Guardian can write the shared folder.
        elsewhere = self.state / "elsewhere.acf"
        elsewhere.write_text(MANIFEST % (12, "Someone Else's Game"))
        (self.steamapps / "appmanifest_12.acf").symlink_to(elsewhere)
        self.assertEqual(self.name_of(12), ["game 12"])
        (self.steamapps / "appmanifest_13.acf").symlink_to("/dev/zero")
        self.assertEqual(self.name_of(13), ["game 13"])

    def test_a_name_is_never_waited_for(self):
        os.mkfifo(self.steamapps / "appmanifest_12.acf")
        self.assertEqual(self.name_of(12), ["game 12"])

    def test_a_manifest_that_is_not_steams_gives_the_number_and_no_traceback(self):
        texts = ['"AppState" "a string"\n', '"AppState" { "name" { "a" "b" } }\n', '"' + '\\"' * 1000,
                 "x" * (2 << 20), "\udcff"]
        for text in texts:
            with self.subTest(text=text[:20]):
                (self.steamapps / "appmanifest_12.acf").write_text(text, errors="surrogateescape")
                self.assertEqual(self.name_of(12), ["game 12"])

    def test_every_reason_has_its_sentence(self):
        reasons = {
            "unreadable": "cannot be read",
            "appid": "names another game",
            "installdir": "names its folder in a way Cairn does not copy",
            "beta-branch": "on a beta branch, which ada's Steam account may not have",
            "not-installed": "not fully installed, or an update is waiting",
            "downloading": "still downloading it",
            "no-folder": "its folder is missing from the shared folder",
            "folder-without-manifest": "may hold ada's saves; docs/TRY.md says how to set it aside",
            "changed": "Steam changed it during the copy",
            "copy-failed": "the copy did not work",
            "unreadable-files": "can be read only by your own account, so they cannot be copied; docs/TRY.md",
            "something-new": "Cairn does not copy it",
        }
        for reason, words in reasons.items():
            with self.subTest(reason=reason):
                self.says("ada", f"skipped 294690 {reason}", "finished")
                [line] = self.said(self.run_step())[:-1]
                self.assertTrue(line.startswith("note: Pajama Sam is not copied for ada; "), line)
                self.assertIn(words, line)

    def test_every_condition_has_its_note(self):
        conditions = {
            "no-library": "ada's Steam has not run yet; their games are copied at the next run after a visit",
            "no-shared": "there is no shared game folder",
            "lists-shared": "remove it in Steam's Settings > Storage during a visit",
            "no-reflink": "full copies would fill it",
            "unreadable-config": "their Steam's list of game folders is not a plain file",
            "unnumbered": "has no game number",
            "refused logged-in": "ada is logged in; their games are copied at the next run with ada logged out",
            "refused busy": "a child's Steam is open from a grown-up's login",
            "refused logging-out": "a grown-up is logging a child out",
            "refused still-running": "a program of ada's is still running",
            "refused not-set-up": "is not labelled yet",
            "refused stopped": "the helper refused: stopped",
            "failed": "stopped early; it is tried again at the next run, and journalctl -t cairn-steam-visit",
        }
        for line, words in conditions.items():
            with self.subTest(line=line):
                ends = line == "failed" or line.startswith("refused ")
                self.says("ada", line, *([] if ends else ["finished"]))
                [said] = self.said(self.run_step())[:-1]
                self.assertTrue(said.startswith("note: "), said)
                self.assertIn(words, said)

    def test_a_line_outside_the_grammar_is_never_printed(self):
        odd = ["/home/ada/.local/share/Steam", "copied 12 /home/ada", "refused ../x",
               "skipped 7 Beta", "\x1b[2J", "copied 12345678901"]
        self.says("ada", *odd, "finished")
        output = self.run_step()
        unknown = "note: copying ada's games printed a line this script does not know; it is left out here"
        self.assertEqual(self.said(output), [unknown] * len(odd) + ["changes: 0"])
        for line in odd:
            self.assertNotIn(line, output, "not in the trace either")

    def test_one_child_failing_stops_neither_the_others_nor_the_rest(self):
        self.says("ada", "refused logged-in", status=1)
        self.says("ben", "copied 294690", "finished")
        output = self.run_step(children=("ada", "ben"))
        self.assertEqual(self.said(output), [
            "note: ada is logged in; their games are copied at the next run with ada logged out",
            "changed: ben's own copy of Pajama Sam",
            "changes: 1",
        ])
        self.assertIn("+ true after", output, "the trace comes back")

    def test_a_helper_that_ends_without_a_word_gets_a_note(self):
        # It died before it could say finished, failed or why it refused.
        for lines in ([], ["copied 294690"], ["refused ../x"]):
            with self.subTest(lines=lines):
                self.says("ada", *lines, status=1)
                said = self.said(self.run_step())
                self.assertIn("note: copying ada's games stopped without saying why; it is tried again at the "
                              "next run, and journalctl -t cairn-steam-visit may have a record", said)

    def test_a_closing_line_needs_no_further_note(self):
        for lines in (["finished"], ["failed"], ["refused busy"]):
            with self.subTest(lines=lines):
                self.says("ada", *lines)
                self.assertNotIn("without saying why", self.run_step())

    def test_with_no_shared_games_nothing_runs_and_nothing_is_said(self):
        # No child is held from logging in, and no note says something waits.
        (self.steamapps / "appmanifest_294690.acf").unlink()
        (self.steamapps / "common").mkdir()
        self.says("ada", "no-library", "finished")
        output = self.run_step(children=("ada", "ben"))
        self.assertEqual(self.said(output), ["changes: 0"])
        self.assertFalse((self.state / "ada-ran").exists())
        self.assertFalse((self.state / "ben-ran").exists())

    def test_runs_after_what_it_needs(self):
        steps = main_steps()
        for needed in ("ensure_children", "install_steam_visit", "make_shared_library",
                       "install_copy_games", "relabel", "make_game_tiles"):
            self.assertLess(steps.index(needed), steps.index("copy_games"), needed)

    def test_installs_the_copy_program_beside_its_parser(self):
        text = SCRIPT.read_text()
        self.assertIn('install_file 644 "$REPO/provision/cairn-copy-games.py" '
                      '"$PREFIX/libexec/cairn/cairn-copy-games"', text)
        self.assertIn('install_file 644 "$REPO/tools/kidscan/kidscan.py" '
                      '"$PREFIX/libexec/cairn/kidscan.py"', text)


def main_steps():
    """The steps main runs, in order."""
    main = SCRIPT.read_text().split("\nmain() {", 1)[1]
    return [line.strip() for line in main.split("\n}", 1)[0].splitlines()]


# Starts as a JPEG does, and is nothing else: no game's art is ever part of
# the tests.
STAND_IN_PICTURE = b"\xff\xd8\xff\xe0 a stand-in, not a picture\n"


class GameTilesTest(unittest.TestCase):
    """The children's game tiles from the shared folder, with a picture of
    each game from the Guardian's Steam (ADR-0033, ADR-0035), made by the
    real kidscan and picture step installed into a temporary prefix."""

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.state = pathlib.Path(self.directory.name)
        self.libexec = self.state / "prefix" / "libexec" / "cairn"
        self.libexec.mkdir(parents=True)
        repo = SCRIPT.parents[1]
        shutil.copy(repo / "tools" / "kidscan" / "kidscan.py", self.libexec / "kidscan.py")
        shutil.copy(repo / "provision" / "cairn-game-pictures.py", self.libexec / "cairn-game-pictures")
        self.library = self.state / "steam-library"
        self.steamapps = self.library / "steamapps"
        (self.steamapps / "common" / "A Game").mkdir(parents=True)
        self.home = self.state / "home" / "gina"
        self.cache = self.home / ".local" / "share" / "Steam" / "appcache" / "librarycache"
        self.cache.mkdir(parents=True)
        self.pictures = self.state / "game-pictures"
        self.games = self.state / "games.json"
        # The children's lists, which only the image has; not made here
        # unless a test lists a game.
        self.lists = self.state / "games"
        self.game(294690, "Pajama Sam")

    def tearDown(self):
        self.directory.cleanup()

    def game(self, appid, name, picture=STAND_IN_PICTURE):
        (self.steamapps / f"appmanifest_{appid}.acf").write_text(MANIFEST % (appid, name))
        if picture is not None:
            (self.cache / str(appid)).mkdir()
            (self.cache / str(appid) / "header.jpg").write_bytes(picture)

    def variables(self):
        return (
            f"PREFIX={shlex.quote(str(self.state / 'prefix'))}\n"
            f"SHARED_LIBRARY={shlex.quote(str(self.library))}\n"
            f"GAMES_FILE={shlex.quote(str(self.games))}\n"
            f"GAME_PICTURES={shlex.quote(str(self.pictures))}\n"
            f"GAME_LISTS={shlex.quote(str(self.lists))}\n"
            f"guardian_home() {{ echo {shlex.quote(str(self.home))}; }}\n"
        )

    def run_step(self, code=0):
        snippet = self.variables() + 'make_game_tiles\ntrue after\necho "changes: $changes"'
        # The tests do not run as root: stat answers from the scenario, and
        # chown and chmod write what they were asked to a log.
        stubs = {"stat": '#!/bin/bash\ncat "$STATE/stat-answer" 2> /dev/null || echo root:root:755\n'}
        for name in ("chown", "chmod"):
            stubs[name] = f'#!/bin/bash\necho "{name} $*" >> "$STATE/calls"\n'
        returned, output, _ = run_steps(snippet, env_extra={"STATE": str(self.state)}, extra_stubs=stubs)
        self.assertEqual(returned, code, output)
        return output

    def calls(self):
        calls = self.state / "calls"
        return calls.read_text().splitlines() if calls.exists() else []

    def said(self, output):
        """What the step printed, without the trace."""
        return [line for line in output.splitlines() if not line.startswith("+")]

    def entries(self):
        return json.loads(self.games.read_text())["entries"]

    def test_puts_the_pictures_folder_right(self):
        self.pictures.mkdir()
        (self.state / "stat-answer").write_text("gina:gina:777\n")
        output = self.run_step()
        self.assertIn(f"changed: {self.pictures} owned by root, mode 755", self.said(output))
        self.assertEqual(self.calls(), [f"chown root:root {self.pictures}", f"chmod 755 {self.pictures}"])

    def test_a_right_pictures_folder_is_left_alone(self):
        self.pictures.mkdir()
        output = self.run_step()
        self.assertNotIn(f"changed: {self.pictures} owned by root, mode 755", self.said(output))
        self.assertEqual(self.calls(), [])

    def test_makes_the_pictures_and_the_games_file(self):
        output = self.run_step()
        self.assertEqual(self.said(output), [
            f"changed: {self.pictures}",
            "changed: the picture for Pajama Sam",
            f"changed: {self.games}",
            "changes: 3",
        ])
        picture = self.pictures / "294690.jpg"
        self.assertEqual(picture.read_bytes(), STAND_IN_PICTURE)
        self.assertEqual(oct(self.pictures.stat().st_mode & 0o777), oct(0o755))
        self.assertEqual(oct(self.games.stat().st_mode & 0o777), oct(0o644))
        [entry] = self.entries()
        self.assertEqual(entry["id"], "steam-294690")
        self.assertEqual(entry["exec"], ["steam", "-applaunch", "294690"])
        # The tile finds the picture by the game's app id (ADR-0047).
        self.assertNotIn("picture", entry)
        self.assertIn("+ true after", output, "the trace comes back")

    def test_a_second_run_changes_nothing(self):
        self.run_step()
        self.assertEqual(self.said(self.run_step()), ["changes: 0"])

    def test_no_shared_folder_leaves_the_games_file_alone(self):
        shutil.rmtree(self.library)
        self.games.write_text("the last run's\n")
        self.assertEqual(self.said(self.run_step()), ["changes: 0"])
        self.assertEqual(self.games.read_text(), "the last run's\n")
        self.assertFalse(self.pictures.exists())

    def test_a_missing_picture_gives_the_note_and_the_tile_its_name(self):
        self.game(400, "Freddi Fish", picture=None)
        said = self.said(self.run_step())
        self.assertIn("note: Freddi Fish has no picture yet, so its tile shows its name. Your own Steam keeps a "
                      "game's picture once its library has shown the game: open Steam in your own login, then run "
                      "this again.", said)
        self.assertEqual(sorted(path.name for path in self.pictures.iterdir()), ["294690.jpg"])

    def test_a_refused_picture_gives_the_note(self):
        header = self.cache / "294690" / "header.jpg"
        header.unlink()
        header.symlink_to(self.state / "elsewhere.jpg")
        said = self.said(self.run_step())
        self.assertIn("note: the picture your Steam keeps for Pajama Sam was left out, because it is not a plain "
                      "file, so its tile shows its name.", said)
        self.assertEqual(list(self.pictures.iterdir()), [])

    def test_a_refused_picture_with_one_kept_says_the_tile_keeps_it(self):
        self.run_step()
        (self.cache / "294690" / "header.jpg").write_bytes(b"not a JPEG")
        said = self.said(self.run_step())
        self.assertIn("note: the picture your Steam keeps for Pajama Sam was left out, because it is not a JPEG "
                      "picture, so its tile keeps the picture from an earlier run.", said)
        self.assertEqual((self.pictures / "294690.jpg").read_bytes(), STAND_IN_PICTURE)

    def test_every_refusal_has_its_sentence(self):
        reasons = {
            "not-a-file": "is not a plain file",
            "too-big": "is larger than 1 MB",
            "not-a-jpeg": "is not a JPEG picture",
            "unreadable": "could not be read",
        }
        self.pictures.mkdir()
        for kept, ending in ((False, "shows its name"), (True, "keeps the picture from an earlier run")):
            if kept:
                (self.pictures / "294690.jpg").write_bytes(STAND_IN_PICTURE)
            for reason, words in reasons.items():
                with self.subTest(reason=reason, kept=kept):
                    _, output, _ = run_steps(self.variables() + f"picture_sentence 'refused 294690 {reason}'")
                    self.assertEqual(self.said(output), [
                        f"note: the picture your Steam keeps for Pajama Sam was left out, because it {words}, "
                        f"so its tile {ending}."
                    ])

    def test_only_a_plain_file_counts_as_a_kept_picture(self):
        # The launcher shows no other kind of file as a tile's picture.
        self.pictures.mkdir()
        elsewhere = self.state / "elsewhere.jpg"
        elsewhere.write_bytes(STAND_IN_PICTURE)
        (self.pictures / "294690.jpg").symlink_to(elsewhere)
        (self.pictures / "400.jpg").mkdir()
        self.game(400, "Freddi Fish", picture=None)
        lines = ("refused 294690 too-big", "refused 400 too-big")
        _, output, _ = run_steps(self.variables() + "\n".join(f"picture_sentence '{line}'" for line in lines))
        self.assertEqual(self.said(output), [
            "note: the picture your Steam keeps for Pajama Sam was left out, because it is larger than 1 MB, "
            "so its tile shows its name.",
            "note: the picture your Steam keeps for Freddi Fish was left out, because it is larger than 1 MB, "
            "so its tile shows its name.",
        ])

    def test_a_line_outside_the_grammar_is_never_printed(self):
        odd = ["/home/gina/.local/share/Steam", "copied 12 /home/gina", "refused 12 Beta",
               "refused 12 not-a-file extra", "\x1b[2J", "copied 12345678901", "missing", "refused folder",
               "refused lists"]
        snippet = self.variables() + "\n".join(f"picture_sentence {shlex.quote(line)}" for line in odd)
        snippet += '\necho "changes: $changes"'
        _, output, _ = run_steps(snippet)
        self.assertEqual(self.said(output), ["changes: 0"])

    def test_a_removed_games_picture_is_removed(self):
        self.run_step()
        (self.steamapps / "appmanifest_294690.acf").unlink()
        said = self.said(self.run_step())
        self.assertIn("changed: the picture for game 294690, which has no tile now, removed", said)
        self.assertIn(f"changed: {self.games}", said)
        self.assertEqual(list(self.pictures.iterdir()), [])
        self.assertEqual(self.entries(), [])

    def test_a_picture_removed_while_its_game_stays_names_the_game(self):
        # An update that is still downloading: the game is in the shared
        # folder, and kidscan gives it no tile until Steam finishes.
        self.run_step()
        manifest = self.steamapps / "appmanifest_294690.acf"
        manifest.write_text(manifest.read_text().replace('"StateFlags"\t\t"4"', '"StateFlags"\t\t"1026"'))
        said = self.said(self.run_step())
        self.assertIn("changed: the picture for Pajama Sam, which has no tile now, removed", said)
        self.assertEqual(list(self.pictures.iterdir()), [])
        self.assertEqual(self.entries(), [])

    def list_for(self, login, appids):
        """A child's own list, as the image's Save writes it (ADR-0041)."""
        self.lists.mkdir(exist_ok=True)
        self.lists.chmod(0o755)
        entries = [{"id": f"steam-{appid}", "title": f"Game {appid}", "category": "games",
                    "engine": "steam", "exec": ["steam", "-applaunch", str(appid)]} for appid in appids]
        (self.lists / f"{login}.json").write_text(json.dumps({"version": 1, "entries": entries}))

    def test_a_game_still_on_a_childs_list_keeps_its_picture(self):
        # The child keeps its tile when the game leaves the shared folder or
        # while Steam updates it, so the picture stays (ADR-0047).
        update = '"StateFlags"\t\t"4"', '"StateFlags"\t\t"1026"'
        manifest = self.steamapps / "appmanifest_294690.acf"
        for case in ("gone", "updating"):
            with self.subTest(case=case):
                self.game(294690, "Pajama Sam", picture=None)
                self.run_step()
                self.list_for("ada", [294690])
                if case == "gone":
                    manifest.unlink()
                else:
                    manifest.write_text(manifest.read_text().replace(*update))
                said = self.said(self.run_step())
                self.assertFalse([line for line in said if "which has no tile now" in line], said)
                self.assertEqual((self.pictures / "294690.jpg").read_bytes(), STAND_IN_PICTURE)
                self.assertEqual(self.entries(), [])

    def test_a_steamapps_that_is_a_link_leaves_everything_alone(self):
        moved = self.state / "someone-elses-steamapps"
        self.steamapps.rename(moved)
        self.steamapps.symlink_to(moved)
        self.games.write_text("the last run's\n")
        said = self.said(self.run_step())
        self.assertEqual(said, [
            f"note: {self.steamapps} is a link, which Cairn does not follow, so the game tiles were not made "
            "again; the children keep the ones from the last run.",
            "changes: 0",
        ])
        self.assertEqual(self.games.read_text(), "the last run's\n")
        self.assertFalse(self.pictures.exists())

    def test_kidscan_failing_keeps_the_old_file(self):
        # It fails as a program, and still loads for the picture step.
        installed = self.libexec / "kidscan.py"
        text = installed.read_text()
        self.assertIn("sys.exit(main())", text)
        installed.write_text(text.replace("sys.exit(main())", "sys.exit(1)"))
        self.games.write_text("the last run's\n")
        said = self.said(self.run_step())
        self.assertIn("note: the game tiles could not be made again; the children keep the ones from the last run.",
                      said)
        self.assertEqual(self.games.read_text(), "the last run's\n")

    def test_a_picture_step_that_does_not_finish_gets_a_note(self):
        program = self.libexec / "cairn-game-pictures"
        for body in ('print("copied 294690")', 'print("refused folder")', 'print("refused lists")',
                     'print("failed")', "raise SystemExit(1)"):
            with self.subTest(body=body):
                program.write_text(body + "\n")
                said = self.said(self.run_step())
                self.assertIn("note: the game pictures were not all made; a tile without one shows the game's name.",
                              said)
                # The tiles are still made, each with its name.
                self.assertEqual([entry["title"] for entry in self.entries()], ["Pajama Sam"])

    def test_game_names_stay_out_of_the_trace(self):
        output = self.run_step()
        self.assertIn("changed: the picture for Pajama Sam", output)
        for line in output.splitlines():
            if line.startswith("+"):
                self.assertNotIn("Pajama Sam", line)

    def test_a_link_for_the_pictures_folder_is_refused(self):
        elsewhere = self.state / "elsewhere"
        elsewhere.mkdir()
        self.pictures.symlink_to(elsewhere)
        output = self.run_step(code=1)
        self.assertIn(f"refused: {self.pictures} is a link; move it aside and run this again", output)
        self.assertEqual(list(elsewhere.iterdir()), [])
        self.assertFalse(self.games.exists())

    def test_a_file_for_the_pictures_folder_is_refused(self):
        self.pictures.write_text("")
        output = self.run_step(code=1)
        self.assertIn(f"refused: {self.pictures} is not a folder; move it aside and run this again", output)
        self.assertFalse(self.games.exists())

    def test_the_home_is_the_guardians_from_the_account_database(self):
        stubs = {"getent": getent_stub(gecos={"gina": "Gina"})}
        _, output, _ = run_steps("guardian=gina\nguardian_home", extra_stubs=stubs)
        self.assertEqual(self.said(output), ["/home/gina"])
        code, output, _ = run_steps('guardian=nobody\necho "[$(guardian_home)]"', extra_stubs=stubs)
        self.assertEqual(code, 0, output)
        self.assertEqual(self.said(output), ["[]"])

    def test_runs_before_the_copies_and_after_what_it_needs(self):
        steps = main_steps()
        for needed in ("make_shared_library", "install_copy_games", "relabel"):
            self.assertLess(steps.index(needed), steps.index("make_game_tiles"), needed)
        self.assertLess(steps.index("make_game_tiles"), steps.index("copy_games"))

    def test_installs_the_picture_program_beside_kidscan(self):
        self.assertIn('install_file 644 "$REPO/provision/cairn-game-pictures.py" '
                      '"$PREFIX/libexec/cairn/cairn-game-pictures"', SCRIPT.read_text())


GROUP_NUMBERS = """#Type Name           ID
g     cairn-l1       461
g     cairn-l2       462
g     cairn-l3       463
g     cairn-l4       464
g     cairn-guardian 465
"""

# A `getent` for group lines, by name or number, from $STATE/group, written
# as /etc/group is.
GROUP_GETENT = r"""#!/bin/bash
[ "$1" = group ] || exit 2
awk -F: -v key="$2" '$1 == key || $3 == key { print; found = 1 } END { exit !found }' \
    "$STATE/group" 2> /dev/null || exit 2
"""


class GroupNumbersTest(unittest.TestCase):
    """A new group takes the number the image gives it (ADR-0038), so a
    computer set up by hand can move to the image without renumbering."""

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.state = pathlib.Path(self.directory.name)
        (self.state / "numbers").write_text(GROUP_NUMBERS)
        self.stubs = {
            "getent": GROUP_GETENT,
            "groupadd": '#!/bin/bash\necho "groupadd $*" >> "$STATE/calls"\n',
        }

    def tearDown(self):
        self.directory.cleanup()

    def run_step(self, snippet, groups=""):
        (self.state / "group").write_text(groups)
        numbers = shlex.quote(str(self.state / "numbers"))
        code, output, _ = run_steps(
            f"GROUP_NUMBERS={numbers}\n{snippet}",
            env_extra={"STATE": str(self.state)}, extra_stubs=self.stubs,
        )
        calls = self.state / "calls"
        return code, output, calls.read_text().splitlines() if calls.exists() else []

    def test_reads_the_images_number(self):
        code, output, _ = self.run_step("fixed_gid cairn-guardian")
        self.assertEqual(code, 0, output)
        self.assertIn("465", output.splitlines())

    def test_a_list_without_the_number_is_refused(self):
        (self.state / "numbers").write_text("g cairn-guardian -\n")
        code, output, _ = run_steps(
            f"GROUP_NUMBERS={shlex.quote(str(self.state / 'numbers'))}\nfixed_gid cairn-guardian"
        )
        self.assertEqual(code, 1, output)
        self.assertIn("refused:", output)

    def test_new_groups_take_their_fixed_numbers(self):
        code, output, calls = self.run_step("ensure_groups")
        self.assertEqual(code, 0, output)
        self.assertEqual(calls, [
            "groupadd --system --gid 461 cairn-l1", "groupadd --system --gid 462 cairn-l2",
            "groupadd --system --gid 463 cairn-l3", "groupadd --system --gid 464 cairn-l4",
            "groupadd --system --gid 465 cairn-guardian",
        ])

    def test_a_taken_number_is_left_to_its_group(self):
        groups = "\n".join([
            "cairn-l1:x:941:", "cairn-l2:x:942:", "cairn-l3:x:943:", "cairn-l4:x:944:",
            "games:x:465:",
        ]) + "\n"
        code, output, calls = self.run_step("ensure_groups", groups)
        self.assertEqual(code, 0, output)
        self.assertEqual(calls, ["groupadd --system cairn-guardian"])
        self.assertIn("note: number 465 is taken here", output)

    def test_groups_already_there_keep_their_numbers(self):
        groups = "\n".join(f"{g}:x:{n}:" for g, n in (
            ("cairn-l1", 941), ("cairn-l2", 942), ("cairn-l3", 943), ("cairn-l4", 944),
            ("cairn-guardian", 945))) + "\n"
        code, output, calls = self.run_step("ensure_groups", groups)
        self.assertEqual(code, 0, output)
        self.assertEqual(calls, [])

    @unittest.skipUnless(
        (SCRIPT.parents[1] / "system_files/usr/lib/sysusers.d/cairn.conf").exists(),
        "the build stage copies neither sysusers.d nor build_files",
    )
    def test_the_checkout_and_the_image_build_read_the_same_number(self):
        repo = SCRIPT.parents[1]
        code, output, _ = run_steps("fixed_gid cairn-guardian")
        self.assertEqual(code, 0, output)
        self.assertIn("465", output.splitlines())
        build = (repo / "build_files/build.sh").read_text()
        self.assertIn("""awk '$1 == "g" && $2 == "cairn-guardian" { print $3 }'""", build)


class ImageGuardTest(unittest.TestCase):
    """On a computer that runs the image, this script installs nothing; the
    family's setup does what is left (ADR-0038)."""

    def test_refuses_on_the_image_before_anything(self):
        with tempfile.TemporaryDirectory() as work:
            mark = pathlib.Path(work, "cairn.conf")
            mark.write_text(GROUP_NUMBERS)
            code, output, calls = run_steps(
                f"IMAGE_MARK={shlex.quote(str(mark))}\nmain --guardian gina --child ada"
            )
        self.assertEqual(code, 1, output)
        self.assertIn("refused: this computer runs the Cairn image; use provision/cairn-family-setup.sh", output)
        self.assertEqual(calls, "")

    def test_a_computer_set_up_by_hand_goes_on(self):
        code, output, _ = run_steps("main --guardian gina --child ada")
        # The tests do not run as root, so the next check stops it.
        self.assertEqual(code, 1, output)
        self.assertNotIn("Cairn image", output)
        self.assertIn("run as root", output)

    def test_the_mark_is_the_images_group_list(self):
        self.assertIn("\nIMAGE_MARK=/usr/lib/sysusers.d/cairn.conf\n", SCRIPT.read_text())

    def test_the_entries_follow_the_window(self):
        steps = main_steps()
        self.assertLess(steps.index("install_steam_for"), steps.index("make_steam_entries"))


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
