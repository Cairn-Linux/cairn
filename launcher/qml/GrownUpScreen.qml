// SPDX-License-Identifier: Apache-2.0
import QtQuick
import Cairn.Brand
import Cairn.Launcher

// Shown instead of whatever the app printed when a launch went wrong. Calm,
// short, and one thing to do: go back.
Rectangle {
    id: screen

    // The tile that was launched, for the one sentence that names it.
    required property string appTitle

    signal dismissed

    color: Tokens.ground

    Accessible.role: Accessible.Dialog
    Accessible.name: heading.text

    Keys.onEscapePressed: screen.dismissed()

    Column {
        anchors.centerIn: parent
        spacing: Tokens.headingSize
        width: parent.width - Tokens.headingSize * 2

        Text {
            id: heading

            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: qsTr("Something needs a grown-up.")
            font.family: Tokens.fontFamily
            font.pixelSize: Tokens.displaySize
            font.weight: Tokens.weightBold
            color: Tokens.text
        }

        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: qsTr("%1 did not start.").arg(screen.appTitle)
            font.family: Tokens.fontFamily
            font.pixelSize: Tokens.headingSize
            color: Tokens.text
        }

        Tile {
            id: backTile

            objectName: "backTile"
            anchors.horizontalCenter: parent.horizontalCenter
            width: Tokens.displaySize * 5
            height: Tokens.displaySize * 2
            title: qsTr("Back")
            kind: TileModel.Machine
            accessibleName: qsTr("Back to the tiles")
            focus: true
            onActivated: screen.dismissed()
        }
    }

    onVisibleChanged: {
        if (visible)
            backTile.forceActiveFocus();
    }
}
