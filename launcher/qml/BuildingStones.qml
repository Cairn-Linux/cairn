// SPDX-License-Identifier: Apache-2.0
pragma ComponentBehavior: Bound
import QtQuick
import Cairn.Brand

// The mark building itself (ADR-0028), at any size, from a step its owner
// counts: a stone more each step, bottom first, until the mark is whole,
// then the whole mark for a step and no mark for a step. Every change is a
// fade over one stone's time. The starting screen draws it, and so does a
// Steam game's tile while Steam updates itself (ADR-0061), so the two never
// differ.
Item {
    id: stones

    // Pixels per unit of the mark's own box.
    required property real unit
    // Counted up by a clock, one a stone's time. Any step past a round goes
    // round again; step -1 is the step with no mark, before the bottom stone.
    property int step: 0

    readonly property int stoneCount: Tokens.markStones.length
    readonly property int roundSteps: stoneCount + 2
    readonly property int roundStep: ((step % roundSteps) + roundSteps) % roundSteps
    readonly property int stonesUp: roundStep < stoneCount ? roundStep + 1 : roundStep === stoneCount ? stoneCount : 0

    // The box the stones take up inside the mark's own, in its units.
    readonly property real stonesLeft: Math.min(...Tokens.markStones.map(stone => stone[0]))
    readonly property real stonesTop: Math.min(...Tokens.markStones.map(stone => stone[1]))
    readonly property real stonesRight: Math.max(...Tokens.markStones.map(stone => stone[0] + stone[2]))
    readonly property real stonesBottom: Math.max(...Tokens.markStones.map(stone => stone[1] + stone[3]))
    // The mark's clear space: one base-stone height on every side.
    readonly property real clearSpace: Tokens.markStones[stoneCount - 1][3]

    width: Tokens.markWidth * unit
    height: Tokens.markHeight * unit
    Accessible.ignored: true

    Repeater {
        model: stones.stoneCount

        Rectangle {
            required property int index
            readonly property var stone: Tokens.markStones[index]

            x: stone[0] * stones.unit
            y: stone[1] * stones.unit
            width: stone[2] * stones.unit
            height: stone[3] * stones.unit
            radius: Tokens.markStoneRadius * stones.unit
            // The mark is Ink on a light ground, as the text is.
            color: Tokens.text
            // The stones are listed top first; the bottom one goes up first.
            opacity: stones.stoneCount - index <= stones.stonesUp ? 1 : 0

            Behavior on opacity {
                NumberAnimation {
                    duration: Tokens.motionStone
                }
            }
        }
    }
}
