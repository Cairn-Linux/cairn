# SPDX-License-Identifier: Apache-2.0
"""Tests for the Cairn brand token generator."""

import importlib.util
import json
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib
from pathlib import Path

REPOSITORY = Path(__file__).resolve().parents[2]
BRAND_DIRECTORY = REPOSITORY / "brand"
BUILD_DIRECTORY = REPOSITORY / "build"

# build.py imports boot_screen.py from its own folder, as a script would.
sys.path.insert(0, str(BRAND_DIRECTORY))
import boot_screen  # noqa: E402

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

    def image_svg(self, name):
        outputs = brand_build.render_image_files(self.tokens)
        return outputs[Path("image") / name]

    def view_box(self, svg):
        start = svg.index('viewBox="') + len('viewBox="')
        return [int(part) for part in svg[start:svg.index('"', start)].split()]

    def test_image_icons_are_square_with_clear_space(self):
        stones = self.tokens["mark"]["stones"]
        clear_space = stones[-1][3]
        for name in ("cairn-logo.svg", "cairn-logo-on-ink.svg"):
            left, top, width, height = self.view_box(self.image_svg(name))
            self.assertEqual(width, height, name)
            self.assertGreaterEqual(min(x for x, _, _, _ in stones) - left, clear_space)
            self.assertGreaterEqual(min(y for _, y, _, _ in stones) - top, clear_space)
            self.assertGreaterEqual(
                left + width - max(x + w for x, _, w, _ in stones), clear_space
            )
            self.assertGreaterEqual(
                top + height - max(y + h for _, y, _, h in stones), clear_space
            )

    def test_image_icons_use_the_mark_colours_and_licence(self):
        colors = brand_build.color_values(self.tokens)
        for name, color in (
            ("cairn-logo.svg", colors["ink"]),
            ("cairn-logo-on-ink.svg", colors["sand"]),
        ):
            svg = self.image_svg(name)
            self.assertTrue(
                svg.startswith("<!-- SPDX-License-Identifier: CC-BY-SA-4.0 -->\n")
            )
            self.assertIn(f'<g fill="{color}">', svg)
            self.assertEqual(svg.count("<rect "), len(self.tokens["mark"]["stones"]))

    def test_console_colour_is_the_console_token_in_decimal(self):
        console = self.tokens["semantic"]["console"]
        value = self.tokens["color"][console]["hex"].lstrip("#")
        red, green, blue = (int(value[i:i + 2], 16) for i in (0, 2, 4))
        self.assertEqual(
            self.image_svg("ansi-color"), f"0;38;2;{red};{green};{blue}\n"
        )

    def test_check_detects_an_altered_image_file(self):
        temporary, copied_brand = self.temporary_brand_copy()
        with temporary:
            icon_path = copied_brand / "image" / "cairn-logo-on-ink.svg"
            icon_path.write_text(
                icon_path.read_text(encoding="utf-8").replace("-11", "0"),
                encoding="utf-8",
            )
            altered = self.run_copied_build(copied_brand, "--check")

            self.assertNotEqual(altered.returncode, 0)
            self.assertEqual(
                altered.stderr.strip(),
                "generated brand output differs: image/cairn-logo-on-ink.svg",
            )


