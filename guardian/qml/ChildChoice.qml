// SPDX-License-Identifier: Apache-2.0
import QtQuick
import Cairn.Brand

// One child to pick on the Games page, with their name, and "not saved"
// after it while their list has changes that are not saved yet. The
// children's choices work as a set of radio buttons.
Rectangle {
    id: choice

    required property string name
    property bool checked: false
    property bool unsaved: false

    signal chosen

    // A held key repeats. Only the first press counts, so holding Enter or
    // Space picks the child once.
    function keyPressed(event) {
        if (!event.isAutoRepeat)
            choice.chosen();
    }

    width: ring.width + label.implicitWidth + Tokens.bodySize * 3 + (choice.unsaved ? unsavedLabel.implicitWidth + Tokens.bodySize / 2 : 0)
    height: Tokens.headingSize * 2
    radius: Tokens.radiusSm
    color: Tokens.card
    border.color: choice.checked ? Tokens.machine : Tokens.line
    border.width: choice.checked ? Tokens.focusWidth : Tokens.strokeHairline
    activeFocusOnTab: true

    Accessible.role: Accessible.RadioButton
    Accessible.name: choice.name
    Accessible.description: choice.unsaved ? qsTr("Changes not saved yet") : ""
    Accessible.checkable: true
    Accessible.checked: choice.checked
    Accessible.focusable: true
    Accessible.onPressAction: choice.chosen()

    Keys.onReturnPressed: event => choice.keyPressed(event)
    Keys.onEnterPressed: event => choice.keyPressed(event)
    Keys.onSpacePressed: event => choice.keyPressed(event)

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

    Text {
        id: unsavedLabel

        objectName: "unsavedLabel"
        visible: choice.unsaved
        anchors.left: label.right
        anchors.leftMargin: Tokens.bodySize / 2
        anchors.verticalCenter: parent.verticalCenter
        text: qsTr("not saved")
        font.family: Tokens.fontFamily
        font.pixelSize: Tokens.minGuardian
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
