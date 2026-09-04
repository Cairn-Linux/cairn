# SPDX-License-Identifier: Apache-2.0
"""Tests for the Cairn brand token generator."""

import importlib.util
import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPOSITORY = Path(__file__).resolve().parents[2]
BRAND_DIRECTORY = REPOSITORY / "brand"
BUILD_DIRECTORY = REPOSITORY / "build"

SPEC = importlib.util.spec_from_file_location(
    "cairn_brand_build", BRAND_DIRECTORY / "build.py"
)
brand_build = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(brand_build)


class BrandBuildTests(unittest.TestCase):
    def setUp(self):
        self.tokens = json.loads(
            (BRAND_DIRECTORY / "tokens.json").read_text(encoding="utf-8")
        )

    def run_copied_build(self, copied_brand, *arguments):
        return subprocess.run(
            [sys.executable, str(copied_brand / "build.py"), *arguments],
            cwd=copied_brand.parent,
            check=False,
            capture_output=True,
            text=True,
        )

    def temporary_brand_copy(self):
        BUILD_DIRECTORY.mkdir(exist_ok=True)
        temporary = tempfile.TemporaryDirectory(dir=BUILD_DIRECTORY)
        copied_brand = Path(temporary.name) / "brand"
        shutil.copytree(
            BRAND_DIRECTORY,
            copied_brand,
            ignore=shutil.ignore_patterns("__pycache__"),
        )
        return temporary, copied_brand

    def generated_bytes(self, copied_brand):
        outputs = brand_build.render_outputs(self.tokens)
        return {path: (copied_brand / path).read_bytes() for path in outputs}

    def test_reserved_qml_handler_name_is_refused(self):
        temporary, copied_brand = self.temporary_brand_copy()
        with temporary:
            tokens_path = copied_brand / "tokens.json"
            copied_tokens = json.loads(tokens_path.read_text(encoding="utf-8"))
            copied_tokens["color"]["on-danger"] = copied_tokens["color"]["ink"].copy()
            tokens_path.write_text(
                json.dumps(copied_tokens, indent=2) + "\n",
                encoding="utf-8",
            )

            refused = self.run_copied_build(copied_brand)

            self.assertNotEqual(refused.returncode, 0)
            self.assertEqual(
                refused.stderr.strip(),
                "refusing reserved QML property name: onDanger",
            )
            self.assertNotIn("Traceback", refused.stderr)

    def test_every_recorded_contrast_is_recomputed(self):
        self.assertEqual(brand_build.measured_contrast("#000000", "#FFFFFF"), 21.0)
        computed = brand_build.recomputed_contrasts(self.tokens)
        self.assertEqual(computed, self.tokens["contrast"])

    def test_rendered_qml_matches_tokens(self):
        lines = [
            line.split("//", 1)[0].strip()
            for line in brand_build.render_qml(self.tokens).splitlines()
        ]
        for name, color in self.tokens["color"].items():
            property_name = brand_build.camel(name)
            self.assertIn(
                f'readonly property color {property_name}: "{color["hex"]}"',
                lines,
            )
        for kind, palette in self.tokens["semantic"]["kind"].items():
            self.assertIn(
                f"readonly property color {kind}: {brand_build.camel(palette)}",
                lines,
            )
        for label, palette in self.tokens["semantic"]["on"].items():
            property_name = brand_build.camel(label) + "Label"
            self.assertIn(
                f"readonly property color {property_name}: "
                f"{brand_build.camel(palette)}",
                lines,
            )
        for name, value in self.tokens["radius"].items():
            property_name = brand_build.camel(f"radius-{name}")
            self.assertIn(
                f"readonly property int {property_name}: {value}", lines
            )
        family = self.tokens["font"]["family"]
        self.assertIn(f'readonly property string fontFamily: "{family}"', lines)

    def test_generation_is_byte_deterministic(self):
        temporary, copied_brand = self.temporary_brand_copy()
        with temporary:
            first_run = self.run_copied_build(copied_brand)
            self.assertEqual(first_run.returncode, 0, first_run.stderr)
            first_outputs = self.generated_bytes(copied_brand)

            second_run = self.run_copied_build(copied_brand)
            self.assertEqual(second_run.returncode, 0, second_run.stderr)
            second_outputs = self.generated_bytes(copied_brand)

            self.assertEqual(first_outputs, second_outputs)

    def test_check_passes_then_detects_an_altered_output(self):
        temporary, copied_brand = self.temporary_brand_copy()
        with temporary:
            clean = self.run_copied_build(copied_brand, "--check")
            self.assertEqual(clean.returncode, 0, clean.stderr)

            css_path = copied_brand / "tokens.css"
            css_path.write_text(
                css_path.read_text(encoding="utf-8") + "/* altered */\n",
                encoding="utf-8",
            )
            altered = self.run_copied_build(copied_brand, "--check")

            self.assertNotEqual(altered.returncode, 0)
            self.assertEqual(
                altered.stderr.strip(),
                "generated brand output differs: tokens.css",
            )
            self.assertNotIn("Traceback", altered.stderr)


if __name__ == "__main__":
    unittest.main()
