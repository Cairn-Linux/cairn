# SPDX-License-Identifier: Apache-2.0
"""Generate and validate brand outputs from tokens.json.

Run without arguments to write the generated files. Run with ``--check`` to
verify that recorded contrast figures and generated files are current.
Standard library only.
"""

import argparse
import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
MINIMUM_LABEL_CONTRAST = 3.0
QML_HANDLER_NAME = re.compile(r"^on[A-Z]")
QML_PROPERTY = re.compile(
    r"^\s*(?:readonly\s+)?property\s+\S+\s+(\w+)\s*:", re.MULTILINE
)


class BuildError(Exception):
    """A brand source or generated output failed validation."""


def luminance(hex_color):
    value = hex_color.lstrip("#")
    red, green, blue = (
        int(value[index:index + 2], 16) / 255 for index in (0, 2, 4)
    )

    def linear(channel):
        if channel <= 0.04045:
            return channel / 12.92
        return ((channel + 0.055) / 1.055) ** 2.4

    return (
        0.2126 * linear(red)
        + 0.7152 * linear(green)
        + 0.0722 * linear(blue)
    )


def contrast_ratio(first, second):
    first_luminance = luminance(first)
    second_luminance = luminance(second)
    lighter = max(first_luminance, second_luminance)
    darker = min(first_luminance, second_luminance)
    return (lighter + 0.05) / (darker + 0.05)


def measured_contrast(first, second):
    return round(contrast_ratio(first, second), 2)


def camel(name):
    head, *tail = name.split("-")
    return head + "".join(part.capitalize() for part in tail)


def qml_number(value):
    return format(value, ".12g")


def em_value(value):
    if not value.endswith("em"):
        raise BuildError(f"tracking value must use em: {value}")
    return float(value[:-2])


def qml_property(kind, name, value, comment=None, writable=False):
    if QML_HANDLER_NAME.match(name):
        raise BuildError(f"refusing reserved QML property name: {name}")
    prefix = "property" if writable else "readonly property"
    line = f"    {prefix} {kind} {name}: {value}"
    if comment:
        line += f"  // {comment}"
    return line


def validate_qml_property_names(qml):
    for name in QML_PROPERTY.findall(qml):
        if QML_HANDLER_NAME.match(name):
            raise BuildError(f"refusing reserved QML property name: {name}")


def color_values(tokens):
    return {name: spec["hex"] for name, spec in tokens["color"].items()}


def contrast_pair_colors(tokens, pair_name):
    name = pair_name.split(" (", 1)[0]
    foreground, separator, ground = name.partition("-on-")
    colors = color_values(tokens)
    if not separator or foreground not in colors or ground not in colors:
        raise BuildError(f"invalid contrast pair: {pair_name}")
    return colors[foreground], colors[ground]


def recomputed_contrasts(tokens):
    figures = {}
    for pair_name in tokens["contrast"]:
        foreground, ground = contrast_pair_colors(tokens, pair_name)
        figures[pair_name] = measured_contrast(foreground, ground)
    return figures


def validate_recorded_contrasts(tokens):
    computed = recomputed_contrasts(tokens)
    for pair_name, recorded in tokens["contrast"].items():
        if computed[pair_name] != recorded:
            raise BuildError(
                f"contrast figure differs for {pair_name}: "
                f"recorded {recorded}, measured {computed[pair_name]}"
            )


def label_ground(tokens, label_name):
    return tokens["semantic"]["kind"].get(label_name, label_name)


def label_contrast(tokens, label_name):
    colors = color_values(tokens)
    foreground_name = tokens["semantic"]["on"][label_name]
    ground_name = label_ground(tokens, label_name)
    return contrast_ratio(colors[foreground_name], colors[ground_name])


def validate_label_contrasts(tokens):
    for label_name in tokens["semantic"]["on"]:
        ratio = label_contrast(tokens, label_name)
        if ratio < MINIMUM_LABEL_CONTRAST:
            foreground = tokens["semantic"]["on"][label_name]
            ground = label_ground(tokens, label_name)
            raise BuildError(
                f"label contrast is below 3:1: {foreground}-on-{ground} "
                f"measures {ratio:.2f}:1"
            )


