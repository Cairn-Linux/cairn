// SPDX-License-Identifier: Apache-2.0
import QtQuick
import Cairn.Brand

// A level Cairn does not give yet, under the levels to pick: its name, the
// word "Planned" and a sentence on what it will be. It is words to read and
// nothing to press, so it stands on the page itself, with no card and no
// radio ring. The words keep their full colour: a faded or blurred
// sentence could not be read, and reading it is all this is for.
Item {
    id: planned

    required property int level
    required property string description

    height: words.implicitHeight
    // A Tab stop, though nothing here can be pressed: a screen reader
    // speaks what Tab lands on, so a parent who moves with Tab alone would
    // otherwise hear two levels and Add, and never that these exist.
    activeFocusOnTab: true

    // Text, not a radio button: a screen reader says the level is planned
    // and reads the sentence, and offers nothing to check or press.
    Accessible.role: Accessible.StaticText
    Accessible.name: qsTr("Level %1, planned").arg(planned.level)
    Accessible.description: planned.description
    Accessible.focusable: true

    Rectangle {
        objectName: "plannedFocusRing"
        anchors.fill: parent
        anchors.margins: -Tokens.focusOffset - Tokens.focusWidth
        radius: Tokens.radiusSm + Tokens.focusOffset + Tokens.focusWidth
        color: Qt.alpha(Tokens.focus, 0)
        border.width: Tokens.focusWidth
        border.color: Tokens.focus
        visible: planned.activeFocus
    }

    Column {
        id: words

        // Under the words of the levels above, past where their rings are,
        // so the four read as one list.
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: Tokens.bodySize * 3
        anchors.rightMargin: Tokens.bodySize

        Row {
            spacing: Tokens.bodySize / 2

            Text {
                id: name

                text: qsTr("Level %1").arg(planned.level)
                font.family: Tokens.fontFamily
                font.pixelSize: Tokens.bodySize
                font.weight: Tokens.weightBold
                lineHeight: Tokens.bodyLineHeight
                color: Tokens.sandLabel
                Accessible.ignored: true
            }

            // A flat pebble, as a button that cannot be pressed is. Its
            // word stands on the same line as the level's name.
            Rectangle {
                y: name.baselineOffset - word.y - word.baselineOffset
                width: word.implicitWidth + Tokens.bodySize
                height: word.implicitHeight + Tokens.strokeHairline * 2
                radius: Tokens.radiusPill
                color: Tokens.pebble

                Text {
                    id: word

                    objectName: "plannedWord"
                    anchors.centerIn: parent
                    text: qsTr("Planned")
                    font.family: Tokens.fontFamily
                    font.pixelSize: Tokens.bodySize
                    color: Tokens.text
                    Accessible.ignored: true
                }
            }
        }

        Text {
            objectName: "plannedDescription"
            width: parent.width
            wrapMode: Text.WordWrap
            text: planned.description
            font.family: Tokens.fontFamily
            font.pixelSize: Tokens.bodySize
            lineHeight: Tokens.bodyLineHeight
            color: Tokens.sandLabel
            Accessible.ignored: true
        }
    }
}
