// SPDX-License-Identifier: Apache-2.0
import QtQuick
import Cairn.Brand

// The password field under a tile that asks for one, with a way in and a way
// back. Parent-facing, so plain and a little longer is fine.
Column {
    id: row

    required property string displayName

    signal submitted(string password)
    signal cancelled

    spacing: Tokens.headingSize / 2

    Accessible.role: Accessible.Grouping
    Accessible.name: qsTr("Password for %1").arg(row.displayName)

    function takeFocus() {
        field.forceActiveFocus();
    }

    function clear() {
        field.text = "";
    }

    // An empty field is not a try: sending it would count as a wrong
    // password towards the lockout.
    function submit() {
        if (field.text.length > 0)
            row.submitted(field.text);
    }

    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        width: Tokens.displaySize * 8
        height: Tokens.headingSize * 2
        radius: Tokens.radiusSm
        color: Tokens.card
        border.color: field.activeFocus ? Tokens.focus : Tokens.line
        border.width: Tokens.focusWidth

        TextInput {
            id: field

            objectName: "passwordField"
            anchors.fill: parent
            anchors.leftMargin: Tokens.headingSize / 2
            anchors.rightMargin: Tokens.headingSize / 2
            verticalAlignment: TextInput.AlignVCenter
            echoMode: TextInput.Password
            font.family: Tokens.fontFamily
            font.pixelSize: Tokens.headingSize
            color: Tokens.paperLabel
            clip: true

            Accessible.role: Accessible.EditableText
            Accessible.name: qsTr("Password for %1").arg(row.displayName)
            Accessible.passwordEdit: true

            Keys.onReturnPressed: row.submit()
            Keys.onEnterPressed: row.submit()
            Keys.onEscapePressed: row.cancelled()

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Password for %1").arg(row.displayName)
                font: field.font
                color: Tokens.fjord
                visible: field.text.length === 0
            }
        }
    }

    Row {
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: Tokens.headingSize

        TextButton {
            objectName: "backButton"
            text: qsTr("Back")
            onActivated: row.cancelled()
        }

        TextButton {
            objectName: "logInButton"
            text: qsTr("Log in")
            onActivated: row.submit()
        }
    }
}
