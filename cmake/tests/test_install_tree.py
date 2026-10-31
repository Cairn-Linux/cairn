#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Installs the build into a temporary folder with the prefix /usr, as the
image does, and checks every file in it (ADR-0038).

Standard library only. Run by CTest with three arguments: the cmake
program, the build folder, and the prefix the build was configured with.
The paths inside the files are fixed when the build is configured, so a
build for provisioning's /usr/local is skipped rather than checked.
"""

import os
import pathlib
import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ElementTree

SKIPPED = 77
REPO = pathlib.Path(__file__).resolve().parents[2]

# Every file the image gets from the build, and its mode.
EXPECTED = {
    "etc/pam.d/cairn-steam-visit": 0o644,
    "etc/pam.d/login": 0o644,
    "etc/pam.d/sddm": 0o644,
    "etc/sddm.conf.d/zz-cairn-no-autologin.conf": 0o644,
    "etc/security/faillock.conf": 0o644,
    "usr/bin/cairn-give-up": 0o755,
    # The group, cairn-guardian, is set when the image is made.
    "usr/bin/cairn-guardian": 0o750,
    "usr/bin/cairn-launcher": 0o755,
    "usr/bin/cairn-log-out": 0o755,
    "usr/bin/cairn-session": 0o755,
    # The group, cairn-guardian, is set when the image is made.
    "usr/bin/cairn-steam-for": 0o750,
    "usr/lib/sddm/sddm.conf.d/zz-cairn.conf": 0o644,
    "usr/lib/systemd/logind.conf.d/10-cairn-power.conf": 0o644,
    "usr/lib64/cairn/libCairnBrand.so": 0o755,
    "usr/lib64/cairn/qml/Cairn/Brand/Tokens.qml": 0o644,
    "usr/lib64/cairn/qml/Cairn/Brand/qmldir": 0o644,
    "usr/lib64/cairn/qml/Cairn/Greeter/cairngreeter.qmltypes": 0o644,
    "usr/lib64/cairn/qml/Cairn/Greeter/libcairngreeter.so": 0o755,
    "usr/lib64/cairn/qml/Cairn/Greeter/libcairngreeterplugin.so": 0o755,
    "usr/lib64/cairn/qml/Cairn/Greeter/qmldir": 0o644,
    "usr/libexec/cairn/cairn-copy-games": 0o644,
    # Root runs it, through pkexec or sudo; anyone may read it.
    "usr/libexec/cairn/cairn-family": 0o755,
    "usr/libexec/cairn/cairn-game-pictures": 0o644,
    "usr/libexec/cairn/cairn-guardian": 0o750,
    "usr/libexec/cairn/cairn-launcher": 0o755,
    "usr/libexec/cairn/cairn-share-tools": 0o644,
    "usr/libexec/cairn/cairn-steam-for": 0o750,
    "usr/libexec/cairn/cairn-steam-visit": 0o755,
    "usr/libexec/cairn/kidscan.py": 0o644,
    "usr/share/cairn/labwc/environment": 0o644,
    "usr/share/cairn/labwc/rc.xml": 0o644,
    "usr/share/cairn/manifest.json": 0o644,
    "usr/share/applications/cairn-guardian.desktop": 0o644,
    "usr/share/cairn/sessions/cairn.desktop": 0o644,
    "usr/share/plasma/systemsettings/externalmodules/cairn-guardian-settings.desktop": 0o644,
    "usr/share/polkit-1/actions/com.cairnlinux.family.policy": 0o644,
    "usr/share/polkit-1/actions/com.cairnlinux.steam-visit.policy": 0o644,
    "usr/share/polkit-1/rules.d/10-cairn-levels.rules": 0o644,
    "usr/share/sddm/themes/cairn/FamilyTile.qml": 0o644,
    "usr/share/sddm/themes/cairn/Main.qml": 0o644,
    "usr/share/sddm/themes/cairn/PasswordRow.qml": 0o644,
    "usr/share/sddm/themes/cairn/TextButton.qml": 0o644,
    "usr/share/sddm/themes/cairn/mark-on-ink.svg": 0o644,
    "usr/share/sddm/themes/cairn/metadata.desktop": 0o644,
    "usr/share/sddm/themes/cairn/theme.conf": 0o644,
}


class InstallTree(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.root = pathlib.Path(cls.folder.name)
        environment = dict(os.environ, DESTDIR=cls.folder.name)
        subprocess.run([CMAKE, "--install", BUILD, "--prefix", "/usr"], env=environment, check=True,
                       stdout=subprocess.DEVNULL)

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def installed(self):
        return {str(path.relative_to(self.root)): path for path in self.root.rglob("*")
                if not path.is_dir() or path.is_symlink()}

    def text(self, name):
        return (self.root / name).read_text()

    def test_every_file_and_no_other(self):
        self.assertEqual(sorted(self.installed()), sorted(EXPECTED))

    def test_modes(self):
        for name, path in self.installed().items():
            with self.subTest(name=name):
                self.assertFalse(path.is_symlink(), "a link would lead out of the image")
                self.assertTrue(path.is_file())
                self.assertEqual(path.stat().st_mode & 0o7777, EXPECTED.get(name), oct(path.stat().st_mode))

    def test_nothing_under_usr_local_or_var(self):
        # /usr/local is a link to /var/usrlocal on Bazzite, and /var is the
        # machine's own, so an image cannot put files or folders in either.
        for path in self.root.rglob("*"):
            name = path.relative_to(self.root).as_posix()
            with self.subTest(name=name):
                self.assertNotIn(name, ("usr/local", "var"))
                self.assertFalse(name.startswith(("usr/local/", "var/")))

    def test_no_file_names_usr_local(self):
        for name, path in self.installed().items():
            with self.subTest(name=name):
                self.assertFalse(b"/usr/local" in path.read_bytes())

    def test_launcher_wrapper_names_the_image_paths(self):
        wrapper = self.text("usr/bin/cairn-launcher")
        self.assertIn("launcher=(/usr/libexec/cairn/cairn-launcher"
                      " --manifest /usr/share/cairn/manifest.json\n", wrapper)
        self.assertIn("--log-out /usr/bin/cairn-log-out\n", wrapper)
        # Each child's own list, never the full one (ADR-0041).
        self.assertIn('games=(--games "/var/lib/cairn/games/$login.json")', wrapper)
        self.assertNotIn("/var/lib/cairn/games.json", wrapper)
        # The launcher finds its library through its rpath, so the games it
        # starts do not inherit a library path.
        self.assertNotIn("LD_LIBRARY_PATH", wrapper)

    def test_steam_for_wrapper_names_the_image_path(self):
        wrapper = self.text("usr/bin/cairn-steam-for")
        self.assertIn('window=(/usr/libexec/cairn/cairn-steam-for "$@")\n', wrapper)
        self.assertNotIn("LD_LIBRARY_PATH", wrapper)

    def test_guardian_wrapper_names_the_image_path(self):
        wrapper = self.text("usr/bin/cairn-guardian")
        self.assertIn('window=(/usr/libexec/cairn/cairn-guardian "$@")\n', wrapper)
        self.assertNotIn("LD_LIBRARY_PATH", wrapper)

    def test_guardian_menu_entry_names_the_wrapper(self):
        # TryExec hides the entry from everyone who may not run the wrapper.
        entry = self.text("usr/share/applications/cairn-guardian.desktop").splitlines()
        self.assertIn("Name=Family", entry)
        self.assertIn("Exec=/usr/bin/cairn-guardian", entry)
        self.assertIn("TryExec=/usr/bin/cairn-guardian", entry)

    def test_system_settings_lists_family(self):
        # "Family" in System Settings, where the hidden Users page was
        # (ADR-0043), runs the same wrapper.
        entry = self.text("usr/share/plasma/systemsettings/externalmodules/"
                          "cairn-guardian-settings.desktop").splitlines()
        self.assertIn("Name=Family", entry)
        self.assertIn("X-KDE-ServiceTypes=SystemSettingsExternalApp", entry)
        self.assertIn("X-KDE-System-Settings-Parent-Category=system-administration", entry)
        self.assertIn("Exec=/usr/bin/cairn-guardian", entry)
        self.assertIn("TryExec=/usr/bin/cairn-guardian", entry)

    def test_programs_find_the_brand_library(self):
        for name in ("usr/libexec/cairn/cairn-launcher", "usr/libexec/cairn/cairn-steam-for",
                     "usr/libexec/cairn/cairn-guardian"):
            with self.subTest(name=name):
                self.assertTrue(b"/usr/lib64/cairn\0" in (self.root / name).read_bytes())

    def test_steam_for_runs_the_image_helper(self):
        # A QStringLiteral is stored as UTF-16.
        program = (self.root / "usr/libexec/cairn/cairn-steam-for").read_bytes()
        helper = "/usr/libexec/cairn/cairn-steam-visit\0".encode("utf-16-le")
        self.assertTrue(helper in program)

    def test_guardian_runs_the_image_family_command(self):
        # pkexec matches the command by the path the polkit action names.
        program = (self.root / "usr/libexec/cairn/cairn-guardian").read_bytes()
        command = "/usr/libexec/cairn/cairn-family\0".encode("utf-16-le")
        self.assertTrue(command in program)

    def test_polkit_actions_name_their_programs(self):
        # pkexec matches the program's real path; /usr/libexec is one.
        # cairn-family's file has one action for each subcommand.
        actions = {
            "com.cairnlinux.steam-visit": ["/usr/libexec/cairn/cairn-steam-visit"],
            "com.cairnlinux.family": ["/usr/libexec/cairn/cairn-family"] * 5,
        }
        for action, programs in actions.items():
            with self.subTest(action=action):
                tree = ElementTree.parse(self.root / f"usr/share/polkit-1/actions/{action}.policy")
                paths = [note.text for note in tree.iter("annotate")
                         if note.get("key") == "org.freedesktop.policykit.exec.path"]
                self.assertEqual(paths, programs)

    def test_family_command_names_the_image_paths(self):
        # QStringLiterals are stored as UTF-16. The menu entries' folder is
        # the computer's own, /usr/local/share/applications, on purpose:
        # /usr/local is a link to /var/usrlocal there (ADR-0038).
        program = (self.root / "usr/libexec/cairn/cairn-family").read_bytes()
        for path in ("/usr/bin/cairn-steam-for\0", "/usr/local/share/applications\0",
                     "/usr/sbin/useradd\0", "/usr/bin/passwd\0", "/usr/bin/gpasswd\0",
                     "/usr/libexec/cairn/cairn-steam-visit\0",
                     "/usr/libexec/cairn/cairn-game-pictures\0",
                     "/usr/libexec/cairn/kidscan.py\0", "/usr/bin/systemd-run\0",
                     "/usr/bin/systemd-inhibit\0",
                     "/usr/bin/python3\0"):
            with self.subTest(path=path):
                self.assertTrue(path.encode("utf-16-le") in program)

    def test_sddm_settings_name_the_image_paths(self):
        settings = self.text("usr/lib/sddm/sddm.conf.d/zz-cairn.conf").splitlines()
        self.assertIn("GreeterEnvironment=QT_WAYLAND_SHELL_INTEGRATION=layer-shell,"
                      "QML_IMPORT_PATH=/usr/lib64/cairn/qml", settings)
        self.assertIn("SessionDir=/usr/share/cairn/sessions", settings)
        self.assertIn("ThemeDir=/usr/share/sddm/themes", settings)
        self.assertIn("Current=cairn", settings)

    def test_sddm_settings_sort_after_fedoras(self):
        # SDDM reads its packaged folder in name order, and Fedora's
        # plasma-wayland.conf sets GreeterEnvironment too.
        for path in (self.root / "usr/lib/sddm/sddm.conf.d").iterdir():
            with self.subTest(name=path.name):
                self.assertGreater(path.name, "plasma-wayland.conf")

    def test_session_and_helper_name_the_image_paths(self):
        self.assertIn('"${CAIRN_LABWC_CONFIG:-/usr/share/cairn/labwc}"', self.text("usr/bin/cairn-session"))
        helper = self.text("usr/libexec/cairn/cairn-steam-visit")
        self.assertIn("CAGE=/usr/libexec/cairn/cage-visit\n", helper)
        self.assertIn("COPY_GAMES=/usr/libexec/cairn/cairn-copy-games\n", helper)

    def test_repository_files_change_only_in_their_paths(self):
        # The theme's mark is a link in the repository; the image gets the file.
        pairs = {
            "usr/share/sddm/themes/cairn/mark-on-ink.svg": "brand/mark-on-ink.svg",
            "usr/share/cairn/manifest.json": "provision/manifest.json",
            "etc/pam.d/login": "session/pam.d/login",
            "etc/pam.d/sddm": "session/sddm/pam.d/sddm",
            "usr/libexec/cairn/kidscan.py": "tools/kidscan/kidscan.py",
            "usr/share/polkit-1/rules.d/10-cairn-levels.rules": "session/polkit/rules.d/10-cairn-levels.rules",
        }
        for installed, source in pairs.items():
            with self.subTest(name=installed):
                self.assertEqual((self.root / installed).read_bytes(), (REPO / source).read_bytes())


if __name__ == "__main__":
    CMAKE, BUILD, PREFIX = sys.argv[1:4]
    if PREFIX != "/usr":
        print(f"skipped: this build is configured for {PREFIX}; the image's is configured for /usr")
        sys.exit(SKIPPED)
    unittest.main(argv=sys.argv[:1])
