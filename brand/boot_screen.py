# SPDX-License-Identifier: Apache-2.0
"""Draw the boot screen's Plymouth theme from tokens.json (ADR-0044).

build.py calls render_boot_screen() and writes what it returns into
brand/image/plymouth/. The stones and the password box are PNG files
drawn here, with the standard library only, so no SVG renderer is needed
in the repository or in the image build. The theme's script is the
hand-written brand/boot-screen.script under a block of numbers taken from
the tokens.
"""

import math
import struct
import zlib
from pathlib import Path

HERE = Path(__file__).resolve().parent
SCRIPT_SOURCE = HERE / "boot-screen.script"
THEME_DIRECTORY = "/usr/share/plymouth/themes/cairn"
OUTPUT_DIRECTORY = Path("image/plymouth")

# Plymouth's script module calls the refresh function this many times a
# second (FRAMES_PER_SECOND in its plugin.c). The script asks for it too.
REFRESH_RATE = 50

# Samples per pixel along each side when working out how much of a pixel a
# shape covers. 8 gives 64 levels of edge softness, plenty at these sizes.
SAMPLES = 8

PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def rgb(hex_color):
    value = hex_color.lstrip("#")
    return tuple(int(value[index:index + 2], 16) for index in (0, 2, 4))


def script_number(value):
    return format(value, ".6g")


# ---- PNG ----


def png_chunk(kind, data):
    body = kind + data
    return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body))


def encode_png(width, height, pixels):
    """An RGBA PNG from rows of (red, green, blue, alpha) tuples."""
    raw = bytearray()
    for row in pixels:
        raw.append(0)  # filter type 0: the row as it is
        for pixel in row:
            raw.extend(pixel)
    header = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    return (
        PNG_SIGNATURE
        + png_chunk(b"IHDR", header)
        + png_chunk(b"IDAT", zlib.compress(bytes(raw), 9))
        + png_chunk(b"IEND", b"")
    )


def png_image(data):
    """The header and the uncompressed rows of a PNG this module wrote.

    Two zlib builds can compress the same rows to different bytes, so
    build.py --check compares this rather than the file's bytes.
    """
    if not data.startswith(PNG_SIGNATURE):
        raise ValueError("not a PNG file")
    position = len(PNG_SIGNATURE)
    header = b""
    compressed = bytearray()
    while position < len(data):
        (length,) = struct.unpack(">I", data[position:position + 4])
        kind = data[position + 4:position + 8]
        body = data[position + 8:position + 8 + length]
        if kind == b"IHDR":
            header = body
        elif kind == b"IDAT":
            compressed.extend(body)
        position += 12 + length
    return header, zlib.decompress(bytes(compressed))


# ---- shapes ----


def rounded_rectangle_contains(box, x, y):
    left, top, right, bottom, radius = box
    if not (left <= x <= right and top <= y <= bottom):
        return False
    nearest_x = min(max(x, left + radius), right - radius)
    nearest_y = min(max(y, top + radius), bottom - radius)
    return (x - nearest_x) ** 2 + (y - nearest_y) ** 2 <= radius ** 2


def coverage(box, column, row):
    """The share of pixel (column, row) inside a rounded rectangle."""
    # The shape is convex, so a pixel whose four corners are in it is all
    # in it. Only the pixels on an edge need sampling.
    corners = ((column, row), (column + 1, row), (column, row + 1), (column + 1, row + 1))
    if all(rounded_rectangle_contains(box, x, y) for x, y in corners):
        return 1.0
    inside = 0
    for sample_row in range(SAMPLES):
        y = row + (sample_row + 0.5) / SAMPLES
        for sample_column in range(SAMPLES):
            x = column + (sample_column + 0.5) / SAMPLES
            if rounded_rectangle_contains(box, x, y):
                inside += 1
    return inside / (SAMPLES * SAMPLES)


def draw_shape(width, height, box, color):
    """One colour, with the shape's edge softened into transparency."""
    left, top, right, bottom, _ = box
    rows = [[(0, 0, 0, 0)] * width for _ in range(height)]
    for row in range(max(0, int(top)), min(height, math.ceil(bottom))):
        for column in range(max(0, int(left)), min(width, math.ceil(right))):
            share = coverage(box, column, row)
            if share > 0:
                rows[row][column] = (*color, round(share * 255))
    return rows


def draw_box_with_edge(width, height, radius, edge, fill, edge_color):
    """A filled rounded rectangle with an edge of `edge` pixels."""
    outer = (0, 0, width, height, radius)
    inner = (edge, edge, width - edge, height - edge, max(0, radius - edge))
    rows = []
    for row in range(height):
        pixels = []
        for column in range(width):
            outer_share = coverage(outer, column, row)
            inner_share = coverage(inner, column, row)
            if outer_share == 0:
                pixels.append((0, 0, 0, 0))
                continue
            edge_share = outer_share - inner_share
            mixed = tuple(
                round((inner_share * f + edge_share * e) / outer_share)
                for f, e in zip(fill, edge_color)
            )
            pixels.append((*mixed, round(outer_share * 255)))
        rows.append(pixels)
    return rows


# ---- the theme ----


