// SPDX-License-Identifier: Apache-2.0
import QtQuick
import Cairn.Brand

// What the last change did, or why nothing changed, in a box of its own.
// Hidden while there is nothing to say.
Rectangle {
    id: box

    property alias text: words.text

    height: words.implicitHeight + Tokens.bodySize * 2
    visible: words.text !== ""
    radius: Tokens.radiusSm
    color: Tokens.card
    border.color: Tokens.machine
    border.width: Tokens.strokeHairline

    Text {
        id: words

        objectName: box.objectName + "Text"
        x: Tokens.bodySize
        y: Tokens.bodySize
        width: parent.width - Tokens.bodySize * 2
        wrapMode: Text.WordWrap
        font.family: Tokens.fontFamily
        font.pixelSize: Tokens.bodySize
        lineHeight: Tokens.bodyLineHeight
        color: Tokens.paperLabel
        Accessible.role: Accessible.AlertMessage
        Accessible.name: text
    }
}
