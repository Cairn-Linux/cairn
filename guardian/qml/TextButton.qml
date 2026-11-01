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

    // A held key repeats. Only the first press counts, so holding Enter or
    // Space presses the button once.
    function keyPressed(event) {
        if (!event.isAutoRepeat)
            button.activated();
    }

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

    Keys.onReturnPressed: event => button.keyPressed(event)
    Keys.onEnterPressed: event => button.keyPressed(event)
    Keys.onSpacePressed: event => button.keyPressed(event)

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

    // The press is the button's alone, so nothing under it, such as the
    // backdrop of a box that must be answered first, takes the tap away.
    TapHandler {
        gesturePolicy: TapHandler.ReleaseWithinBounds
        onTapped: {
            button.forceActiveFocus(Qt.MouseFocusReason);
            button.activated();
        }
    }
}
