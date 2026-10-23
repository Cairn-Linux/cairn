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

    height: Math.max(names.implicitHeight, buttons.height) + Tokens.bodySize * 2
    radius: Tokens.radiusSm
    color: Tokens.card
    border.color: Tokens.line
    border.width: Tokens.strokeHairline

    Accessible.role: Accessible.Grouping
    Accessible.name: qsTr("%1, level %2").arg(row.child.name).arg(row.child.level)

    Column {
        id: names

        anchors.left: parent.left
        anchors.right: buttons.left
        anchors.leftMargin: Tokens.bodySize
        anchors.rightMargin: Tokens.bodySize
        anchors.verticalCenter: parent.verticalCenter

        Text {
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
            objectName: "childLevel"
            width: parent.width
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

        anchors.right: parent.right
        anchors.rightMargin: Tokens.bodySize
        anchors.verticalCenter: parent.verticalCenter
        spacing: Tokens.bodySize / 2

        Repeater {
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
