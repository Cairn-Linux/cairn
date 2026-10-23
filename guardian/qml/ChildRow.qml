// SPDX-License-Identifier: Apache-2.0
pragma ComponentBehavior: Bound
import QtQuick
import Cairn.Brand

// One child on the Children page: their name, their level, and a button
// for each other level the page can give. Up or down, the button reads the
// same (DESIGN §3).
Rectangle {
    id: row

    // login, name, level and otherLevels, from ChildrenPage.listedChildren.
    required property var child

    signal levelChosen(int level)

    // The name and the buttons side by side when they fit, otherwise the
    // buttons on a line of their own under the name, so a long name or two
    // buttons never squeeze the name away.
    readonly property bool sideBySide: Math.max(childName.implicitWidth, childLevel.implicitWidth) + buttons.width + Tokens.bodySize * 3 <= row.width

    function takeFocus() {
        const first = buttonRepeater.itemAt(0);
        if (first)
            first.forceActiveFocus(Qt.TabFocusReason);
    }

    height: (row.sideBySide ? Math.max(names.implicitHeight, buttons.height) : names.implicitHeight + Tokens.bodySize / 2 + buttons.height) + Tokens.bodySize * 2
    radius: Tokens.radiusSm
    color: Tokens.card
    border.color: Tokens.line
    border.width: Tokens.strokeHairline

    Accessible.role: Accessible.Grouping
    Accessible.name: qsTr("%1, level %2").arg(row.child.name).arg(row.child.level)

    Column {
        id: names

        x: Tokens.bodySize
        y: row.sideBySide ? (row.height - names.implicitHeight) / 2 : Tokens.bodySize
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

    Row {
        id: buttons

        objectName: "buttons"
        x: row.sideBySide ? row.width - Tokens.bodySize - buttons.width : Tokens.bodySize
        y: row.sideBySide ? (row.height - buttons.height) / 2 : names.y + names.implicitHeight + Tokens.bodySize / 2
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
    }
}
