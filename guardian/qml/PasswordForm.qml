// SPDX-License-Identifier: Apache-2.0
import QtQuick
import Cairn.Brand
import Cairn.Family

// A new password, typed twice. The page asks for the one in use now only
// after both are typed, in Plasma's own box.
Column {
    id: form

    required property AccountPage page

    spacing: Tokens.bodySize

    Accessible.role: Accessible.Grouping
    Accessible.name: qsTr("Change your password")

    function clear() {
        newField.text = "";
        againField.text = "";
    }

    function takeFocus() {
        if (form.page.reason === AccountPage.Different)
            againField.takeFocus();
        else
            newField.takeFocus();
    }

    function submit() {
        form.page.changePassword(newField.text, againField.text);
    }

    Text {
        width: form.width
        wrapMode: Text.WordWrap
        text: qsTr("Password")
        font.family: Tokens.fontFamily
        font.pixelSize: Tokens.headingSize
        font.weight: Tokens.weightBold
        lineHeight: Tokens.headingLineHeight
        color: Tokens.text
        Accessible.role: Accessible.Heading
        Accessible.name: text
    }

    Text {
        width: form.width
        wrapMode: Text.WordWrap
        text: qsTr("Type the new password twice. Then a box asks for the password you use now. Choose one the children do not know: it lets anyone change the family's accounts.")
        font.family: Tokens.fontFamily
        font.pixelSize: Tokens.bodySize
        lineHeight: Tokens.bodyLineHeight
        color: Tokens.text
        Accessible.role: Accessible.StaticText
        Accessible.name: text
    }

    LabelledField {
        id: newField

        objectName: "newPasswordField"
        width: form.width
        secret: true
        label: qsTr("New password")
        onSubmitted: againField.takeFocus()
    }

    LabelledField {
        id: againField

        objectName: "againPasswordField"
        width: form.width
        secret: true
        label: qsTr("New password again")
        onSubmitted: form.submit()
    }

    TextButton {
        objectName: "changePasswordButton"
        text: qsTr("Change password")
        onActivated: form.submit()
    }
}
