// SPDX-License-Identifier: Apache-2.0
import QtQuick
import Cairn.Brand
import Cairn.Family

// Adding a child: the name the login screen will show, and a level. The
// window makes the account's own short name from the name, and asks for
// one only when the name has no letters a to z to make it from.
Column {
    id: form

    required property ChildrenPage page
    property int level: 1

    spacing: Tokens.bodySize
    topPadding: Tokens.bodySize

    Accessible.role: Accessible.Grouping
    Accessible.name: qsTr("Add a child")

    function clear() {
        nameField.text = "";
        shortNameField.text = "";
        form.level = 1;
    }

    function submit() {
        form.page.addChild(nameField.text, shortNameField.visible ? shortNameField.text : "", form.level);
    }

    Text {
        width: form.width
        wrapMode: Text.WordWrap
        text: qsTr("Add a child")
        font.family: Tokens.fontFamily
        font.pixelSize: Tokens.headingSize
        font.weight: Tokens.weightBold
        lineHeight: Tokens.headingLineHeight
        color: Tokens.text
        Accessible.role: Accessible.Heading
        Accessible.name: text
    }

    LabelledField {
        id: nameField

        objectName: "nameField"
        width: form.width
        label: qsTr("Name")
        hint: qsTr("The name on their login tile, such as Ada.")
        onSubmitted: form.submit()
    }

    LabelledField {
        id: shortNameField

        objectName: "shortNameField"
        width: form.width
        visible: form.page.needsShortName(nameField.text)
        label: qsTr("Short name for the account")
        hint: qsTr("The account also needs a short name with letters a to z. Only the computer uses it; the login tile shows the name above.")
        onSubmitted: form.submit()
    }

    Text {
        width: form.width
        wrapMode: Text.WordWrap
        text: qsTr("Level")
        font.family: Tokens.fontFamily
        font.pixelSize: Tokens.bodySize
        font.weight: Tokens.weightBold
        color: Tokens.text
        Accessible.ignored: true
    }

    LevelChoice {
        objectName: "level1"
        width: form.width
        level: 1
        description: qsTr("The launcher only. Opens apps and games, and nothing else.")
        checked: form.level === 1
        onChosen: form.level = 1
    }

    LevelChoice {
        objectName: "level2"
        width: form.width
        level: 2
        description: qsTr("The launcher, and their own files too.")
        checked: form.level === 2
        onChosen: form.level = 2
    }

    TextButton {
        objectName: "addButton"
        text: qsTr("Add")
        Accessible.name: qsTr("Add the child")
        onActivated: form.submit()
    }
}
