#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Writes the manifest kidscan makes from the synthetic Steam library in
test_kidscan.py, with the stand-in scummvm, for the launcher to read (#97).

Given a second path, it also writes the games file provisioning makes
(ADR-0033) from the same library: every game through Steam, from the
library folders only, with a picture for each. The pictures are a few
stand-in bytes in a folder beside that file, never real game art.

CTest runs this before the launcher's tst_kidscanmanifest, which reads the
files. Standard library only.

Run:  python3 tools/kidscan/tests/roundtrip_manifest.py OUTPUT [GAMES]
"""

import shutil
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_kidscan  # noqa: E402  (FakeLibrary and the stand-in scummvm)
from test_kidscan import kidscan  # noqa: E402

# Starts as a JPEG does, and is nothing else.
STAND_IN_PICTURE = b"\xff\xd8\xff\xe0 a stand-in, not a picture\n"


def write_pictures(library, folder):
    """A stand-in <appid>.jpg for every app in the fixture's libraries."""
    shutil.rmtree(folder, ignore_errors=True)
    folder.mkdir(parents=True)
    for lib in (library.root, library.lib2):
        for manifest in (lib / "steamapps").glob("appmanifest_*.acf"):
            appid = manifest.name.removeprefix("appmanifest_").removesuffix(".acf")
            (folder / f"{appid}.jpg").write_bytes(STAND_IN_PICTURE)


def main(argv):
    if len(argv) not in (2, 3):
        print("usage: roundtrip_manifest.py OUTPUT [GAMES]", file=sys.stderr)
        return 2
    output = Path(argv[1])
    output.unlink(missing_ok=True)
    games = Path(argv[2]) if len(argv) == 3 else None
    if games is not None:
        games.unlink(missing_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        base = Path(tmp)
        library = test_kidscan.FakeLibrary(base)
        scummvm = test_kidscan.write_fake_scummvm(base / "scummvm")
        kidscan.STEAM_ROOTS = []  # never the real home directory
        code = kidscan.main([
            "--steam-root", str(library.root),
            "--scummvm", str(scummvm),
            "-o", str(output),
        ])
        if code != 0 or games is None:
            return code
        pictures = games.parent / f"{games.stem}-pictures"
        write_pictures(library, pictures)
        return kidscan.main([
            "--no-scummvm",
            "--library", str(library.root),
            "--library", str(library.lib2),
            "--pictures", str(pictures),
            "-o", str(games),
        ])


if __name__ == "__main__":
    sys.exit(main(sys.argv))
