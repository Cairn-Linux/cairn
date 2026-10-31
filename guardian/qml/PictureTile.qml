// SPDX-License-Identifier: Apache-2.0
import QtQuick
import Cairn.Brand

// One of Plasma's own pictures to choose as the grown-up's picture.
Rectangle {
    id: tile

    required property url source
    required property string name

    signal chosen

    // A held key repeats. Only the first press counts, so holding Enter or
    // Space chooses the picture once.
    function keyPressed(event) {
        if (!event.isAutoRepeat)
            tile.chosen();
    }

    width: Tokens.headingSize * 4
    height: width
    radius: Tokens.radiusSm
    color: Tokens.card
    border.color: tile.activeFocus ? Tokens.focus : Tokens.line
    border.width: tile.activeFocus ? Tokens.focusWidth : Tokens.strokeHairline
    activeFocusOnTab: true

    Accessible.role: Accessible.Button
    Accessible.name: qsTr("Use the picture %1").arg(tile.name)
    Accessible.focusable: true
    Accessible.onPressAction: tile.chosen()

    Keys.onReturnPressed: event => tile.keyPressed(event)
    Keys.onEnterPressed: event => tile.keyPressed(event)
    Keys.onSpacePressed: event => tile.keyPressed(event)

    Image {
        anchors.fill: parent
        anchors.margins: Tokens.focusWidth * 2
        source: tile.source
        sourceSize.width: width
        sourceSize.height: height
        fillMode: Image.PreserveAspectFit
        asynchronous: true
        Accessible.ignored: true
    }

    TapHandler {
        gesturePolicy: TapHandler.ReleaseWithinBounds
        onTapped: {
            tile.forceActiveFocus(Qt.MouseFocusReason);
            tile.chosen();
        }
    }
}
