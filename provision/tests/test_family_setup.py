#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""The family's setup on a computer that runs the image, and the renumbering
of cairn-guardian before a computer set up by hand moves to it (ADR-0038,
slice 4 of #164).

Standard library only. Both scripts load cairn-provision.sh's steps, and
run their own only from main when executed, so sourcing one here defines
them without touching the machine. The system's commands are stand-ins
that answer from files in a temporary folder and write what they were
asked to a log.
"""

import pathlib
import shlex
import tempfile
import unittest

from test_provision import GROUP_GETENT, GROUP_NUMBERS, SECRET, getent_stub, id_stub, run_steps

HERE = pathlib.Path(__file__).resolve().parent.parent
FAMILY = HERE / "cairn-family-setup.sh"
RENUMBER = HERE / "cairn-renumber-guardians.sh"

# Every step of cairn-provision.sh that installs a program or a setting. The
# image has them all, so none may run on it.
MACHINE_STEPS = {
    "check_build", "record_facts", "ensure_groups", "layer_packages", "install_session_files",
    "install_launcher", "install_display_manager", "install_greeter", "protect_guardians",
    "install_steam_visit", "install_steam_for", "install_copy_games", "relabel",
}


def main_steps(script):
    """The steps a script's main runs, in order."""
    main = script.read_text().split("\nmain() {", 1)[1]
    lines = [line.strip() for line in main.split("\n}", 1)[0].splitlines()]
    return [line for line in lines if line and not line.startswith("#")]


def logger(name):
    return f'#!/bin/bash\necho "{name} $*" >> "$STATE/calls"\n'


class Scenario(unittest.TestCase):
    """A temporary folder for the stand-ins' answers and their log."""

    script = FAMILY

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.state = pathlib.Path(self.directory.name)
        self.mark = self.state / "cairn.conf"
        self.mark.write_text(GROUP_NUMBERS)
        self.groups("")
        self.stubs = {"getent": GROUP_GETENT}

    def tearDown(self):
        self.directory.cleanup()

    def groups(self, text):
        (self.state / "group").write_text(text)

    def run_script(self, snippet, mark=True):
        set_mark = f"IMAGE_MARK={shlex.quote(str(self.mark))}\n" if mark else ""
        numbers = f"GROUP_NUMBERS={shlex.quote(str(self.mark))}\n"
        code, output, _ = run_steps(
            set_mark + numbers + snippet, env_extra={"STATE": str(self.state)},
            extra_stubs=self.stubs, script=self.script,
        )
        calls = self.state / "calls"
        return code, output, calls.read_text().splitlines() if calls.exists() else []


class FamilyStepsTest(unittest.TestCase):
    def test_runs_only_the_familys_own_steps(self):
        steps = set(main_steps(FAMILY))
        self.assertEqual(steps & MACHINE_STEPS, set())
        for step in ("ensure_guardian", "make_children", "make_steam_entries",
                     "make_shared_library", "make_game_tiles", "install_flatpaks"):
            self.assertIn(step, steps)

    def test_copies_no_games_to_any_child(self):
        # Each child gets the games on their own list, from the Guardian
        # tool's Save (ADR-0041); the kids' laptop keeps copy_games.
        self.assertNotIn("copy_games", main_steps(FAMILY))
        self.assertIn("copy_games", main_steps(HERE / "cairn-provision.sh"))
        self.assertNotIn("--all", FAMILY.read_text())

    def test_says_where_the_games_are_chosen(self):
        text = FAMILY.read_text()
        self.assertIn("open Family from the menu, then its Games page, and press Save", text)

    def test_children_come_from_the_images_command(self):
        # cairn-family makes the children and their entries (ADR-0039), not
        # provisioning's own steps.
        steps = main_steps(FAMILY)
        self.assertNotIn("ensure_children", steps)
        self.assertLess(steps.index("ensure_image_groups"), steps.index("make_children"))

    def test_every_childs_entry_is_refreshed_after_the_children(self):
        # A child left off this run keeps an up-to-date entry too.
        steps = main_steps(FAMILY)
        self.assertLess(steps.index("make_children"), steps.index("make_steam_entries"))
        self.assertLess(steps.index("make_steam_entries"), steps.index("relabel_entries"))

    def test_checks_come_before_any_change(self):
        steps = main_steps(FAMILY)
        first_change = steps.index("ensure_image_groups")
        for check in ("parse_arguments \"$@\"", "check_image", "check_accounts",
                      "check_children_are_cairns", "check_checkout", "check_guardian_number"):
            self.assertLess(steps.index(check), first_change, check)

    def test_the_full_list_comes_after_the_folder_and_the_flatpaks_last(self):
        steps = main_steps(FAMILY)
        self.assertLess(steps.index("make_shared_library"), steps.index("make_game_tiles"))
        changes = [s for s in steps if not s.startswith(("set ", "echo ", "parse_", "check_", "["))]
        self.assertEqual(changes[-1], "install_flatpaks")

    def test_names_the_images_paths(self):
        text = FAMILY.read_text()
        self.assertIn("\nPREFIX=/usr\n", text)
        self.assertIn("\nSHARE=/usr/share/cairn\n", text)
        self.assertIn('\nGROUP_NUMBERS="$IMAGE_MARK"\n', text)
        self.assertIn('\nFAMILY_COMMAND="$PREFIX/libexec/cairn/cairn-family"\n', text)


class FamilyTilesNoteTest(unittest.TestCase):
    """make_game_tiles is shared with the kids' laptop, but on the image
    each child's tiles come from their own list (ADR-0041), so its notes
    name what the family's setup really keeps."""

    def test_a_link_names_the_games_page(self):
        with tempfile.TemporaryDirectory() as folder:
            library = pathlib.Path(folder)
            (library / "steamapps").symlink_to(folder)
            code, output, _ = run_steps(
                f"SHARED_LIBRARY={shlex.quote(folder)}\nmake_game_tiles", script=FAMILY)
        self.assertEqual(code, 0, output)
        self.assertIn(
            f"note: {library}/steamapps is a link, which Cairn does not follow, so the list of shared "
            "games was not made again; the Games page in Family shows the one from the last run, and "
            "each child keeps the games on their own list.", output)
        said = "\n".join(line for line in output.splitlines() if not line.startswith("+"))
        self.assertNotIn("the children keep the ones from the last run", said)

    def test_a_failed_list_names_the_games_page(self):
        code, output, _ = run_steps('echo "said: $TILES_NOT_MADE"', script=FAMILY)
        self.assertEqual(code, 0, output)
        self.assertIn(
            "said: the list of shared games could not be made again; the Games page in Family shows "
            "the one from the last run, and each child keeps the games on their own list.", output)

    def test_the_picture_step_reads_each_childs_own_list(self):
        # So a game a child still has keeps its picture (ADR-0047).
        code, output, _ = run_steps('echo "lists: $GAME_LISTS"', script=FAMILY)
        self.assertEqual(code, 0, output)
        said = [line for line in output.splitlines() if not line.startswith("+")]
        self.assertIn("lists: /var/lib/cairn/games", said)


class FamilyGuardTest(Scenario):
    def test_refuses_a_computer_set_up_by_hand(self):
        code, output, calls = self.run_script("main --guardian gina --child ada", mark=False)
        self.assertEqual(code, 1, output)
        self.assertIn("refused: this computer does not run the Cairn image", output)
        self.assertEqual(calls, [])

    def test_the_image_goes_on(self):
        code, output, _ = self.run_script("check_image\necho after")
        self.assertEqual(code, 0, output)
        self.assertIn("after", output.splitlines())


class CheckoutTest(Scenario):
    """The image's helpers must be the checkout's own, with /usr/local
    changed to /usr as the build changes it."""

    FILES = {
        "provision/cairn-copy-games.py": "cairn-copy-games",
        "provision/cairn-game-pictures.py": "cairn-game-pictures",
        "provision/cairn-share-tools.py": "cairn-share-tools",
        "tools/kidscan/kidscan.py": "kidscan.py",
        "session/bin/cairn-steam-visit": "cairn-steam-visit",
    }

    def setUp(self):
        super().setUp()
        self.repo = self.state / "repo"
        self.libexec = self.state / "prefix" / "libexec" / "cairn"
        self.libexec.mkdir(parents=True)
        for source, installed in self.FILES.items():
            path = self.repo / source
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(f"{installed}\nCAGE=/usr/local/libexec/cairn/cage-visit\n")
            (self.libexec / installed).write_text(f"{installed}\nCAGE=/usr/libexec/cairn/cage-visit\n")
        self.command = self.libexec / "cairn-family"
        self.command.write_text("#!/bin/bash\necho done\n")
        self.command.chmod(0o755)

    def check(self):
        return self.run_script(
            f"REPO={shlex.quote(str(self.repo))}\nPREFIX={shlex.quote(str(self.state / 'prefix'))}\n"
            f"FAMILY_COMMAND={shlex.quote(str(self.command))}\n"
            "check_checkout\necho after"
        )

    def test_the_same_files_pass(self):
        code, output, _ = self.check()
        self.assertEqual(code, 0, output)
        self.assertIn("after", output.splitlines())

    def test_any_difference_is_refused(self):
        for installed in self.FILES.values():
            with self.subTest(installed):
                path = self.libexec / installed
                kept = path.read_text()
                path.write_text("another commit\n")
                code, output, _ = self.check()
                path.write_text(kept)
                self.assertEqual(code, 1, output)
                self.assertIn("refused: this checkout does not match the image", output)
                self.assertIn("check out the commit the image was built from", output)

    def test_a_missing_helper_is_refused(self):
        (self.libexec / "kidscan.py").unlink()
        code, output, _ = self.check()
        self.assertEqual(code, 1, output)

    def test_an_image_without_the_family_command_is_refused(self):
        # It is compiled, so it cannot be compared with the checkout; an
        # image built before it is refused instead (ADR-0039).
        self.command.unlink()
        code, output, _ = self.check()
        self.assertEqual(code, 1, output)
        self.assertIn("refused: this image has no", output)
        self.assertIn("cairn-family", output)


class ImageGroupsTest(Scenario):
    def setUp(self):
        super().setUp()
        self.stubs["groupadd"] = logger("groupadd")

    def test_the_right_number_passes(self):
        self.groups("cairn-guardian:x:465:gina\n")
        code, output, _ = self.run_script("check_guardian_number\necho after")
        self.assertEqual(code, 0, output)
        self.assertIn("after", output.splitlines())

    def test_another_number_is_refused_and_names_the_renumbering(self):
        self.groups("cairn-guardian:x:945:gina\n")
        code, output, calls = self.run_script("check_guardian_number")
        self.assertEqual(code, 1, output)
        self.assertIn("cairn-guardian is number 945 here and the image needs 465", output)
        self.assertIn("provision/cairn-renumber-guardians.sh", output)
        self.assertEqual(calls, [])

    def test_level_groups_keep_their_numbers(self):
        self.groups("cairn-l1:x:941:ada\ncairn-guardian:x:465:gina\n")
        code, output, _ = self.run_script("check_guardian_number\necho after")
        self.assertEqual(code, 0, output)

    def test_a_missing_group_is_made_with_its_number(self):
        self.groups("cairn-l1:x:461:\ncairn-l2:x:462:\ncairn-l3:x:463:\ncairn-l4:x:464:\n")
        code, output, calls = self.run_script("check_guardian_number\nensure_image_groups")
        self.assertEqual(code, 0, output)
        self.assertEqual(calls, ["groupadd --system --gid 465 cairn-guardian"])
        self.assertIn("changed: group cairn-guardian", output)

    def test_groups_that_are_there_are_left_alone(self):
        self.groups("".join(f"{g}:x:{n}:\n" for g, n in (
            ("cairn-l1", 461), ("cairn-l2", 462), ("cairn-l3", 463), ("cairn-l4", 464),
            ("cairn-guardian", 465))))
        code, output, calls = self.run_script("ensure_image_groups")
        self.assertEqual(code, 0, output)
        self.assertEqual(calls, [])


# A stand-in for cairn-family: it writes its arguments to the log, says
# what $STATE/answer says, or done, and on done writes the child's entry
# into $STATE/applications, as the real command does.
FAMILY_STAND_IN = r"""#!/bin/bash
echo "cairn-family $*" >> "$STATE/calls"
answer="$(cat "$STATE/answer" 2> /dev/null || echo done)"
echo "$answer"
[ "$answer" = done ] || exit 1
mkdir -p "$STATE/applications"
echo "entry for $2" > "$STATE/applications/cairn-steam-for-$2.desktop"
"""


class ChildrenTest(Scenario):
    """Each child named is made, or brought to its level, by the image's
    cairn-family, and the run stops on anything but done."""

    def setUp(self):
        super().setUp()
        command = self.state / "cairn-family"
        command.write_text(FAMILY_STAND_IN)
        command.chmod(0o755)
        self.apps = self.state / "applications"
        self.setup = (f"FAMILY_COMMAND={shlex.quote(str(command))}\n"
                      f"APPS={shlex.quote(str(self.apps))}\n"
                      "children=(ada ben); levels=(1 2)\n")

    def test_each_child_is_made_at_its_level(self):
        code, output, calls = self.run_script(self.setup + "make_children\necho after")
        self.assertEqual(code, 0, output)
        self.assertEqual(calls, ["cairn-family create-child ada 1", "cairn-family create-child ben 2"])
        self.assertIn("changed: child ada at level 1", output)
        self.assertIn("changed: child ben at level 2", output)
        self.assertIn("after", output.splitlines())

    def test_a_second_run_counts_no_change(self):
        self.apps.mkdir()
        for child in ("ada", "ben"):
            (self.apps / f"cairn-steam-for-{child}.desktop").write_text(f"entry for {child}\n")
        code, output, calls = self.run_script(self.setup + "make_children")
        self.assertEqual(code, 0, output)
        self.assertEqual(len(calls), 2)
        self.assertNotIn("changed:", output)

    def test_a_refusal_stops_the_run_and_says_the_word(self):
        (self.state / "answer").write_text("refused taken\n")
        code, output, calls = self.run_script(self.setup + "make_children\necho after")
        self.assertEqual(code, 1, output)
        self.assertEqual(calls, ["cairn-family create-child ada 1"])
        self.assertIn("ada's account was not made or changed; cairn-family said: refused taken", output)
        self.assertIn("journalctl -t cairn-family", output)
        self.assertNotIn("after", output.splitlines())

    def test_a_word_this_checkout_does_not_know_is_not_done(self):
        # An image from another commit may answer otherwise; that stops too.
        (self.state / "answer").write_text("done-and-more\n")
        code, output, _ = self.run_script(self.setup + "make_children")
        self.assertEqual(code, 1, output)
        self.assertIn("cairn-family said: done-and-more", output)

    def test_silence_is_not_done(self):
        (self.state / "answer").write_text("")
        code, output, _ = self.run_script(self.setup + "make_children")
        self.assertEqual(code, 1, output)
        self.assertIn("cairn-family said: nothing", output)


class CairnChildrenTest(Scenario):
    """An existing account named as a child must be one Cairn made."""

    def check(self, accounts):
        self.stubs["id"] = id_stub(accounts)
        return self.run_script("children=(ada ben); levels=(1 2)\ncheck_children_are_cairns\necho after")

    def test_new_accounts_and_cairns_children_pass(self):
        code, output, _ = self.check({"ada": (1002, ["ada", "cairn-l1"])})
        self.assertEqual(code, 0, output)
        self.assertIn("after", output.splitlines())

    def test_a_child_at_another_level_passes(self):
        code, output, _ = self.check({"ben": (1003, ["ben", "cairn-l3"])})
        self.assertEqual(code, 0, output)

    def test_an_account_cairn_did_not_make_is_refused(self):
        code, output, calls = self.check({"ben": (1003, ["ben", "users"])})
        self.assertEqual(code, 1, output)
        self.assertIn("refused: ben is an account Cairn did not make", output)
        self.assertEqual(calls, [])


class EntriesTest(Scenario):
    """After cairn-family, every child has the image's entry, named on this
    run or not, and an account that is no longer a child loses its own."""

    def test_every_child_gets_the_images_entry(self):
        apps = self.state / "applications"
        apps.mkdir()
        # ada was named and cairn-family wrote her entry. ben was left off
        # this run and has none. cy was set up by hand, with /usr/local.
        (apps / "cairn-steam-for-ada.desktop").write_text("from cairn-family\n")
        (apps / "cairn-steam-for-cy.desktop").write_text(
            "Exec=/usr/local/bin/cairn-steam-for cy\n")
        (apps / "cairn-steam-for-gone.desktop").write_text("entry\n")
        (apps / "other.desktop").write_text("entry\n")
        self.stubs["getent"] = getent_stub(
            gecos={"ada": "Ada", "ben": "", "cy": "Cy"},
            groups={"cairn-l1": ["ada", "cy"], "cairn-l2": ["ben"], "cairn-l3": [], "cairn-l4": []})
        self.stubs["id"] = id_stub({"ada": (1002, ["ada", "cairn-l1"]),
                                    "ben": (1003, ["ben", "cairn-l2"]),
                                    "cy": (1004, ["cy", "cairn-l1"])})
        code, output, calls = self.run_script(
            f"APPS={shlex.quote(str(apps))}\nchildren=(ada); levels=(1)\nmake_steam_entries")
        self.assertEqual(code, 0, output)
        files = {path.name: path.read_text() for path in apps.iterdir()}
        self.assertEqual(sorted(files), [
            "cairn-steam-for-ada.desktop", "cairn-steam-for-ben.desktop",
            "cairn-steam-for-cy.desktop", "other.desktop"])
        self.assertIn("Name=Steam for ben\n", files["cairn-steam-for-ben.desktop"])
        self.assertIn("Exec=/usr/bin/cairn-steam-for ben\n", files["cairn-steam-for-ben.desktop"])
        self.assertIn("Exec=/usr/bin/cairn-steam-for cy\n", files["cairn-steam-for-cy.desktop"])
        self.assertNotIn("/usr/local/", files["cairn-steam-for-cy.desktop"])
        self.assertIn("Name=Steam for Ada\n", files["cairn-steam-for-ada.desktop"])
        self.assertIn("removed", output)
        self.assertEqual(calls, [])


class FlatpakTest(Scenario):
    def test_installed_apps_are_not_installed_again(self):
        self.stubs["flatpak"] = '#!/bin/bash\necho "flatpak $*" >> "$STATE/calls"\n'
        code, output, calls = self.run_script("install_flatpaks")
        self.assertEqual(code, 0, output)
        self.assertTrue(all(call.startswith("flatpak info") for call in calls), calls)

    def test_an_offline_install_stops_the_run(self):
        self.stubs["flatpak"] = '#!/bin/bash\n[ "$1" = install ] && exit 1\nexit 1\n'
        code, output, _ = self.run_script("install_flatpaks\necho after")
        self.assertNotEqual(code, 0, output)
        self.assertNotIn("after", output.splitlines())


def root_id_stub(accounts):
    """id_stub for a run as root: `id -u` with no account answers 0."""
    root = '[ "$*" != -u ] || { echo 0; exit 0; }\n'
    return id_stub(accounts).replace("#!/bin/bash\n", "#!/bin/bash\n" + root, 1)


class PasswordRunTest(Scenario):
    """The family's whole run, from its first check to the Flatpaks, with a
    recognisable fake password for the Guardian in CAIRN_GUARDIAN_PASSWORD
    (#103). Every command that would change the computer is a stand-in,
    and every folder the run writes is in the temporary folder."""

    def setUp(self):
        super().setUp()
        repo = self.state / "repo"
        prefix = self.state / "prefix"
        libexec = prefix / "libexec" / "cairn"
        libexec.mkdir(parents=True)
        # The image's helpers are the checkout's own, so check_checkout passes.
        for source, installed in CheckoutTest.FILES.items():
            (repo / source).parent.mkdir(parents=True, exist_ok=True)
            (repo / source).write_text(f"{installed}\n")
            (libexec / installed).write_text(f"{installed}\n")
        command = libexec / "cairn-family"
        command.write_text(FAMILY_STAND_IN)
        command.chmod(0o755)
        self.groups("".join(f"{group}:x:{number}:\n" for group, number in (
            ("cairn-l1", 461), ("cairn-l2", 462), ("cairn-l3", 463), ("cairn-l4", 464),
            ("cairn-guardian", 465))))
        for name in ("groupadd", "chown", "chmod", "getfacl", "setfacl", "restorecon"):
            self.stubs[name] = logger(name)
        # Neither app is there yet, and both install.
        self.stubs["flatpak"] = logger("flatpak") + '[ "$1" != info ]\n'
        # FAMILY_COMMAND was made from PREFIX when the script was loaded, so
        # it is set again with it.
        folders = {
            "REPO": repo, "PREFIX": prefix, "FAMILY_COMMAND": command,
            "APPS": self.state / "applications", "SHARED_LIBRARY": self.state / "steam-library",
            "GAME_PICTURES": self.state / "game-pictures", "GAMES_FILE": self.state / "games.json",
            "GAME_LISTS": self.state / "games",
        }
        self.folders = "".join(f"{name}={shlex.quote(str(path))}\n" for name, path in folders.items())

    def run_family(self, accounts=None):
        """main, as root. Returns (code, output, every line the stand-ins logged)."""
        self.stubs["id"] = root_id_stub(accounts or {})
        mark = shlex.quote(str(self.mark))
        code, output, calls = run_steps(
            f"IMAGE_MARK={mark}\nGROUP_NUMBERS={mark}\n{self.folders}main --guardian gina --child ada",
            env_extra={"STATE": str(self.state), "CAIRN_GUARDIAN_PASSWORD": SECRET},
            extra_stubs=self.stubs, script=FAMILY,
        )
        more = self.state / "calls"
        return code, output, calls + (more.read_text() if more.exists() else "")

    def test_a_new_guardians_password_reaches_only_chpasswd(self):
        code, output, calls = self.run_family()
        self.assertEqual(code, 0, output)
        self.assertIn("done: ", output)
        self.assertNotIn(SECRET, output)
        self.assertIn(f"chpasswd stdin: gina:{SECRET}", calls.splitlines())
        for line in calls.splitlines():
            if not line.startswith("chpasswd stdin:"):
                self.assertNotIn(SECRET, line)
        # The trace was on before the password step, and is on again after it.
        self.assertIn("+ useradd --create-home --groups wheel gina", output)
        self.assertIn("+ make_children", output)

    def test_an_existing_guardians_run_never_prints_it(self):
        # The step does not run, so the variable stays set for the whole run,
        # and no stand-in may be given it either.
        code, output, calls = self.run_family({"gina": (1001, ["gina", "wheel", "cairn-guardian"])})
        self.assertEqual(code, 0, output)
        self.assertIn("done: ", output)
        self.assertNotIn(SECRET, output)
        self.assertNotIn(SECRET, calls)
        self.assertNotIn("chpasswd", calls)


# getfacl's dump: the old number in entries that must move, and in places
# that must not.
ACL_DUMP = """# file: /var/lib/cairn/steam-library/steamapps/probe
# owner: 0
# group: 945
user::rw-
user:945:rwx
group::rwx
group:945:rwx
group:9450:rwx
mask::rwx
other::r--
default:group:945:rwx

"""


class Renumber(Scenario):
    """Stand-ins for everything the renumbering asks of the system. find
    prints a file for the old number until the log shows groupmod ran,
    unless $STATE/stays is there. loginctl answers a member's sessions
    from $STATE/sessions-NAME and a session's Type or State from
    $STATE/session-ID-Type or $STATE/session-ID-State."""

    script = RENUMBER

    def setUp(self):
        super().setUp()
        self.groups("cairn-l1:x:941:ada\ncairn-guardian:x:945:gina\n")
        (self.state / "acl").write_text(ACL_DUMP)
        self.stubs.update({
            "id": "#!/bin/bash\necho 0\n",
            "loginctl": r"""#!/bin/bash
case "$1" in
    show-user) [ "$3" = --property=Sessions ] && cat "$STATE/sessions-$2" ;;
    show-session) cat "$STATE/session-$2-${3#--property=}" ;;
esac
""",
            "pgrep": '#!/bin/bash\n[ -e "$STATE/steam-$2" ]\n',
            "find": r"""#!/bin/bash
echo "find $*" >> "$STATE/calls"
if [ -e "$STATE/stays" ] || ! grep -q '^groupmod' "$STATE/calls"; then
    case " $* " in
        *" -print0 "*) printf '/var/home/gina/a file\0' ;;
        *" -print "*) echo "/var/home/gina/a file" ;;
    esac
fi
""",
            "chgrp": logger("chgrp"),
            "getfacl": '#!/bin/bash\necho "getfacl $*" >> "$STATE/calls"\ncat "$STATE/acl"\n',
            "setfacl": '#!/bin/bash\necho "setfacl $*" >> "$STATE/calls"\ncat > "$STATE/restored"\n',
            "groupmod": logger("groupmod"),
        })

    def renumber(self, snippet="main"):
        places = " ".join(shlex.quote(str(self.state / p)) for p in ("etc", "home"))
        (self.state / "etc").mkdir(exist_ok=True)
        (self.state / "home").mkdir(exist_ok=True)
        return self.run_script(f"REOWN_PLACES=({places})\n{snippet}")


class RenumberTest(Renumber):
    def test_the_right_number_changes_nothing(self):
        self.groups("cairn-guardian:x:465:gina\n")
        code, output, calls = self.renumber()
        self.assertEqual(code, 0, output)
        self.assertEqual(calls, [])
        self.assertIn("done: 0 change(s)", output)

    def test_files_move_before_the_group_and_again_after(self):
        code, output, calls = self.renumber()
        self.assertEqual(code, 0, output)
        names = [call.split()[0] for call in calls]
        groupmod = names.index("groupmod")
        self.assertEqual(calls[groupmod], "groupmod --gid 465 cairn-guardian")
        for before in ("find", "chgrp", "getfacl", "setfacl"):
            self.assertLess(names.index(before), groupmod, before)
        after = names[groupmod + 1:]
        self.assertIn("find", after)
        self.assertIn("setfacl", after)
        self.assertIn("chgrp -h -- 465 /var/home/gina/a file", calls)
        self.assertIn("changed: cairn-guardian is number 465 now (was 945)", output)
        self.assertIn("log in again", output)

    def test_the_walk_stays_on_each_filesystem(self):
        _, _, calls = self.renumber()
        walks = [call for call in calls if call.startswith("find")]
        self.assertTrue(walks)
        for walk in walks:
            self.assertIn(" -xdev -ignore_readdir_race ", walk)
            self.assertIn(" -gid 945 ", walk)
        restores = [call for call in calls if call.startswith("setfacl")]
        self.assertTrue(all(call == "setfacl -P --restore=-" for call in restores), restores)

    def test_only_the_old_numbers_acl_entries_move(self):
        self.renumber()
        restored = (self.state / "restored").read_text().splitlines()
        for line in ("group:465:rwx", "default:group:465:rwx", "user:945:rwx",
                     "group:9450:rwx", "# group: 945", "group::rwx"):
            self.assertIn(line, restored)
        self.assertNotIn("group:945:rwx", restored)
        self.assertNotIn("default:group:945:rwx", restored)

    def test_a_file_still_on_the_old_number_is_named(self):
        (self.state / "stays").touch()
        code, output, _ = self.renumber()
        self.assertEqual(code, 0, output)
        self.assertIn("note: /var/home/gina/a file still has the old number 945", output)

    def test_no_file_left_says_nothing_more(self):
        (self.state / "acl").write_text("")
        code, output, _ = self.renumber()
        self.assertEqual(code, 0, output)
        self.assertNotIn("still has the old number", output)

    def test_no_acl_data_reaches_the_trace(self):
        # The trace stays on, and only the commands are in it.
        code, output, _ = self.renumber()
        self.assertEqual(code, 0, output)
        self.assertIn("+ groupmod --gid 465 cairn-guardian", output)
        self.assertNotIn("user:945:rwx", output)

    def test_a_vanished_file_does_not_stop_the_walk_after_groupmod(self):
        # find, chgrp, getfacl and setfacl all fail, as they do when a file
        # goes in the middle of a walk; the run still reaches its report.
        for name in ("find", "getfacl", "setfacl"):
            self.stubs[name] = self.stubs[name] + "exit 1\n"
        self.stubs["chgrp"] = logger("chgrp") + "exit 1\n"
        code, output, calls = self.renumber()
        self.assertEqual(code, 0, output)
        self.assertIn("groupmod --gid 465 cairn-guardian", calls)
        self.assertIn("done: 1 change(s)", output)
        self.assertNotIn("run again with", output)

    def test_a_run_stopped_after_groupmod_names_the_rerun(self):
        code, output, _ = self.renumber("left_behind() { exit 3; }\nmain")
        self.assertNotEqual(code, 0, output)
        self.assertIn("run again with: ", output)
        self.assertTrue(output.rstrip().endswith(" 945"), output)

    def test_a_rerun_with_the_old_number_moves_what_was_left(self):
        self.groups("cairn-guardian:x:465:gina\n")
        (self.state / "stays").touch()
        (self.state / "acl").write_text("")
        code, output, calls = self.renumber("main 945")
        self.assertEqual(code, 0, output)
        self.assertIn("chgrp -h -- 465 /var/home/gina/a file", calls)
        self.assertFalse(any(call.startswith("groupmod") for call in calls), calls)
        self.assertTrue(all(" -gid 945 " in c for c in calls if c.startswith("find")), calls)
        self.assertIn("done: 0 change(s)", output)

    def test_a_guardians_text_login_does_not_count(self):
        # The ssh login running the script, and a Ctrl+Alt+F3 console.
        (self.state / "sessions-gina").write_text("19 c3\n")
        (self.state / "session-19-Type").write_text("tty\n")
        (self.state / "session-c3-Type").write_text("tty\n")
        code, output, calls = self.renumber()
        self.assertEqual(code, 0, output)
        self.assertIn("groupmod --gid 465 cairn-guardian", calls)

    def test_a_closing_desktop_does_not_count(self):
        (self.state / "sessions-gina").write_text("2\n")
        (self.state / "session-2-Type").write_text("wayland\n")
        (self.state / "session-2-State").write_text("closing\n")
        code, output, calls = self.renumber()
        self.assertEqual(code, 0, output)
        self.assertIn("groupmod --gid 465 cairn-guardian", calls)


class RenumberRefusalTest(Renumber):
    def assertRefusedUnchanged(self, needle):
        code, output, calls = self.renumber()
        self.assertEqual(code, 1, output)
        self.assertIn(f"refused: {needle}", output)
        self.assertEqual([c for c in calls if not c.startswith(("getent",))], [])

    def test_a_guardian_on_the_screen(self):
        (self.state / "sessions-gina").write_text("2\n")
        (self.state / "session-2-Type").write_text("wayland\n")
        (self.state / "session-2-State").write_text("active\n")
        self.assertRefusedUnchanged("gina is logged in on the screen")

    def test_a_guardian_on_the_screen_and_over_ssh(self):
        (self.state / "sessions-gina").write_text("19 2\n")
        (self.state / "session-19-Type").write_text("tty\n")
        (self.state / "session-2-Type").write_text("x11\n")
        self.assertRefusedUnchanged("gina is logged in on the screen")

    def test_a_rerun_before_the_group_moved(self):
        code, output, calls = self.renumber("main 945")
        self.assertEqual(code, 1, output)
        self.assertIn("refused: cairn-guardian is still number 945", output)
        self.assertEqual([c for c in calls if not c.startswith(("getent",))], [])

    def test_a_rerun_from_another_groups_number(self):
        self.groups("cairn-guardian:x:465:gina\ngames:x:945:\n")
        code, output, calls = self.renumber("main 945")
        self.assertEqual(code, 1, output)
        self.assertIn("refused: number 945 belongs to the group games", output)
        self.assertEqual(calls, [])

    def test_a_guardians_steam(self):
        (self.state / "steam-gina").touch()
        self.assertRefusedUnchanged("Steam is open for gina")

    def test_the_number_taken_by_another_group(self):
        self.groups("cairn-guardian:x:945:gina\ngames:x:465:\n")
        self.assertRefusedUnchanged("number 465 belongs to the group games")

    def test_no_group(self):
        self.groups("cairn-l1:x:941:ada\n")
        self.assertRefusedUnchanged("there is no cairn-guardian group")

    def test_not_as_root(self):
        self.stubs["id"] = "#!/bin/bash\necho 1000\n"
        code, output, calls = self.renumber()
        self.assertEqual(code, 1, output)
        self.assertEqual(calls, [])


class RenumberNumbersTest(Renumber):
    def test_the_images_list_on_the_image_and_the_checkouts_otherwise(self):
        code, output, _ = self.run_script(
            'GROUP_NUMBERS=checkout\nchoose_group_numbers\necho "numbers: $GROUP_NUMBERS"'
        )
        self.assertEqual(code, 0, output)
        self.assertIn(f"numbers: {self.mark}", output.splitlines())
        code, output, _ = self.run_script(
            'GROUP_NUMBERS=checkout\nchoose_group_numbers\necho "numbers: $GROUP_NUMBERS"', mark=False
        )
        self.assertIn("numbers: checkout", output.splitlines())

    def test_the_places_leave_the_stores_alone(self):
        text = RENUMBER.read_text()
        self.assertIn("REOWN_PLACES=(/etc /var/lib/cairn /var/usrlocal /var/home /var/tmp)", text)
        self.assertIn("chgrp -h", text)


class ImageFoldersTest(unittest.TestCase):
    """The folders the image makes in /var at every boot (ADR-0038)."""

    def test_the_childrens_lists_have_a_root_folder(self):
        # cairn-family save-games writes each child's list there and
        # refuses a folder that is not root's own (ADR-0041).
        tmpfiles = (HERE.parent / "system_files/usr/lib/tmpfiles.d/cairn.conf").read_text()
        lines = [line.split() for line in tmpfiles.splitlines() if line and not line.startswith("#")]
        self.assertIn(["d", "/var/lib/cairn/games", "0755", "root", "root", "-"], lines)

    def test_steams_tools_have_a_folder_guardians_change_and_everyone_reads(self):
        # The tools step writes it as a Guardian, and the copy step reads it
        # as each child (ADR-0046), with the shared folder's own ACL.
        tmpfiles = (HERE.parent / "system_files/usr/lib/tmpfiles.d/cairn.conf").read_text()
        lines = [line.split() for line in tmpfiles.splitlines() if line and not line.startswith("#")]
        self.assertIn(["d", "/var/lib/cairn/steam-tools", "2775", "root", "cairn-guardian", "-"], lines)
        acls = {line[1]: line[6] for line in lines if line[0] == "a"}
        self.assertEqual(acls["/var/lib/cairn/steam-tools"], acls["/var/lib/cairn/steam-library"])
        self.assertIn("d:g:cairn-guardian:rwx", acls["/var/lib/cairn/steam-tools"])
        self.assertIn("d:o::r-x", acls["/var/lib/cairn/steam-tools"])


if __name__ == "__main__":
    unittest.main()