def boot_sizes(tokens):
    """Every size on the boot screen, in pixels, from the tokens.

    The mark is the launcher's starting screen's: three display sizes
    tall (launcher/qml/StartingScreen.qml). Text, spacing and the
    password box are the login screen's (greeter/theme/PasswordRow.qml).
    """
    type_tokens = tokens["type"]
    heading = type_tokens["heading"]["size"]
    _, _, mark_width, mark_height = (int(part) for part in tokens["mark"]["viewBox"].split())
    unit = type_tokens["display"]["size"] * 3 / mark_height
    return {
        "unit": unit,
        "mark-width": math.ceil(mark_width * unit),
        "mark-height": math.ceil(mark_height * unit),
        "text": heading,
        "gap": heading,
        "entry-width": type_tokens["display"]["size"] * 8,
        "entry-height": heading * 2,
        "entry-radius": tokens["radius"]["sm"],
        "entry-edge": tokens["interaction"]["focus-width"],
        "bullet": heading // 2,
    }


def boot_colors(tokens):
    """The token colour name of each thing the boot screen draws.

    The ground is the login screen's dark ground, so the screen stays the
    same colour from the boot screen to the login screen. The stones and
    the words on it take that ground's label colour, as the mark's rule
    asks. The password box is a card wearing the focus ring, as the login
    screen's is, and the dots and letters in it take the card's label
    colour, because the ground's would vanish on the card.
    """
    semantic = tokens["semantic"]
    ground = semantic["dark-ground"]
    box = semantic["card"]
    return {
        "ground": ground,
        "on-ground": semantic["on"][ground],
        "box": box,
        "ring": semantic["focus"],
        "in-box": semantic["on"][box],
    }


def boot_rgb(tokens, role):
    return rgb(tokens["color"][boot_colors(tokens)[role]]["hex"])


def stone_images(tokens, sizes):
    """One PNG per stone, bottom stone first, each the whole mark's size.

    Every stone sits where it sits in the mark, so the script puts all
    four at one place and only changes how much each one shows.
    """
    color = boot_rgb(tokens, "on-ground")
    unit = sizes["unit"]
    radius = tokens["mark"]["rx"] * unit
    images = {}
    bottom_first = list(reversed(tokens["mark"]["stones"]))
    for number, (x, y, width, height) in enumerate(bottom_first, start=1):
        box = (x * unit, y * unit, (x + width) * unit, (y + height) * unit, radius)
        rows = draw_shape(sizes["mark-width"], sizes["mark-height"], box, color)
        images[f"stone-{number}.png"] = encode_png(
            sizes["mark-width"], sizes["mark-height"], rows
        )
    return images


def entry_images(tokens, sizes):
    width, height = sizes["entry-width"], sizes["entry-height"]
    entry = draw_box_with_edge(
        width,
        height,
        sizes["entry-radius"],
        sizes["entry-edge"],
        boot_rgb(tokens, "box"),
        # Typing always goes to this box, so it wears the focus ring, as
        # the login screen's box does while it has the keyboard.
        boot_rgb(tokens, "ring"),
    )
    side = sizes["bullet"]
    bullet = draw_shape(side, side, (0, 0, side, side, side / 2), boot_rgb(tokens, "in-box"))
    return {
        "entry.png": encode_png(width, height, entry),
        "bullet.png": encode_png(side, side, bullet),
    }


def color_lines(name, hex_color):
    red, green, blue = (channel / 255 for channel in rgb(hex_color))
    return [
        f"{name}.red = {script_number(red)};",
        f"{name}.green = {script_number(green)};",
        f"{name}.blue = {script_number(blue)};",
    ]


def render_script(tokens, sizes):
    colors = {role: tokens["color"][name]["hex"] for role, name in boot_colors(tokens).items()}
    family = tokens["font"]["family"]
    lines = [
        "# SPDX-License-Identifier: Apache-2.0",
        "# GENERATED by brand/build.py from brand/tokens.json and",
        "# brand/boot-screen.script. Edit those, not this file.",
        "",
        "# ---- from brand/tokens.json ----",
        *color_lines("ground", colors["ground"]),
        *color_lines("text", colors["on-ground"]),
        *color_lines("box_text", colors["in-box"]),
        f"stone_count = {len(tokens['mark']['stones'])};",
        f"refresh_rate = {REFRESH_RATE};",
        "# One stone every motion.stone milliseconds, each fading in over",
        "# the same time (ADR-0028).",
        f"frames_per_step = {round(tokens['motion']['stone'] * REFRESH_RATE / 1000)};",
        f"mark_width = {sizes['mark-width']};",
        f"mark_height = {sizes['mark-height']};",
        f"gap = {sizes['gap']};",
        f"bullet_size = {sizes['bullet']};",
        f'text_font = "{family} {sizes["text"]}px";',
        "",
    ]
    body = SCRIPT_SOURCE.read_text(encoding="utf-8")
    # The source's own licence line is already at the top.
    body_lines = body.splitlines()
    if body_lines and body_lines[0].startswith("# SPDX-License-Identifier:"):
        body_lines = body_lines[1:]
    return "\n".join(lines + body_lines) + "\n"


def render_theme_file():
    return f"""# SPDX-License-Identifier: Apache-2.0
# GENERATED by brand/build.py. Do not edit.
[Plymouth Theme]
Name=Cairn
Description=The Cairn mark's stones building, one at a time (ADR-0044).
ModuleName=script

[script]
ImageDir={THEME_DIRECTORY}
ScriptFile={THEME_DIRECTORY}/cairn.script
"""


def render_boot_screen(tokens):
    sizes = boot_sizes(tokens)
    files = {"cairn.plymouth": render_theme_file(), "cairn.script": render_script(tokens, sizes)}
    files.update(stone_images(tokens, sizes))
    files.update(entry_images(tokens, sizes))
    return {OUTPUT_DIRECTORY / name: contents for name, contents in files.items()}
