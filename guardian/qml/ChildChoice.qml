// SPDX-License-Identifier: Apache-2.0
import QtQuick
import Cairn.Brand

// One child to pick on the Games page, with their name. The children's
// choices work as a set of radio buttons.
Rectangle {
    id: choice

    required property string name
    property bool checked: false

    signal chosen

    width: ring.width + label.implicitWidth + Tokens.bodySize * 3
    height: Tokens.headingSize * 2
    radius: Tokens.radiusSm
    color: Tokens.card
    border.color: choice.checked ? Tokens.machine : Tokens.line
    border.width: choice.checked ? Tokens.focusWidth : Tokens.strokeHairline
    activeFocusOnTab: true

    Accessible.role: Accessible.RadioButton
    Accessible.name: choice.name
    Accessible.checkable: true
    Accessible.checked: choice.checked
    Accessible.focusable: true
    Accessible.onPressAction: choice.chosen()

    Keys.onReturnPressed: choice.chosen()
    Keys.onEnterPressed: choice.chosen()
    Keys.onSpacePressed: choice.chosen()

    Rectangle {
        anchors.fill: parent
        anchors.margins: -Tokens.focusOffset - Tokens.focusWidth
        radius: choice.radius + Tokens.focusOffset + Tokens.focusWidth
        color: Qt.alpha(Tokens.focus, 0)
        border.width: Tokens.focusWidth
        border.color: Tokens.focus
        visible: choice.activeFocus
    }

    // A ring, filled when this child is the one picked.
    Rectangle {
        id: ring

        x: Tokens.bodySize
        anchors.verticalCenter: parent.verticalCenter
        width: Tokens.bodySize
        height: width
        radius: Tokens.radiusPill
        color: Tokens.card
        border.color: Tokens.machine
        border.width: Tokens.strokeHairline

        Rectangle {
            anchors.centerIn: parent
            width: parent.width / 2
            height: width
            radius: Tokens.radiusPill
            color: Tokens.machine
            visible: choice.checked
        }
    }

    // A name from the account database, so it is shown as plain text.
    Text {
        id: label

        anchors.left: ring.right
        anchors.leftMargin: Tokens.bodySize
        anchors.verticalCenter: parent.verticalCenter
        text: choice.name
        textFormat: Text.PlainText
        font.family: Tokens.fontFamily
        font.pixelSize: Tokens.bodySize
        font.weight: Tokens.weightBold
        color: Tokens.paperLabel
        Accessible.ignored: true
    }

    TapHandler {
        onTapped: {
            choice.forceActiveFocus(Qt.MouseFocusReason);
            choice.chosen();
        }
    }
}
