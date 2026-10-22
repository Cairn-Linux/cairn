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

from test_provision import GROUP_GETENT, GROUP_NUMBERS, getent_stub, run_steps

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
        for step in ("ensure_guardian", "ensure_children", "make_steam_entries",
                     "make_shared_library", "make_game_tiles", "copy_games", "install_flatpaks"):
            self.assertIn(step, steps)

    def test_checks_come_before_any_change(self):
        steps = main_steps(FAMILY)
        first_change = steps.index("ensure_image_groups")
        for check in ("parse_arguments \"$@\"", "check_image", "check_accounts",
                      "check_checkout", "check_guardian_number"):
            self.assertLess(steps.index(check), first_change, check)

    def test_the_tiles_come_before_the_copies_and_the_flatpaks_last(self):
        steps = main_steps(FAMILY)
        self.assertLess(steps.index("make_shared_library"), steps.index("make_game_tiles"))
        self.assertLess(steps.index("make_game_tiles"), steps.index("copy_games"))
        changes = [s for s in steps if not s.startswith(("set ", "echo ", "parse_", "check_", "["))]
        self.assertEqual(changes[-1], "install_flatpaks")

    def test_names_the_images_paths(self):
        text = FAMILY.read_text()
        self.assertIn("\nPREFIX=/usr\n", text)
        self.assertIn("\nSHARE=/usr/share/cairn\n", text)
        self.assertIn('\nGROUP_NUMBERS="$IMAGE_MARK"\n', text)


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

    def check(self):
        return self.run_script(
            f"REPO={shlex.quote(str(self.repo))}\nPREFIX={shlex.quote(str(self.state / 'prefix'))}\n"
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


class FamilyEntriesTest(Scenario):
    """The entries name the image's window and go in the machine's folder;
    the window itself is the image's and is never changed."""

    def test_entries_name_the_images_window(self):
        apps = self.state / "applications"
        stubs = {
            "getent": getent_stub(gecos={"ada": "Ada"}, groups={"cairn-l1": ["ada"], "cairn-l2": [],
                                                                "cairn-l3": [], "cairn-l4": []}),
            "chgrp": logger("chgrp"), "chmod": logger("chmod"),
        }
        self.stubs = stubs
        code, output, calls = self.run_script(
            f"APPS={shlex.quote(str(apps))}\nmake_steam_entries"
        )
        self.assertEqual(code, 0, output)
        entry = (apps / "cairn-steam-for-ada.desktop").read_text()
        self.assertIn("Exec=/usr/bin/cairn-steam-for ada\n", entry)
        self.assertIn("TryExec=/usr/bin/cairn-steam-for\n", entry)
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


if __name__ == "__main__":
    unittest.main()
