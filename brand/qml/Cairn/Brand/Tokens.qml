// SPDX-License-Identifier: Apache-2.0
// GENERATED from brand/tokens.json by brand/build.py. Do not edit.
pragma Singleton
import QtQuick

// Import with `import Cairn.Brand` (QML import path: brand/qml).
QtObject {
    // ---- palette ----
    readonly property color ink: "#1C333C"  // Text, the mark, the only dark ground (terminal, login, boot)
    readonly property color fjord: "#2E6276"  // Accent, links, machine tiles
    readonly property color sky: "#A5C8D6"  // Tints, selection, focus ring, hint text on Ink
    readonly property color ochre: "#D9A03C"  // Make tiles (draw, music, build). Large fills only, never text
    readonly property color moss: "#7C956A"  // Practice tiles (GCompris, Tux). Large fills only, never text
    readonly property color sand: "#F4EDE0"  // Everyday ground
    readonly property color paper: "#FBF7EF"  // Cards on Sand, and games tiles (ADR-0024)
    readonly property color pebble: "#E8DFCC"  // Pill and chip fill on Sand (derived tint used in the guide)
    readonly property color line: "#E0D5BF"  // Hairline borders on Sand and Paper, and the edge of a games tile (derived tint used in the guide)

    // ---- semantic: use these in UI, not the palette names ----
    readonly property color ground: sand
    readonly property color card: paper
    readonly property color darkGround: ink
    readonly property color text: ink
    readonly property color link: fjord
    readonly property color linkOnInk: sky
    readonly property color selection: sky
    readonly property color focus: sky
    readonly property int focusWidth: 3
    readonly property int focusOffset: 3

    // kind — colour codes what a tile does, never which app it is
    readonly property color make: ochre
    readonly property color practice: moss
    readonly property color games: paper
    readonly property color machine: fjord
    readonly property color gamesEdge: line  // a games tile is a quiet card, so it carries a hairline edge
    readonly property int strokeHairline: 2
    // Label pair comments are measured contrast ratios against each ground.
    readonly property color makeLabel: ink  // 5.69:1
    readonly property color practiceLabel: ink  // 4.01:1
    readonly property color gamesLabel: ink  // 12.37:1
    readonly property color machineLabel: sand  // 5.77:1
    readonly property color inkLabel: sand  // 11.36:1
    readonly property color sandLabel: ink  // 11.36:1
    readonly property color paperLabel: ink  // 12.37:1

    // ---- type ----
    readonly property string fontFamily: "Atkinson Hyperlegible Next"
    readonly property string fontFamilyMono: "Atkinson Hyperlegible Mono"
    readonly property int weightRegular: 400
    readonly property int weightBold: 700
    property real textScale: 1.0
    readonly property real displaySize: 48 * textScale
    readonly property real displayLineHeight: 1.1
    readonly property real displayLetterSpacing: displaySize * -0.02
    readonly property real headingSize: 28 * textScale
    readonly property real headingLineHeight: 1.2
    readonly property real bodySize: 17 * textScale
    readonly property real bodyLineHeight: 1.65
    readonly property real terminalSize: 18 * textScale
    readonly property real terminalLineHeight: 1.7
    readonly property real kickerSize: 13 * textScale
    readonly property real kickerLetterSpacing: kickerSize * 0.14
    readonly property real minChild: 18 * textScale  // smallest child-facing text
    readonly property real minGuardian: 16 * textScale  // smallest Guardian/docs text

    // ---- radius ----
    readonly property int radiusSm: 10
    readonly property int radiusTile: 18
    readonly property int radiusCard: 20
    readonly property int radiusCover: 28
    readonly property int radiusTerminal: 14
    readonly property int radiusPill: 999

    // ---- motion (milliseconds) ----
    // Milliseconds. Motion only says something true (ADR-0028): rows of tiles
    // sliding into view, the mark building one stone at a time while a tile
    // starts. Never to decorate, distract, reward, or hold a child's
    // attention; slow and small, and nothing flashes.
    readonly property int motionRow: 160
    readonly property int motionStone: 500

    // ---- mark: the stones in the units of its viewBox, top stone first ----
    readonly property int markWidth: 120
    readonly property int markHeight: 110
    readonly property int markStoneRadius: 8
    readonly property var markStones: [[54, 8, 24, 16], [34, 30, 44, 16], [30, 52, 64, 16], [16, 74, 88, 16]]  // [x, y, width, height]
}
