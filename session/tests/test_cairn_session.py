#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Runs bin/cairn-session with stand-ins on PATH and checks which session it
picks for each level group (ADR-0012).

Standard library only. The stand-ins are tiny shell scripts: `id` prints
the groups a test asks for, and the login, the first of them, for `id -un`,
and `labwc`, `startplasma-wayland`, `systemctl` and `systemd-cat` print
their name and arguments instead of starting anything. One test has
`startplasma-wayland` print the PATH it was given, and the language tests
(ADR-0058) have the session print its environment, with the language
folder moved to a temporary one in a copy of the script.
"""

import os
import pathlib
import subprocess
import tempfile
import unittest

DISPATCHER = pathlib.Path(__file__).resolve().parent.parent / "bin" / "cairn-session"


def write_stubs(bin_dir, stubs):
    for name, body in stubs.items():
        stub = pathlib.Path(bin_dir, name)
        stub.write_text(body)
        stub.chmod(0o755)


def id_stub(groups):
    login = groups.split()[0] if groups.split() else ""
    return f'#!/bin/sh\nif [ "$1" = -un ]; then echo "{login}"; else echo "{groups}"; fi\n'


def run_dispatcher(groups, systemctl_fails=False, journal_missing=False):
    with tempfile.TemporaryDirectory() as bin_dir:
        stubs = {
            "id": id_stub(groups),
            "labwc": '#!/bin/sh\necho "labwc $*"\n',
            "startplasma-wayland": '#!/bin/sh\necho "startplasma-wayland $*"\n',
            "systemctl": f'#!/bin/sh\necho "systemctl $*"\nexit {1 if systemctl_fails else 0}\n',
            "systemd-cat": f'#!/bin/sh\necho "systemd-cat $*"\nexit {1 if journal_missing else 0}\n',
        }
        write_stubs(bin_dir, stubs)
        env = {"PATH": f"{bin_dir}:/usr/bin:/bin", "CAIRN_LABWC_CONFIG": "/kiosk"}
        result = subprocess.run(
            [str(DISPATCHER)], env=env, capture_output=True, text=True, check=False
        )
        lines = result.stdout.strip().splitlines()
        # The session itself is the last thing started; anything before it
        # is returned apart.
        return result.returncode, lines[-1] if lines else "", lines[:-1]


def path_given_to_plasma(groups):
    # The login's PATH as Fedora's .bashrc leaves it: two folders in the
    # account's own home first, then the stand-ins and the system's folders.
    login = groups.split()[0]
    with tempfile.TemporaryDirectory() as bin_dir:
        stubs = {
            "id": id_stub(groups),
            "startplasma-wayland": '#!/bin/sh\necho "$PATH"\n',
        }
        write_stubs(bin_dir, stubs)
        given = f"/var/home/{login}/.local/bin:/var/home/{login}/bin:{bin_dir}:/usr/bin:/bin"
        result = subprocess.run(
            [str(DISPATCHER)], env={"PATH": given}, capture_output=True, text=True, check=False
        )
        return result.stdout.strip(), given


LANGUAGE_DIR = "/var/lib/cairn/locale/"
CHILD_XDG = "/usr/local/share/cairn/child-xdg"
FEDORA_XDG = "/etc/xdg:/usr/share/kde-settings/kde-profile/default/xdg"
KEYS = ("LANG", "LANGUAGE", "LC_ALL", "LC_TIME", "LC_MESSAGES", "XDG_CONFIG_DIRS")


def environment_given(groups, language=None, given=None):
    """The session's environment, as the program it starts sees it, for
    the variables in KEYS, the folder of language folders, and the calls
    to systemctl. `language` is the text of the account's
    plasma-localerc, or None for none and no folder; `given` is the
    environment the login hands the session, beyond PATH."""
    login = groups.split()[0]
    with tempfile.TemporaryDirectory() as folder:
        root = pathlib.Path(folder)
        bin_dir = root / "bin"
        bin_dir.mkdir()
        languages = root / "locale"
        if language is not None:
            (languages / login).mkdir(parents=True)
            (languages / login / "plasma-localerc").write_text(language)
        script = root / "cairn-session"
        text = DISPATCHER.read_text()
        assert text.count(LANGUAGE_DIR) == 1
        script.write_text(text.replace(LANGUAGE_DIR, f"{languages}/"))
        script.chmod(0o755)
        printing = '#!/bin/sh\nenv > "$CAIRN_TEST_SEEN"\n'
        write_stubs(bin_dir, {
            "id": id_stub(groups),
            "labwc": printing,
            "startplasma-wayland": printing,
            "systemctl": '#!/bin/sh\necho "systemctl $*" >> "$CAIRN_TEST_SEEN.systemctl"\n',
            "systemd-cat": '#!/bin/sh\nshift\nexec "$@"\n',
        })
        seen = root / "seen"
        env = dict(given or {}, PATH=f"{bin_dir}:/usr/bin:/bin", CAIRN_LABWC_CONFIG="/kiosk",
                   CAIRN_TEST_SEEN=str(seen))
        subprocess.run([str(script)], env=env, capture_output=True, text=True, check=True)
        lines = seen.read_text().splitlines()
        values = dict(line.split("=", 1) for line in lines if "=" in line)
        calls = pathlib.Path(f"{seen}.systemctl")
        called = calls.read_text().splitlines() if calls.exists() else []
        return {key: values[key] for key in KEYS if key in values}, languages, called


# Plasma-localerc as cairn-family set-language writes it, cut short.
BRITISH = """# Written by cairn-family set-language (ADR-0058). Family changes it.
[Formats]
LANG[$i]=en_GB.UTF-8
LC_TIME[$i]=en_GB.UTF-8
LC_ALL[$i]=en_GB.UTF-8

