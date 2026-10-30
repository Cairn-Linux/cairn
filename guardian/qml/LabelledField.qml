// SPDX-License-Identifier: Apache-2.0
import QtQuick
import Cairn.Brand

// A text field with its label above it, and a line of help when it needs
// one. A secret one, for a password, shows dots in place of what is typed.
Column {
    id: field

    required property string label
    property string hint: ""
    property alias text: input.text
    property bool secret: false

    signal submitted

    spacing: Tokens.bodySize / 2

    function takeFocus() {
        input.forceActiveFocus();
    }

    Text {
        width: field.width
        wrapMode: Text.WordWrap
        text: field.label
        font.family: Tokens.fontFamily
        font.pixelSize: Tokens.bodySize
        font.weight: Tokens.weightBold
        color: Tokens.text
        Accessible.ignored: true
    }

    Text {
        width: field.width
        wrapMode: Text.WordWrap
        visible: field.hint !== ""
        text: field.hint
        font.family: Tokens.fontFamily
        font.pixelSize: Tokens.bodySize
        lineHeight: Tokens.bodyLineHeight
        color: Tokens.text
        Accessible.ignored: true
    }

    Rectangle {
        width: field.width
        height: Tokens.headingSize * 2
        radius: Tokens.radiusSm
        color: Tokens.card
        border.color: input.activeFocus ? Tokens.focus : Tokens.line
        border.width: Tokens.focusWidth

        TextInput {
            id: input

            objectName: "input"
            anchors.fill: parent
            anchors.leftMargin: Tokens.headingSize / 2
            anchors.rightMargin: Tokens.headingSize / 2
            verticalAlignment: TextInput.AlignVCenter
            activeFocusOnTab: true
            font.family: Tokens.fontFamily
            font.pixelSize: Tokens.bodySize
            color: Tokens.paperLabel
            selectionColor: Tokens.selection
            selectedTextColor: Tokens.paperLabel
            clip: true
            echoMode: field.secret ? TextInput.Password : TextInput.Normal
            passwordMaskDelay: 0

            Accessible.role: Accessible.EditableText
            Accessible.name: field.label
            Accessible.description: field.hint
            Accessible.passwordEdit: field.secret

            Keys.onReturnPressed: field.submitted()
            Keys.onEnterPressed: field.submitted()
        }
    }
}
