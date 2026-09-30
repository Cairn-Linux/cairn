// SPDX-License-Identifier: Apache-2.0
import QtQuick
import Cairn.Brand

// A plain word on the dark ground that does one thing.
Rectangle {
    id: button

    property alias text: label.text

    signal activated

    width: label.implicitWidth + Tokens.headingSize * 2
    height: Tokens.headingSize * 2
    radius: Tokens.radiusSm
    color: Tokens.fjord
    border.color: Tokens.focus
    border.width: button.activeFocus ? Tokens.focusWidth : 0
    activeFocusOnTab: true

    Accessible.role: Accessible.Button
    Accessible.name: label.text
    Accessible.onPressAction: button.activated()

    Keys.onReturnPressed: button.activated()
    Keys.onEnterPressed: button.activated()
    Keys.onSpacePressed: button.activated()

    Text {
        id: label

        anchors.centerIn: parent
        font.family: Tokens.fontFamily
        font.pixelSize: Tokens.minChild
        font.weight: Tokens.weightBold
        color: Tokens.machineLabel
    }

    MouseArea {
        anchors.fill: parent
        onClicked: button.activated()
    }
}