def label_ratio_comment(tokens, label_name):
    return f"{label_contrast(tokens, label_name):.2f}:1"


def render_css(tokens):
    colors = color_values(tokens)
    semantic = tokens["semantic"]
    type_tokens = tokens["type"]
    interaction = tokens["interaction"]
    weights = tokens["font"]["weights"]
    return f"""/* Cairn brand tokens — generated from brand/tokens.json; edit that file, not this one.
   Source: docs/brand-guide/Cairn Brand Guide.dc.html (v0.1). */

:root {{
  /* palette */
  --cairn-ink:    {colors['ink']};
  --cairn-fjord:  {colors['fjord']};
  --cairn-sky:    {colors['sky']};
  --cairn-ochre:  {colors['ochre']};
  --cairn-moss:   {colors['moss']};
  --cairn-sand:   {colors['sand']};
  --cairn-paper:  {colors['paper']};
  --cairn-pebble: {colors['pebble']};
  --cairn-line:   {colors['line']};

  /* semantic */
  --cairn-ground:       var(--cairn-{semantic['ground']});
  --cairn-card:         var(--cairn-{semantic['card']});
  --cairn-dark-ground:  var(--cairn-{semantic['dark-ground']});
  --cairn-text:         var(--cairn-{semantic['text']});
  --cairn-link:         var(--cairn-{semantic['link']});
  --cairn-link-on-ink:  var(--cairn-{semantic['link-on-ink']});
  --cairn-selection:    var(--cairn-{semantic['selection']});
  --cairn-focus:        var(--cairn-{semantic['focus']});
  --cairn-focus-width:  {interaction['focus-width']}px;
  --cairn-focus-offset: {interaction['focus-offset']}px;

  /* kind — colour codes what a tile does, never which app it is */
  --cairn-make:        var(--cairn-{semantic['kind']['make']});
  --cairn-practice:    var(--cairn-{semantic['kind']['practice']});
  --cairn-machine:     var(--cairn-{semantic['kind']['machine']});
  /* label colour pairs and their measured contrast ratios */
  --cairn-on-make:     var(--cairn-{semantic['on']['make']});   /* {label_ratio_comment(tokens, 'make')} */
  --cairn-on-practice: var(--cairn-{semantic['on']['practice']});   /* {label_ratio_comment(tokens, 'practice')} */
  --cairn-on-machine:  var(--cairn-{semantic['on']['machine']});  /* {label_ratio_comment(tokens, 'machine')} */
  --cairn-on-ink:      var(--cairn-{semantic['on']['ink']});  /* {label_ratio_comment(tokens, 'ink')} */
  --cairn-on-sand:     var(--cairn-{semantic['on']['sand']});   /* {label_ratio_comment(tokens, 'sand')} */
  --cairn-on-paper:    var(--cairn-{semantic['on']['paper']});   /* {label_ratio_comment(tokens, 'paper')} */

  /* type */
  --cairn-font:      "{tokens['font']['family']}", system-ui, sans-serif;
  --cairn-font-mono: "{tokens['font']['family-mono']}", ui-monospace, monospace;
  --cairn-weight-regular: {weights['regular']};
  --cairn-weight-bold:    {weights['bold']};
  --cairn-display-size: {type_tokens['display']['size']}px; --cairn-display-line: {type_tokens['display']['line']}; --cairn-display-tracking: {type_tokens['display']['tracking']};
  --cairn-heading-size: {type_tokens['heading']['size']}px; --cairn-heading-line: {type_tokens['heading']['line']};
  --cairn-body-size:    {type_tokens['body']['size']}px; --cairn-body-line:    {type_tokens['body']['line']};
  --cairn-terminal-size: {type_tokens['terminal']['size']}px; --cairn-terminal-line: {type_tokens['terminal']['line']};
  --cairn-kicker-size:  {type_tokens['kicker']['size']}px; --cairn-kicker-tracking: {type_tokens['kicker']['tracking']};
  --cairn-min-child: {type_tokens['min-child']}px;
  --cairn-min-guardian: {type_tokens['min-guardian']}px;

  /* radius */
  --cairn-radius-sm:       {tokens['radius']['sm']}px;
  --cairn-radius-tile:     {tokens['radius']['tile']}px;
  --cairn-radius-card:     {tokens['radius']['card']}px;
  --cairn-radius-cover:    {tokens['radius']['cover']}px;
  --cairn-radius-terminal: {tokens['radius']['terminal']}px;
  --cairn-radius-pill:     {tokens['radius']['pill']}px;
}}

/* Dark surfaces (terminal, login, boot): the only dark ground is Ink. */
.cairn-on-ink {{
  background: var(--cairn-ink);
  color: var(--cairn-on-ink);
  --cairn-text: var(--cairn-on-ink);
  --cairn-link: var(--cairn-link-on-ink);
}}

/* Interaction states are themed, never browser defaults. */
:focus-visible {{ outline: var(--cairn-focus-width) solid var(--cairn-focus); outline-offset: var(--cairn-focus-offset); }}
::selection {{ background: var(--cairn-selection); color: var(--cairn-on-sand); }}
"""


