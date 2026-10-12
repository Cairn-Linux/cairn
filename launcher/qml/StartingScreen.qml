// SPDX-License-Identifier: Apache-2.0
pragma ComponentBehavior: Bound
import QtQuick
import Cairn.Brand

// Shown from the moment a tile is pressed until the app's window is on the
// screen (ADR-0028). A Steam game can take seconds to come up, and tiles
// that look the same after a press read as broken. The mark builds itself,
// bottom stone first, so the screen is plainly still working, and one
// sentence says what is on its way.
Rectangle {
    id: screen

    // The tile that is starting, for the sentence that names it.
    required property string appTitle

    // Steps through one round of the building: a stone more each step until
    // the mark is whole, the whole mark for a step, no mark for a step.
    property int step: 0
    readonly property int stoneCount: Tokens.markStones.length
    readonly property int stonesUp: step < stoneCount ? step + 1 : step === stoneCount ? stoneCount : 0

    color: Tokens.ground

    Accessible.role: Accessible.ProgressBar
    Accessible.name: sentence.text

    // Every start begins at the bottom stone. Keys pressed while it starts
    // land here and do nothing: the launcher runs one program at a time.
    onVisibleChanged: {
        step = 0;
        if (visible)
            screen.forceActiveFocus();
    }

    Timer {
        interval: Tokens.motionStone
        repeat: true
        running: screen.visible
        onTriggered: screen.step = (screen.step + 1) % (screen.stoneCount + 2)
    }

    Column {
        anchors.centerIn: parent
        spacing: Tokens.headingSize
        width: parent.width - Tokens.headingSize * 2

        Item {
            id: mark

            // Pixels per unit of the mark's own box.
            readonly property real unit: Tokens.displaySize * 3 / Tokens.markHeight

            anchors.horizontalCenter: parent.horizontalCenter
            width: Tokens.markWidth * unit
            height: Tokens.markHeight * unit

            Repeater {
                model: screen.stoneCount

                Rectangle {
                    required property int index
                    readonly property var stone: Tokens.markStones[index]

                    x: stone[0] * mark.unit
                    y: stone[1] * mark.unit
                    width: stone[2] * mark.unit
                    height: stone[3] * mark.unit
                    radius: Tokens.markStoneRadius * mark.unit
                    // The mark is Ink on a light ground, as the text is.
                    color: Tokens.text
                    // The stones are listed top first; the bottom one goes up first.
                    opacity: screen.stoneCount - index <= screen.stonesUp ? 1 : 0

                    Behavior on opacity {
                        NumberAnimation {
                            duration: Tokens.motionStone
                        }
                    }
                }
            }
        }

        Text {
            id: sentence

            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: qsTr("%1 is starting.").arg(screen.appTitle)
            font.family: Tokens.fontFamily
            font.pixelSize: Tokens.headingSize
            font.weight: Tokens.weightBold
            color: Tokens.text
        }
    }
}
