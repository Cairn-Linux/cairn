#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""build_files/kde-pictures.sh (ADR-0043, #166): only KDE's own pictures
left in Plasma's picture folder. The pictures are empty files in a folder
of the test's own, and a stand-in rpm answers for them as the image's rpm
did in the cairn-iso VM on 2026-10-31."""

import os
import subprocess
import tempfile
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parents[2] / "build_files" / "kde-pictures.sh"

AVATARS = "/usr/share/plasma/avatars"
FACES = "/usr/share/pixmaps/faces"

# KDE's own: its drawings at the top, its photos with their licences in a
# folder of their own.
KDE = [
    f"{AVATARS}/Artist Konqi.png",
    f"{AVATARS}/Katie.png",
    f"{AVATARS}/Konqi.png",
    f"{AVATARS}/photos/Air Balloon.png",
    f"{AVATARS}/photos/Air Balloon.png.license",
    f"{AVATARS}/photos/Owl.png",
    f"{AVATARS}/photos/Owl.png.license",
]
KDE_FOLDERS = [AVATARS, f"{AVATARS}/photos"]

# Bazzite's: links no package owns, to its copies under GNOME's names.
LINKS = {
    f"{AVATARS}/alyx.jpg": f"{FACES}/hummingbird.jpg",
    f"{AVATARS}/gmod_gman.jpg": f"{FACES}/calculator.jpg",
    f"{AVATARS}/pyro.jpg": f"{FACES}/headphones.jpg",
}

# The Steam Deck look's folders, which its package owns.
DECK = [
    f"{AVATARS}/Portal2/chell.png",
    f"{AVATARS}/circles/Cat.png",
    f"{AVATARS}/circles/Konqui.png",
    f"{AVATARS}/tf2/heavy.png",
    f"{AVATARS}/tf2/pyro.png",
]
DECK_FOLDERS = [f"{AVATARS}/Portal2", f"{AVATARS}/circles", f"{AVATARS}/tf2"]

# Beside the two folders, where the script has no business.
ELSEWHERE = ["/usr/share/pixmaps/system-logo.png", "/usr/share/plasma/look-and-feel/notes.txt"]

# rpm as the script asks it, answering from the table in RPM_OWNERS, one
# path and package a line, a tab between. It refuses any other question,
# and with RPM_BROKEN set it fails as rpm does without its database.
RPM = r"""#!/bin/sh
if [ -n "$RPM_BROKEN" ]; then
    echo "error: cannot open Packages database in /usr/lib/sysimage/rpm" >&2
    exit 1
fi
if [ $# -ne 6 ] || [ "$1" != --root ] || [ "$2" != "$RPM_ROOT" ] || [ "$3" != -qf ] \
    || [ "$4" != --qf ] || [ "$5" != '%{NAME}\n' ]; then
    echo "rpm stand-in: not a question kde-pictures.sh asks: $*" >&2
    exit 2
fi
owners=$(WANTED="$6" awk -F '\t' '$1 == ENVIRON["WANTED"] { print $2 }' "$RPM_OWNERS")
if [ -z "$owners" ]; then
    echo "file $6 is not owned by any package"
    exit 1
fi
printf '%s\n' "$owners"
"""


# The image's build stage runs the tests without build_files/; the image's
# own check, check-guardian-desktop.sh, covers the step there. Only the
# missing folder skips, so a script renamed or gone fails here.
@unittest.skipUnless(SCRIPT.parent.is_dir(), "the build stage does not copy build_files")
class KdePicturesTest(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        work = Path(self.folder.name)
        self.root = work / "root"
        bin_folder = work / "bin"
        bin_folder.mkdir()
        rpm = bin_folder / "rpm"
        rpm.write_text(RPM)
        rpm.chmod(0o755)
        self.owners = work / "owners"
        self.env = dict(os.environ, PATH=f"{bin_folder}:{os.environ['PATH']}",
                        RPM_ROOT=str(self.root), RPM_OWNERS=str(self.owners))
        self.table = {}
        for path in KDE + KDE_FOLDERS:
            self.table[path] = ["plasma-workspace"]
        for path in DECK + DECK_FOLDERS:
            self.table[path] = ["steamdeck-kde-presets-desktop"]
        for path in KDE + DECK + list(LINKS.values()) + ELSEWHERE:
            self.make_file(path)
        for link, picture in LINKS.items():
            self.at(link).symlink_to(picture)

    def tearDown(self):
        self.folder.cleanup()

    def at(self, path):
        """Where a path of the system is in the test's own folder."""
        return self.root / path.lstrip("/")

    def make_file(self, path):
        self.at(path).parent.mkdir(parents=True, exist_ok=True)
        self.at(path).touch()

    def run_script(self, *arguments):
        self.owners.write_text("".join(
            f"{path}\t{package}\n" for path, packages in self.table.items()
            for package in packages))
        return subprocess.run(["bash", str(SCRIPT), *arguments, str(self.root)],
                              env=self.env, capture_output=True, text=True)

    def left(self, folder):
        """Every path in a folder of the system, folders too, as rpm names
        them."""
        return sorted("/" + str(path.relative_to(self.root))
                      for path in self.at(folder).rglob("*"))

    def test_leaves_only_kdes_pictures(self):
        result = self.run_script()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.left(AVATARS), sorted(KDE + [f"{AVATARS}/photos"]))

    def test_takes_bazzites_copies_and_their_folder(self):
        result = self.run_script()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse(self.at(FACES).exists())

    def test_leaves_everything_else_alone(self):
        result = self.run_script()
        self.assertEqual(result.returncode, 0, result.stderr)
        for path in ELSEWHERE:
            self.assertTrue(self.at(path).is_file(), path)

    def test_a_dry_run_names_each_path_and_removes_nothing(self):
        before = self.left("/usr")
        result = self.run_script("--dry-run")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(sorted(result.stdout.splitlines()), sorted(
            f"Would remove {path}"
            for path in list(LINKS) + list(LINKS.values()) + [FACES] + DECK + DECK_FOLDERS))
        self.assertEqual(self.left("/usr"), before)

    def test_a_second_run_changes_nothing(self):
        self.assertEqual(self.run_script().returncode, 0)
        once = self.left("/usr")
        result = self.run_script()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.left("/usr"), once)

    def test_keeps_a_face_a_package_owns(self):
        owned = f"{FACES}/packaged.jpg"
        self.make_file(owned)
        self.table[owned] = ["some-faces"]
        self.table[FACES] = ["some-faces"]
        result = self.run_script()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.left(FACES), [owned])

    def test_a_picture_owned_by_kde_and_another_package_stays(self):
        self.table[f"{AVATARS}/Katie.png"] = ["another-package", "plasma-workspace"]
        result = self.run_script()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue(self.at(f"{AVATARS}/Katie.png").is_file())

    def test_stops_rather_than_remove_a_kde_picture_in_a_folder_not_kdes(self):
        kept = f"{AVATARS}/extra/Konqi.png"
        self.make_file(kept)
        self.table[kept] = ["plasma-workspace"]
        result = self.run_script()
        self.assertNotEqual(result.returncode, 0)
        self.assertTrue(self.at(kept).is_file())

    def test_stops_before_anything_goes_when_rpm_cannot_answer(self):
        before = self.left("/usr")
        self.env["RPM_BROKEN"] = "1"
        result = self.run_script()
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.left("/usr"), before)

    def test_has_the_licence_line(self):
        self.assertEqual(SCRIPT.read_text().splitlines()[1],
                         "# SPDX-License-Identifier: Apache-2.0")


if __name__ == "__main__":
    unittest.main()