def render_qml(tokens):
    semantic = tokens["semantic"]
    type_tokens = tokens["type"]
    interaction = tokens["interaction"]
    weights = tokens["font"]["weights"]
    display_tracking = qml_number(em_value(type_tokens["display"]["tracking"]))
    kicker_tracking = qml_number(em_value(type_tokens["kicker"]["tracking"]))
    lines = [
        "// SPDX-License-Identifier: Apache-2.0",
        "// GENERATED from brand/tokens.json by brand/build.py. Do not edit.",
        "pragma Singleton",
        "import QtQuick",
        "",
        "// Import with `import Cairn.Brand` (QML import path: brand/qml).",
        "QtObject {",
        "    // ---- palette ----",
    ]
    for name, spec in tokens["color"].items():
        lines.append(
            qml_property("color", camel(name), json.dumps(spec["hex"]), spec["role"])
        )
    lines += [
        "",
        "    // ---- semantic: use these in UI, not the palette names ----",
        qml_property("color", "ground", camel(semantic["ground"])),
        qml_property("color", "card", camel(semantic["card"])),
        qml_property("color", "darkGround", camel(semantic["dark-ground"])),
        qml_property("color", "text", camel(semantic["text"])),
        qml_property("color", "link", camel(semantic["link"])),
        qml_property("color", "linkOnInk", camel(semantic["link-on-ink"])),
        qml_property("color", "selection", camel(semantic["selection"])),
        qml_property("color", "focus", camel(semantic["focus"])),
        qml_property("int", "focusWidth", str(interaction["focus-width"])),
        qml_property("int", "focusOffset", str(interaction["focus-offset"])),
        "",
        "    // kind — colour codes what a tile does, never which app it is",
        qml_property("color", "make", camel(semantic["kind"]["make"])),
        qml_property("color", "practice", camel(semantic["kind"]["practice"])),
        qml_property("color", "machine", camel(semantic["kind"]["machine"])),
        "    // Label pair comments are measured contrast ratios against each ground.",
    ]
    for label_name, color_name in semantic["on"].items():
        lines.append(
            qml_property(
                "color",
                f"{camel(label_name)}Label",
                camel(color_name),
                label_ratio_comment(tokens, label_name),
            )
        )
    lines += [
        "",
        "    // ---- type ----",
        qml_property("string", "fontFamily", json.dumps(tokens["font"]["family"])),
        qml_property(
            "string", "fontFamilyMono", json.dumps(tokens["font"]["family-mono"])
        ),
        qml_property("int", "weightRegular", str(weights["regular"])),
        qml_property("int", "weightBold", str(weights["bold"])),
        qml_property("real", "textScale", "1.0", writable=True),
        qml_property(
            "real", "displaySize", f"{type_tokens['display']['size']} * textScale"
        ),
        qml_property(
            "real", "displayLineHeight", str(type_tokens["display"]["line"])
        ),
        qml_property(
            "real", "displayLetterSpacing", f"displaySize * {display_tracking}"
        ),
        qml_property(
            "real", "headingSize", f"{type_tokens['heading']['size']} * textScale"
        ),
        qml_property(
            "real", "headingLineHeight", str(type_tokens["heading"]["line"])
        ),
        qml_property("real", "bodySize", f"{type_tokens['body']['size']} * textScale"),
        qml_property(
            "real", "bodyLineHeight", str(type_tokens["body"]["line"])
        ),
        qml_property(
            "real", "terminalSize", f"{type_tokens['terminal']['size']} * textScale"
        ),
        qml_property(
            "real", "terminalLineHeight", str(type_tokens["terminal"]["line"])
        ),
        qml_property(
            "real", "kickerSize", f"{type_tokens['kicker']['size']} * textScale"
        ),
        qml_property(
            "real", "kickerLetterSpacing", f"kickerSize * {kicker_tracking}"
        ),
        qml_property(
            "real",
            "minChild",
            f"{type_tokens['min-child']} * textScale",
            "smallest child-facing text",
        ),
        qml_property(
            "real",
            "minGuardian",
            f"{type_tokens['min-guardian']} * textScale",
            "smallest Guardian/docs text",
        ),
        "",
        "    // ---- radius ----",
    ]
    for name, value in tokens["radius"].items():
        property_name = "radius" + camel(name)[0].upper() + camel(name)[1:]
        lines.append(qml_property("int", property_name, str(value)))
    lines += ["}", ""]
    qml = "\n".join(lines)
    validate_qml_property_names(qml)
    return qml


