// SPDX-License-Identifier: Apache-2.0
pragma ComponentBehavior: Bound
import QtQuick
import Cairn.Brand

// One child on the Children page: their name, their level, a button for
// each other level the page can give, and Change name. Up or down, the
// level button reads the same (DESIGN §3). Change name opens a field for
// the new name under the rest (ADR-0056).
Rectangle {
    id: row

    // login, name, level and otherLevels, from ChildrenPage.listedChildren.
    required property var child
    // Whether the field for a new name is open, and what it holds. The page
    // keeps both, because the list is made again after every change.
    property bool renaming: false
    property string typedName: ""

    signal levelChosen(int level)
    signal renameOpened
    signal typedNameEdited(string name)
    signal nameChosen(string name)
    signal renameClosed

    // The buttons' width on one line.
    readonly property real buttonsWidth: {
        let total = changeName.width;
        for (let i = 0; i < buttonRepeater.count; ++i) {
            const button = buttonRepeater.itemAt(i);
            if (button)
                total += button.width + buttons.spacing;
        }
        return total;
    }
    // The name and the buttons side by side when they fit, otherwise the
    // buttons under the name, as many to a line as fit, so a long name or
    // three buttons never squeeze the name away.
    readonly property bool sideBySide: Math.max(childName.implicitWidth, childLevel.implicitWidth) + row.buttonsWidth + Tokens.bodySize * 3 <= row.width
    // The name and the buttons, without the field for a new name.
    readonly property real topHeight: row.sideBySide ? Math.max(names.implicitHeight, buttons.height) : names.implicitHeight + Tokens.bodySize / 2 + buttons.height

    function takeFocus() {
        const first = buttonRepeater.itemAt(0);
        if (first)
            first.forceActiveFocus(Qt.TabFocusReason);
        else
            changeName.forceActiveFocus(Qt.TabFocusReason);
    }

    function focusChangeName() {
        changeName.forceActiveFocus(Qt.TabFocusReason);
    }

    function focusNewName() {
        nameChange.takeFocus();
    }

    height: row.topHeight + (row.renaming ? Tokens.bodySize + nameChange.implicitHeight : 0) + Tokens.bodySize * 2
    radius: Tokens.radiusSm
    color: Tokens.card
    border.color: Tokens.line
    border.width: Tokens.strokeHairline

    Accessible.role: Accessible.Grouping
    Accessible.name: qsTr("%1, level %2").arg(row.child.name).arg(row.child.level)

    Column {
        id: names

        x: Tokens.bodySize
        y: row.sideBySide ? Tokens.bodySize + (row.topHeight - names.implicitHeight) / 2 : Tokens.bodySize
        width: row.sideBySide ? buttons.x - Tokens.bodySize * 2 : row.width - Tokens.bodySize * 2

        Text {
            id: childName

            objectName: "childName"
            width: parent.width
            elide: Text.ElideRight
            text: row.child.name
            font.family: Tokens.fontFamily
            font.pixelSize: Tokens.bodySize
            font.weight: Tokens.weightBold
            color: Tokens.paperLabel
            Accessible.role: Accessible.StaticText
            Accessible.name: text
        }

        Text {
            id: childLevel

            objectName: "childLevel"
            width: parent.width
            elide: Text.ElideRight
            text: qsTr("Level %1").arg(row.child.level)
            font.family: Tokens.fontFamily
            font.pixelSize: Tokens.bodySize
            color: Tokens.paperLabel
            Accessible.role: Accessible.StaticText
            Accessible.name: text
        }
    }

    Flow {
        id: buttons

        objectName: "buttons"
        width: row.sideBySide ? Math.ceil(row.buttonsWidth) : row.width - Tokens.bodySize * 2
        x: row.sideBySide ? row.width - Tokens.bodySize - buttons.width : Tokens.bodySize
        y: row.sideBySide ? Tokens.bodySize + (row.topHeight - buttons.height) / 2 : names.y + names.implicitHeight + Tokens.bodySize / 2
        spacing: Tokens.bodySize / 2

        Repeater {
            id: buttonRepeater

            model: row.child.otherLevels

            TextButton {
                required property int modelData

                objectName: "moveTo" + modelData
                quiet: true
                text: qsTr("Move to level %1").arg(modelData)
                Accessible.name: qsTr("Move %1 to level %2").arg(row.child.name).arg(modelData)
                onActivated: row.levelChosen(modelData)
            }
        }

        TextButton {
            id: changeName

            objectName: "changeName"
            quiet: true
            text: qsTr("Change name")
            Accessible.name: qsTr("Change the name of %1").arg(row.child.name)
            onActivated: {
                row.renameOpened();
                Qt.callLater(row.focusNewName);
            }
        }
    }

    NameChange {
        id: nameChange

        objectName: "nameChange"
        x: Tokens.bodySize
        y: Tokens.bodySize + row.topHeight + Tokens.bodySize
        width: row.width - Tokens.bodySize * 2
        visible: row.renaming
        childName: row.child.name
        text: row.typedName
        onTextChanged: {
            if (row.renaming && nameChange.text !== row.typedName)
                row.typedNameEdited(nameChange.text);
        }
        onChosen: name => row.nameChosen(name)
        onCancelled: {
            row.renameClosed();
            Qt.callLater(row.focusChangeName);
        }
    }
}
