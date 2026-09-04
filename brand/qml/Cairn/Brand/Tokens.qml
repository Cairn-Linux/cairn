pragma Singleton
import QtQuick

// Cairn brand tokens — GENERATED from brand/tokens.json by brand/build.py. Do not edit.
// Import with `import Cairn.Brand` (QML import path: brand/qml) and use Tokens.ink etc.
QtObject {
    // ---- palette ----
    readonly property color ink: "#1C333C"  // Text, the mark, the only dark ground (terminal, login, boot)
    readonly property color fjord: "#2E6276"  // Accent, links, machine tiles
    readonly property color sky: "#A5C8D6"  // Tints, selection, focus ring, hint text on Ink
    readonly property color ochre: "#D9A03C"  // Make tiles (draw, music, build). Large fills only, never text
    readonly property color moss: "#7C956A"  // Practice tiles (GCompris, Tux). Large fills only, never text
    readonly property color sand: "#F4EDE0"  // Everyday ground
    readonly property color paper: "#FBF7EF"  // Cards on Sand
    readonly property color pebble: "#E8DFCC"  // Pill and chip fill on Sand (derived tint used in the guide)
    readonly property color line: "#E0D5BF"  // Hairline borders on Sand and Paper (derived tint used in the guide)

    // ---- semantic: use these in UI, not the palette names ----
    readonly property color ground: sand
    readonly property color card: paper
    readonly property color darkGround: ink
    readonly property color text: ink
    readonly property color link: fjord
    readonly property color selection: sky
    readonly property color focus: sky

    // kind — colour codes what a tile does, never which app it is
    readonly property color make: ochre
    readonly property color practice: moss
    readonly property color machine: fjord
    // label colour per kind; every pair clears 3:1 (WCAG AA large) at the 18px-bold minimum
    readonly property color onMake: ink  // 5.69:1
    readonly property color onPractice: ink  // 4.01:1 (Sand on Moss is 2.83:1 and fails)
    readonly property color onMachine: sand  // 5.77:1
    readonly property color onInk: sand  // 11.36:1

    // ---- type ----
    readonly property string fontFamily: "Atkinson Hyperlegible"
    readonly property string fontFamilyMono: "Atkinson Hyperlegible Mono"
    readonly property int weightRegular: Font.Normal
    readonly property int weightBold: Font.Bold
    readonly property int displaySize: 48
    readonly property real displayLineHeight: 1.1
    readonly property real displayLetterSpacing: -0.96  // px, from -0.02em
    readonly property int headingSize: 28
    readonly property real headingLineHeight: 1.2
    readonly property int bodySize: 17
    readonly property real bodyLineHeight: 1.65
    readonly property int terminalSize: 18
    readonly property real terminalLineHeight: 1.7
    readonly property int kickerSize: 13
    readonly property int minChild: 18      // smallest child-facing text
    readonly property int minGuardian: 16   // smallest Guardian/docs text

    // ---- radius ----
    readonly property int radiusSm: 10
    readonly property int radiusTile: 18
    readonly property int radiusCard: 20
    readonly property int radiusCover: 28
    readonly property int radiusTerminal: 14
    readonly property int radiusPill: 999
}