def render_qmldir():
    return """# SPDX-License-Identifier: Apache-2.0
# GENERATED by brand/build.py. Do not edit.
module Cairn.Brand
singleton Tokens 1.0 Tokens.qml
"""


def render_cmake():
    return """# SPDX-License-Identifier: Apache-2.0
# GENERATED by brand/build.py. Do not edit.

set_source_files_properties(Tokens.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(CairnBrand
    URI Cairn.Brand
    VERSION 1.0
    QML_FILES
        Tokens.qml
)
"""


def render_marks(tokens):
    stones = "".join(
        f'<rect x="{x}" y="{y}" width="{width}" height="{height}" rx="{tokens["mark"]["rx"]}"/>'
        for x, y, width, height in tokens["mark"]["stones"]
    )

    def svg(fill):
        return (
            f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="{tokens["mark"]["viewBox"]}" '
            f'width="120" height="110" role="img" aria-label="Cairn"><g fill="{fill}">'
            f"{stones}</g></svg>\n"
        )

    colors = color_values(tokens)
    return {
        Path("mark.svg"): svg(colors["ink"]),
        Path("mark-on-ink.svg"): svg(colors["sand"]),
        Path("mark-currentcolor.svg"): svg("currentColor"),
    }


def render_outputs(tokens):
    outputs = {
        Path("tokens.css"): render_css(tokens),
        Path("qml/Cairn/Brand/Tokens.qml"): render_qml(tokens),
        Path("qml/Cairn/Brand/qmldir"): render_qmldir(),
        Path("qml/Cairn/Brand/CMakeLists.txt"): render_cmake(),
    }
    outputs.update(render_marks(tokens))
    return outputs


def write_outputs(outputs):
    for relative_path, contents in outputs.items():
        destination = HERE / relative_path
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_text(contents, encoding="utf-8", newline="\n")


def check_outputs(outputs):
    for relative_path, contents in outputs.items():
        destination = HERE / relative_path
        if (
            not destination.exists()
            or destination.read_bytes() != contents.encode("utf-8")
        ):
            raise BuildError(f"generated brand output differs: {relative_path}")


def load_tokens():
    return json.loads((HERE / "tokens.json").read_text(encoding="utf-8"))


def validate_tokens(tokens):
    validate_recorded_contrasts(tokens)
    validate_label_contrasts(tokens)


def parse_arguments(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--check",
        action="store_true",
        help="validate tokens and fail if generated files differ",
    )
    return parser.parse_args(argv)


def main(argv=None):
    arguments = parse_arguments(argv)
    try:
        tokens = load_tokens()
        validate_tokens(tokens)
        outputs = render_outputs(tokens)
        if arguments.check:
            check_outputs(outputs)
            print("brand tokens and generated outputs are current")
        else:
            write_outputs(outputs)
            print("wrote CSS, QML module files, and 3 SVGs")
    except BuildError as error:
        print(error, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