class BootScreenTests(unittest.TestCase):
    def setUp(self):
        self.tokens = json.loads(
            (BRAND_DIRECTORY / "tokens.json").read_text(encoding="utf-8")
        )
        self.files = boot_screen.render_boot_screen(self.tokens)
        self.sizes = boot_screen.boot_sizes(self.tokens)

    def file(self, name):
        return self.files[Path("image/plymouth") / name]

    def rgba(self, name):
        header, raw = boot_screen.png_image(self.file(name))
        width, height = int.from_bytes(header[0:4], "big"), int.from_bytes(header[4:8], "big")
        stride = 1 + width * 4
        rows = []
        for row in range(height):
            line = raw[row * stride:(row + 1) * stride]
            self.assertEqual(line[0], 0, "every row is stored unfiltered")
            rows.append([tuple(line[1 + i * 4:5 + i * 4]) for i in range(width)])
        return rows

    def token_rgb(self, name):
        return boot_screen.rgb(self.tokens["color"][name]["hex"])

    def script_rgb(self, name):
        """A colour the generated script sets, as the bytes Plymouth draws.

        Plymouth multiplies each share by 255 and cuts off the fraction.
        """
        script = self.file("cairn.script")
        channels = []
        for channel in ("red", "green", "blue"):
            prefix = f"{name}.{channel} = "
            line = next(line for line in script.splitlines() if line.startswith(prefix))
            channels.append(int(255 * float(line[len(prefix):].rstrip(";"))))
        return tuple(channels)

    @staticmethod
    def hex_color(rgb):
        return "#" + "".join(f"{channel:02X}" for channel in rgb[:3])

    def test_the_theme_is_a_script_theme_with_every_file(self):
        names = sorted(path.name for path in self.files)
        self.assertEqual(
            names,
            [
                "bullet.png",
                "cairn.plymouth",
                "cairn.script",
                "entry.png",
                "stone-1.png",
                "stone-2.png",
                "stone-3.png",
                "stone-4.png",
            ],
        )
        theme = self.file("cairn.plymouth")
        self.assertIn("ModuleName=script\n", theme)
        self.assertIn(
            "ScriptFile=/usr/share/plymouth/themes/cairn/cairn.script\n", theme
        )

    def stone_centre(self, number):
        x, y, width, height = list(reversed(self.tokens["mark"]["stones"]))[number - 1]
        unit = self.sizes["unit"]
        rows = self.rgba(f"stone-{number}.png")
        return rows[int((y + height / 2) * unit)][int((x + width / 2) * unit)]

    def test_one_stone_image_per_stone_bottom_first(self):
        stones = self.tokens["mark"]["stones"]
        semantic = self.tokens["semantic"]
        # The mark's rule: Sand on dark grounds, which is the dark ground's
        # label colour.
        on_dark_ground = self.token_rgb(semantic["on"][semantic["dark-ground"]])
        inked = []
        for number in range(1, len(stones) + 1):
            rows = self.rgba(f"stone-{number}.png")
            self.assertEqual(len(rows), self.sizes["mark-height"])
            self.assertEqual(len(rows[0]), self.sizes["mark-width"])
            self.assertEqual(self.stone_centre(number), (*on_dark_ground, 255), number)
            self.assertEqual(rows[0][0][3], 0, "outside the stone is clear")
            inked.append(sum(pixel[3] for row in rows for pixel in row))
        self.assertEqual(inked, sorted(inked, reverse=True), "widest stone first")

    def test_the_script_draws_the_login_screens_colours(self):
        # greeter/theme/Main.qml draws on Tokens.darkGround, so the screen
        # keeps one colour from the boot screen to the login screen, to the
        # last bit of blue.
        semantic = self.tokens["semantic"]
        self.assertEqual(self.script_rgb("ground"), self.token_rgb(semantic["dark-ground"]))
        self.assertEqual(
            self.script_rgb("text"), self.token_rgb(semantic["on"][semantic["dark-ground"]])
        )
        self.assertEqual(
            self.script_rgb("box_text"), self.token_rgb(semantic["on"][semantic["card"]])
        )

    def test_every_channel_comes_back_as_the_same_byte(self):
        # Plymouth makes a byte of a share three ways: the ground in double
        # precision, FreeType's text in single precision, and Pango's text
        # through cairo's 16-bit colour.
        for channel in range(256):
            share = float(boot_screen.script_share(channel))
            single = struct.unpack("f", struct.pack("f", share))[0]
            single_product = struct.unpack("f", struct.pack("f", 255 * single))[0]
            self.assertEqual(int(255 * share), channel)
            self.assertEqual(int(single_product), channel)
            self.assertEqual(int(share * 65535 + 0.5) >> 8, channel)

    def test_everything_drawn_stands_out_from_what_is_behind_it(self):
        ground = self.script_rgb("ground")
        box_rows = self.rgba("entry.png")
        middle = box_rows[len(box_rows) // 2]
        box = middle[len(middle) // 2]
        ring = middle[0]
        side = self.sizes["bullet"]
        dot = self.rgba("bullet.png")[side // 2][side // 2]
        # Words need 4.5:1, shapes and the focus ring 3:1 (WCAG 1.4.3 and
        # 1.4.11). The ring counts against the ground around the box.
        pairs = {
            "the stones on the ground": (self.stone_centre(1), ground, 3.0),
            "the words on the ground": (self.script_rgb("text"), ground, 4.5),
            "the box on the ground": (box, ground, 3.0),
            "the focus ring on the ground": (ring, ground, 3.0),
            "the dots in the box": (dot, box, 3.0),
            "an answer typed in the box": (self.script_rgb("box_text"), box, 4.5),
        }
        for what, (front, behind, minimum) in pairs.items():
            with self.subTest(what):
                ratio = brand_build.contrast_ratio(self.hex_color(front), self.hex_color(behind))
                self.assertGreaterEqual(ratio, minimum)

    def test_the_mark_is_the_launchers_size(self):
        display = self.tokens["type"]["display"]["size"]
        self.assertEqual(self.sizes["mark-height"], display * 3)

    def test_the_password_box_wears_the_focus_ring_on_a_card(self):
        rows = self.rgba("entry.png")
        colors = self.tokens["color"]
        semantic = self.tokens["semantic"]
        middle = rows[len(rows) // 2]
        self.assertEqual(middle[0][:3], boot_screen.rgb(colors[semantic["focus"]]["hex"]))
        self.assertEqual(
            middle[len(middle) // 2],
            (*boot_screen.rgb(colors[semantic["card"]]["hex"]), 255),
        )
        self.assertEqual(rows[0][0][3], 0, "the corner is rounded off")

    def test_script_numbers_come_from_the_tokens(self):
        script = self.file("cairn.script")
        self.assertTrue(script.startswith("# SPDX-License-Identifier: Apache-2.0\n"))
        self.assertEqual(script.count("SPDX-License-Identifier"), 1)
        stone = self.tokens["motion"]["stone"]
        self.assertIn(
            f"frames_per_step = {stone * boot_screen.REFRESH_RATE // 1000};", script
        )
        self.assertIn(f"stone_count = {len(self.tokens['mark']['stones'])};", script)
        family = self.tokens["font"]["family"]
        self.assertIn(f'text_font = "{family} {self.sizes["text"]}px";', script)
        self.assertIn("Plymouth.SetDisplayPasswordFunction(display_password);", script)

    def test_text_is_at_least_the_childs_minimum(self):
        self.assertGreaterEqual(self.sizes["text"], self.tokens["type"]["min-child"])

    def test_words_on_screen_are_plain(self):
        script = (BRAND_DIRECTORY / "boot-screen.script").read_text(encoding="utf-8")
        words = [
            line.split('"')[1]
            for line in script.splitlines()
            if line.endswith('";') and "_words = " in line
        ]
        # Plymouth cannot wrap a line. A longer sentence must be seen again
        # on the narrowest screen (ADR-0044) before it goes in here.
        self.assertEqual(
            words,
            [
                "A grown-up needs to type the password to start this computer.",
                "That password did not work. Type it again.",
                "Caps Lock is on.",
            ],
        )
        for sentence in words:
            self.assertNotIn("!", sentence)
            self.assertTrue(sentence.isascii(), "the initramfs has no UTF-8 locale")

    def test_png_survives_another_zlib(self):
        temporary, copied_brand = BrandBuildTests.temporary_brand_copy(self)
        with temporary:
            stone_path = copied_brand / "image" / "plymouth" / "stone-1.png"
            header, raw = boot_screen.png_image(stone_path.read_bytes())
            recompressed = (
                boot_screen.PNG_SIGNATURE
                + boot_screen.png_chunk(b"IHDR", header)
                + boot_screen.png_chunk(b"IDAT", zlib.compress(raw, 1))
                + boot_screen.png_chunk(b"IEND", b"")
            )
            self.assertNotEqual(recompressed, stone_path.read_bytes())
            stone_path.write_bytes(recompressed)
            same = BrandBuildTests.run_copied_build(self, copied_brand, "--check")
            self.assertEqual(same.returncode, 0, same.stderr)

            changed = bytearray(raw)
            changed[-1] ^= 0xFF
            stone_path.write_bytes(
                boot_screen.PNG_SIGNATURE
                + boot_screen.png_chunk(b"IHDR", header)
                + boot_screen.png_chunk(b"IDAT", zlib.compress(bytes(changed), 9))
                + boot_screen.png_chunk(b"IEND", b"")
            )
            altered = BrandBuildTests.run_copied_build(self, copied_brand, "--check")
            self.assertNotEqual(altered.returncode, 0)
            self.assertEqual(
                altered.stderr.strip(),
                "generated brand output differs: image/plymouth/stone-1.png",
            )


if __name__ == "__main__":
    unittest.main()
