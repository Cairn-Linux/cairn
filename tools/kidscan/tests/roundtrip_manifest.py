#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Writes the manifest kidscan makes from the synthetic Steam library in
test_kidscan.py, with the stand-in scummvm, for the launcher to read (#97).

CTest runs this before the launcher's tst_kidscanmanifest, which reads the
file. Standard library only.

Run:  python3 tools/kidscan/tests/roundtrip_manifest.py OUTPUT
"""

import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_kidscan  # noqa: E402  (FakeLibrary and the stand-in scummvm)
from test_kidscan import kidscan  # noqa: E402


def main(argv):
    if len(argv) != 2:
        print("usage: roundtrip_manifest.py OUTPUT", file=sys.stderr)
        return 2
    output = Path(argv[1])
    output.unlink(missing_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        base = Path(tmp)
        library = test_kidscan.FakeLibrary(base)
        scummvm = test_kidscan.write_fake_scummvm(base / "scummvm")
        kidscan.STEAM_ROOTS = []  # never the real home directory
        return kidscan.main([
            "--steam-root", str(library.root),
            "--scummvm", str(scummvm),
            "-o", str(output),
        ])


if __name__ == "__main__":
    sys.exit(main(sys.argv))