[Translations]
LANGUAGE[$i]=en_GB:en
"""


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

    def test_plasma_keeps_the_path_the_login_gave(self):
        # The kiosk's PATH is set in labwc's own environment file (#244),
        # which Plasma never reads, so an L3 or L4 child and a grown-up keep
        # the login's, the folders in their own home included.
        for groups in ("cat cairn-l3", "dan cairn-l4", "guardian wheel cairn-guardian"):
            with self.subTest(groups=groups):
                seen, given = path_given_to_plasma(groups)
                self.assertEqual(seen, given)

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


class ChildLanguageTest(unittest.TestCase):
    """A child's language is the Guardian's (ADR-0058, #299)."""

    def test_an_older_child_gets_the_kiosk_file_after_the_systems(self):
        # After /etc/xdg, never before: its kdeglobals locks the group.
        for groups in ("cat cairn-l3", "dan cairn-l4"):
            with self.subTest(groups=groups):
                seen, _, called = environment_given(groups)
                self.assertEqual(seen, {"XDG_CONFIG_DIRS": f"{FEDORA_XDG}:{CHILD_XDG}"})

    def test_an_older_childs_language_folder_comes_last(self):
        seen, languages, called = environment_given("cat cairn-l3", language=BRITISH)
        # Plasma sets the language from the file, not the session.
        self.assertEqual(seen, {"XDG_CONFIG_DIRS": f"{FEDORA_XDG}:{CHILD_XDG}:{languages}/cat"})
        self.assertEqual(called, [])

    def test_the_folders_the_login_gave_stay_in_front(self):
        given = {"XDG_CONFIG_DIRS": "/etc/xdg"}
        seen, languages, called = environment_given("dan cairn-l4", language=BRITISH, given=given)
        self.assertEqual(seen["XDG_CONFIG_DIRS"], f"/etc/xdg:{CHILD_XDG}:{languages}/dan")

    def test_a_grown_up_and_an_account_cairn_did_not_make_are_left_alone(self):
        # A Guardian picks their own language in Plasma's settings.
        for groups in ("guardian wheel cairn-guardian", "bazzite wheel",
                       "gran cairn-guardian cairn-l3", "grandpa"):
            for given in ({}, {"XDG_CONFIG_DIRS": "/etc/xdg"}):
                with self.subTest(groups=groups, given=given):
                    seen, _, called = environment_given(groups, language=BRITISH, given=given)
                    self.assertEqual(seen, given)

    def test_the_kiosk_takes_the_language_from_the_file(self):
        for groups in ("ada cairn-l1", "ben cairn-l2"):
            with self.subTest(groups=groups):
                seen, _, called = environment_given(groups, language=BRITISH, given={"LANG": "fr_FR.UTF-8"})
                self.assertEqual(seen, {"LANG": "en_GB.UTF-8", "LANGUAGE": "en_GB:en",
                                        "LC_ALL": "en_GB.UTF-8", "LC_TIME": "en_GB.UTF-8"})
                # The child's systemd, for Steam at login, after the sound.
                self.assertEqual(called, [
                    AUDIO,
                    "systemctl --user import-environment LANG LC_TIME LC_ALL LANGUAGE",
                ])

    def test_the_kiosk_keeps_the_login_language_with_no_file(self):
        seen, _, called = environment_given("ada cairn-l1", given={"LANG": "en_US.UTF-8"})
        self.assertEqual(seen, {"LANG": "en_US.UTF-8"})
        self.assertEqual(called, [AUDIO])

    def test_the_kiosk_takes_nothing_else_from_the_file(self):
        # Only a locale's characters, and only the language's variables;
        # nothing in it is ever run.
        text = "\n".join([
            "[Formats]",
            "LANG[$i]=$(touch ran)",
            "LC_TIME[$i]=en_GB.UTF-8; touch ran",
            "LC_MESSAGES[$i]=",
            "PATH[$i]=/tmp",
            "LD_PRELOAD=/tmp/x.so",
            "LC_ALL X[$i]=en_GB.UTF-8",
            "LANGUAGE[$i]=en_GB:en",
            "not a line at all",
        ]) + "\n"
        seen, languages, called = environment_given("ada cairn-l1", language=text)
        self.assertEqual(seen, {"LANGUAGE": "en_GB:en"})
        self.assertEqual(called[1:],
                         ["systemctl --user import-environment LANGUAGE"])
        self.assertFalse((languages.parent / "ran").exists())
        self.assertFalse(pathlib.Path("ran").exists())


if __name__ == "__main__":
    unittest.main()
