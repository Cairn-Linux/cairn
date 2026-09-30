// SPDX-License-Identifier: Apache-2.0
import QtQuick
import Cairn.Brand
import Cairn.Launcher

// Asked once before the child's session ends (ADR-0021), so a stray press
// does not end it. Back is where the focus starts: pressing Enter twice by
// accident keeps the child where they were.
Rectangle {
    id: screen

    signal stayed
    signal confirmed

    color: Tokens.ground

    Accessible.role: Accessible.Dialog
    Accessible.name: heading.text

    Keys.onEscapePressed: screen.stayed()

    function takeFocus() {
        if (visible)
            backTile.forceActiveFocus();
    }

    onVisibleChanged: takeFocus()

    Column {
        anchors.centerIn: parent
        spacing: Tokens.headingSize
        width: parent.width - Tokens.headingSize * 2

        Text {
            id: heading

            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: qsTr("Log out now?")
            font.family: Tokens.fontFamily
            font.pixelSize: Tokens.displaySize
            font.weight: Tokens.weightBold
            color: Tokens.text
        }

        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: qsTr("The next person can choose their name.")
            font.family: Tokens.fontFamily
            font.pixelSize: Tokens.headingSize
            color: Tokens.text
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Tokens.headingSize

            Tile {
                id: backTile

                objectName: "stayTile"
                width: Tokens.displaySize * 5
                height: Tokens.displaySize * 2
                title: qsTr("Back")
                kind: TileModel.Machine
                accessibleName: qsTr("Back to the tiles")
                KeyNavigation.right: logOutTile
                onActivated: screen.stayed()
            }

            Tile {
                id: logOutTile

                objectName: "confirmLogOutTile"
                width: Tokens.displaySize * 5
                height: Tokens.displaySize * 2
                title: qsTr("Log out")
                kind: TileModel.Machine
                accessibleName: qsTr("Log out")
                KeyNavigation.left: backTile
                onActivated: screen.confirmed()
            }
        }
    }
}
