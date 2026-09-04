#!/usr/bin/env python3
"""Generate brand outputs from tokens.json.

Writes, next to this file:
  tokens.css                       CSS custom properties (web surfaces, docs)
  qml/Cairn/Brand/Tokens.qml       QML singleton for Qt/QML surfaces
  qml/Cairn/Brand/qmldir           module file so `import Cairn.Brand` works
  mark.svg, mark-on-ink.svg, mark-currentcolor.svg

tokens.json is the source of truth. Edit it, run this, commit everything.
Standard library only.
"""

import json
from pathlib import Path

HERE = Path(__file__).resolve().parent


def luminance(hex_color):
    h = hex_color.lstrip("#")
    r, g, b = (int(h[i:i + 2], 16) / 255 for i in (0, 2, 4))

    def lin(c):
        return c / 12.92 if c <= 0.03928 else ((c + 0.055) / 1.055) ** 2.4

    return 0.2126 * lin(r) + 0.7152 * lin(g) + 0.0722 * lin(b)


def contrast(a, b):
    la, lb = luminance(a), luminance(b)
    return round((max(la, lb) + 0.05) / (min(la, lb) + 0.05), 2)


def camel(name):
    head, *tail = name.split("-")
    return head + "".join(part.capitalize() for part in tail)


def write_css(t, hexes, cr):
    css = f"""/* Cairn brand tokens — generated from brand/tokens.json; edit that file, not this one.
   Source: docs/brand-guide/Cairn Brand Guide.dc.html (v0.1). */

:root {{
  /* palette */
  --cairn-ink:    {hexes['ink']};
  --cairn-fjord:  {hexes['fjord']};
  --cairn-sky:    {hexes['sky']};
  --cairn-ochre:  {hexes['ochre']};
  --cairn-moss:   {hexes['moss']};
  --cairn-sand:   {hexes['sand']};
  --cairn-paper:  {hexes['paper']};
  --cairn-pebble: {hexes['pebble']};
  --cairn-line:   {hexes['line']};

  /* semantic */
  --cairn-ground:      var(--cairn-sand);
  --cairn-card:        var(--cairn-paper);
  --cairn-dark-ground: var(--cairn-ink);
  --cairn-text:        var(--cairn-ink);
  --cairn-link:        var(--cairn-fjord);
  --cairn-selection:   var(--cairn-sky);
  --cairn-focus:       var(--cairn-sky);

  /* kind — colour codes what a tile does, never which app it is */
  --cairn-make:        var(--cairn-ochre);
  --cairn-practice:    var(--cairn-moss);
  --cairn-machine:     var(--cairn-fjord);
  /* label colour per kind: chosen so every tile label clears 3:1 (WCAG AA large) at 18px bold */
  --cairn-on-make:     var(--cairn-ink);   /* {cr['ink-on-ochre']}:1 */
  --cairn-on-practice: var(--cairn-ink);   /* {cr['ink-on-moss']}:1 — Sand on Moss is {cr['sand-on-moss (rejected)']}:1 and fails */
  --cairn-on-machine:  var(--cairn-sand);  /* {cr['sand-on-fjord']}:1 */
  --cairn-on-ink:      var(--cairn-sand);  /* {cr['sand-on-ink']}:1 */

  /* type */
  --cairn-font:      "{t['font']['family']}", system-ui, sans-serif;
  --cairn-font-mono: "{t['font']['family-mono']}", ui-monospace, monospace;
  --cairn-display-size: {t['type']['display']['size']}px; --cairn-display-line: {t['type']['display']['line']}; --cairn-display-tracking: {t['type']['display']['tracking']};
  --cairn-heading-size: {t['type']['heading']['size']}px; --cairn-heading-line: {t['type']['heading']['line']};
  --cairn-body-size:    {t['type']['body']['size']}px; --cairn-body-line:    {t['type']['body']['line']};
  --cairn-terminal-size: {t['type']['terminal']['size']}px; --cairn-terminal-line: {t['type']['terminal']['line']};
  --cairn-kicker-size:  {t['type']['kicker']['size']}px; --cairn-kicker-tracking: {t['type']['kicker']['tracking']};
  --cairn-min-child: {t['type']['min-child']}px;
  --cairn-min-guardian: {t['type']['min-guardian']}px;

  /* radius */
  --cairn-radius-sm:       {t['radius']['sm']}px;
  --cairn-radius-tile:     {t['radius']['tile']}px;
  --cairn-radius-card:     {t['radius']['card']}px;
  --cairn-radius-cover:    {t['radius']['cover']}px;
  --cairn-radius-terminal: {t['radius']['terminal']}px;
  --cairn-radius-pill:     {t['radius']['pill']}px;
}}

/* Dark surfaces (terminal, login, boot): the only dark ground is Ink. */
.cairn-on-ink {{
  background: var(--cairn-ink);
  color: var(--cairn-sand);
  --cairn-text: var(--cairn-sand);
  --cairn-link: var(--cairn-sky);
}}

/* Interaction states are themed, never browser defaults. */
:focus-visible {{ outline: 3px solid var(--cairn-focus); outline-offset: 3px; }}
::selection {{ background: var(--cairn-selection); color: var(--cairn-ink); }}
"""
    (HERE / "tokens.css").write_text(css)


