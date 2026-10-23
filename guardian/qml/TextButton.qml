// SPDX-License-Identifier: Apache-2.0
import QtQuick
import Cairn.Brand

// A plain word that does one thing. The quiet kind is for the way out
// beside the main button. A disabled one is a flat pebble, so it does not
// look pressable.
Rectangle {
    id: button

    property alias text: label.text
    property bool quiet: false

    signal activated

    width: label.implicitWidth + Tokens.headingSize * 2
    height: Tokens.headingSize * 2
    radius: Tokens.radiusSm
    color: !button.enabled ? Tokens.pebble : button.quiet ? Tokens.card : Tokens.machine
    border.color: Tokens.gamesEdge
    border.width: button.quiet && button.enabled ? Tokens.strokeHairline : 0
    activeFocusOnTab: true

    Accessible.role: Accessible.Button
    Accessible.name: label.text
    Accessible.focusable: true
    Accessible.onPressAction: button.activated()

    Keys.onReturnPressed: button.activated()
    Keys.onEnterPressed: button.activated()
    Keys.onSpacePressed: button.activated()

    Rectangle {
        anchors.fill: parent
        anchors.margins: -Tokens.focusOffset - Tokens.focusWidth
        radius: button.radius + Tokens.focusOffset + Tokens.focusWidth
        color: Qt.alpha(Tokens.focus, 0)
        border.width: Tokens.focusWidth
        border.color: Tokens.focus
        visible: button.activeFocus
    }

    Text {
        id: label

        anchors.centerIn: parent
        font.family: Tokens.fontFamily
        font.pixelSize: Tokens.bodySize
        font.weight: Tokens.weightBold
        color: button.quiet || !button.enabled ? Tokens.paperLabel : Tokens.machineLabel
    }

    TapHandler {
        onTapped: {
            button.forceActiveFocus(Qt.MouseFocusReason);
            button.activated();
        }
    }
}
