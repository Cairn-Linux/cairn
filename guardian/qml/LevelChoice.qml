// SPDX-License-Identifier: Apache-2.0
import QtQuick
import Cairn.Brand

// One level to pick for a new child, with a sentence on what it gives.
// Two of these work as a pair of radio buttons.
Rectangle {
    id: choice

    required property int level
    required property string description
    property bool checked: false

    signal chosen

    height: words.implicitHeight + Tokens.bodySize * 2
    radius: Tokens.radiusSm
    color: Tokens.card
    border.color: choice.checked ? Tokens.machine : Tokens.line
    border.width: choice.checked ? Tokens.focusWidth : Tokens.strokeHairline
    activeFocusOnTab: true

    Accessible.role: Accessible.RadioButton
    Accessible.name: qsTr("Level %1").arg(choice.level)
    Accessible.description: choice.description
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

    // A ring, filled when this level is the one picked.
    Rectangle {
        id: ring

        x: Tokens.bodySize
        y: Tokens.bodySize + (Tokens.bodySize * Tokens.bodyLineHeight - height) / 2
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

    Column {
        id: words

        anchors.left: ring.right
        anchors.right: parent.right
        anchors.leftMargin: Tokens.bodySize
        anchors.rightMargin: Tokens.bodySize
        y: Tokens.bodySize

        Text {
            width: parent.width
            text: qsTr("Level %1").arg(choice.level)
            font.family: Tokens.fontFamily
            font.pixelSize: Tokens.bodySize
            font.weight: Tokens.weightBold
            lineHeight: Tokens.bodyLineHeight
            color: Tokens.paperLabel
            Accessible.ignored: true
        }

        Text {
            width: parent.width
            wrapMode: Text.WordWrap
            text: choice.description
            font.family: Tokens.fontFamily
            font.pixelSize: Tokens.bodySize
            lineHeight: Tokens.bodyLineHeight
            color: Tokens.paperLabel
            Accessible.ignored: true
        }
    }

    TapHandler {
        onTapped: {
            choice.forceActiveFocus(Qt.MouseFocusReason);
            choice.chosen();
        }
    }
}