def write_qml(t, hexes, cr):
    sem = t["semantic"]
    ty = t["type"]
    lines = [
        "pragma Singleton",
        "import QtQuick",
        "",
        "// Cairn brand tokens — GENERATED from brand/tokens.json by brand/build.py. Do not edit.",
        "// Import with `import Cairn.Brand` (QML import path: brand/qml) and use Tokens.ink etc.",
        "QtObject {",
        "    // ---- palette ----",
    ]
    for name, spec in t["color"].items():
        lines.append(f'    readonly property color {camel(name)}: "{spec["hex"]}"  // {spec["role"]}')
    lines += [
        "",
        "    // ---- semantic: use these in UI, not the palette names ----",
        f"    readonly property color ground: {camel(sem['ground'])}",
        f"    readonly property color card: {camel(sem['card'])}",
        f"    readonly property color darkGround: {camel(sem['dark-ground'])}",
        f"    readonly property color text: {camel(sem['text'])}",
        f"    readonly property color link: {camel(sem['link'])}",
        f"    readonly property color selection: {camel(sem['selection'])}",
        f"    readonly property color focus: {camel(sem['focus'])}",
        "",
        "    // kind — colour codes what a tile does, never which app it is",
        f"    readonly property color make: {camel(sem['kind']['make'])}",
        f"    readonly property color practice: {camel(sem['kind']['practice'])}",
        f"    readonly property color machine: {camel(sem['kind']['machine'])}",
        "    // label colour per kind; every pair clears 3:1 (WCAG AA large) at the 18px-bold minimum",
        f"    readonly property color onMake: {camel(sem['on']['make'])}  // {cr['ink-on-ochre']}:1",
        f"    readonly property color onPractice: {camel(sem['on']['practice'])}  // {cr['ink-on-moss']}:1 (Sand on Moss is {cr['sand-on-moss (rejected)']}:1 and fails)",
        f"    readonly property color onMachine: {camel(sem['on']['machine'])}  // {cr['sand-on-fjord']}:1",
        f"    readonly property color onInk: {camel(sem['on']['ink'])}  // {cr['sand-on-ink']}:1",
        "",
        "    // ---- type ----",
        f'    readonly property string fontFamily: "{t["font"]["family"]}"',
        f'    readonly property string fontFamilyMono: "{t["font"]["family-mono"]}"',
        "    readonly property int weightRegular: Font.Normal",
        "    readonly property int weightBold: Font.Bold",
        f"    readonly property int displaySize: {ty['display']['size']}",
        f"    readonly property real displayLineHeight: {ty['display']['line']}",
        f"    readonly property real displayLetterSpacing: {ty['display']['size'] * float(ty['display']['tracking'].rstrip('em')):.2f}  // px, from {ty['display']['tracking']}",
        f"    readonly property int headingSize: {ty['heading']['size']}",
        f"    readonly property real headingLineHeight: {ty['heading']['line']}",
        f"    readonly property int bodySize: {ty['body']['size']}",
        f"    readonly property real bodyLineHeight: {ty['body']['line']}",
        f"    readonly property int terminalSize: {ty['terminal']['size']}",
        f"    readonly property real terminalLineHeight: {ty['terminal']['line']}",
        f"    readonly property int kickerSize: {ty['kicker']['size']}",
        f"    readonly property int minChild: {ty['min-child']}      // smallest child-facing text",
        f"    readonly property int minGuardian: {ty['min-guardian']}   // smallest Guardian/docs text",
        "",
        "    // ---- radius ----",
    ]
    for name, value in t["radius"].items():
        lines.append(f"    readonly property int radius{name.capitalize()}: {value}")
    lines += ["}", ""]
    out = HERE / "qml" / "Cairn" / "Brand"
    out.mkdir(parents=True, exist_ok=True)
    (out / "Tokens.qml").write_text("\n".join(lines))
    (out / "qmldir").write_text("module Cairn.Brand\nsingleton Tokens 1.0 Tokens.qml\n")


def write_marks(t, hexes):
    rects = "".join(
        f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{t["mark"]["rx"]}"/>'
        for x, y, w, h in t["mark"]["stones"]
    )

    def svg(fill):
        return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="{t["mark"]["viewBox"]}" '
                f'width="120" height="110" role="img" aria-label="Cairn"><g fill="{fill}">{rects}</g></svg>\n')

    (HERE / "mark.svg").write_text(svg(hexes["ink"]))
    (HERE / "mark-on-ink.svg").write_text(svg(hexes["sand"]))
    (HERE / "mark-currentcolor.svg").write_text(svg("currentColor"))


def main():
    t = json.loads((HERE / "tokens.json").read_text())
    hexes = {k: v["hex"] for k, v in t["color"].items()}
    cr = {
        "ink-on-ochre": contrast(hexes["ink"], hexes["ochre"]),
        "ink-on-moss": contrast(hexes["ink"], hexes["moss"]),
        "sand-on-moss (rejected)": contrast(hexes["sand"], hexes["moss"]),
        "sand-on-fjord": contrast(hexes["sand"], hexes["fjord"]),
        "sand-on-ink": contrast(hexes["sand"], hexes["ink"]),
    }
    write_css(t, hexes, cr)
    write_qml(t, hexes, cr)
    write_marks(t, hexes)
    print("wrote tokens.css, qml/Cairn/Brand/Tokens.qml, qmldir, 3 SVGs")


if __name__ == "__main__":
    main()
