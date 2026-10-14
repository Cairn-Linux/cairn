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


def run_steps(snippet, accounts=None, env_extra=None, extra_stubs=None):
    """Sources the script, sets names, runs `snippet`. Returns (code, output, log)."""
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
[ "$1" = --copy-games ] || exit 2
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
            "something-new": "Cairn does not copy it",
        }
        for reason, words in reasons.items():
            with self.subTest(reason=reason):
                self.says("ada", f"skipped 294690 {reason}")
                [line] = self.said(self.run_step())[:-1]
                self.assertTrue(line.startswith("note: Pajama Sam is not copied for ada; "), line)
                self.assertIn(words, line)

    def test_every_condition_has_its_note(self):
        conditions = {
            "no-library": "ada's Steam has not run yet; their games are copied at the next run after a visit",
            "no-shared": "there is no shared game folder",
            "lists-shared": "remove it in Steam's Settings > Storage during a visit",
            "no-reflink": "full copies would fill it",
            "unnumbered": "has no game number",
            "refused logged-in": "ada is logged in; their games are copied at the next run with ada logged out",
            "refused busy": "a child's Steam is open from a grown-up's login",
            "refused still-running": "a program of ada's is still running",
            "refused not-set-up": "is not labelled yet",
            "refused stopped": "the helper refused: stopped",
            "failed": "stopped early",
        }
        for line, words in conditions.items():
            with self.subTest(line=line):
                self.says("ada", line)
                [said] = self.said(self.run_step())[:-1]
                self.assertTrue(said.startswith("note: "), said)
                self.assertIn(words, said)

    def test_a_line_outside_the_grammar_is_never_printed(self):
        odd = ["/home/ada/.local/share/Steam", "copied 12 /home/ada", "refused ../x",
               "skipped 7 Beta", "\x1b[2J", "copied 12345678901"]
        self.says("ada", *odd)
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

    def test_runs_after_what_it_needs(self):
        main = SCRIPT.read_text().split("\nmain() {", 1)[1]
        steps = [line.strip() for line in main.split("\n}", 1)[0].splitlines()]
        for needed in ("ensure_children", "install_steam_visit", "make_shared_library",
                       "install_copy_games", "relabel"):
            self.assertLess(steps.index(needed), steps.index("copy_games"), needed)

    def test_installs_the_copy_program_beside_its_parser(self):
        text = SCRIPT.read_text()
        self.assertIn('install_file 644 "$REPO/provision/cairn-copy-games.py" '
                      '"$PREFIX/libexec/cairn/cairn-copy-games"', text)
        self.assertIn('install_file 644 "$REPO/tools/kidscan/kidscan.py" '
                      '"$PREFIX/libexec/cairn/kidscan.py"', text)


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
