// SPDX-License-Identifier: Apache-2.0
import QtQuick
import Cairn.Brand
import Cairn.Launcher

// Shown instead of whatever the app printed when a launch went wrong, or when
// a window opened that nobody asked for. Calm, short, and at most one thing
// to do: go back.
Rectangle {
    id: screen

    // The tile that was launched, or the window that opened, for the one
    // sentence that names it.
    required property string appTitle
    // A window opened on its own. Only closing it ends this, so there is no
    // Back tile: a child cannot dismiss what a grown-up has to see.
    required property bool openedOnItsOwn

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
            text: screen.openedOnItsOwn ? qsTr("%1 opened on its own.").arg(screen.appTitle) : qsTr("%1 did not start.").arg(screen.appTitle)
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
            visible: !screen.openedOnItsOwn
            focus: visible
            onActivated: screen.dismissed()
        }
    }

    // Decided from the flag, not from the Back tile's visibility: when the
    // flag changes this runs before the tile's binding has caught up.
    function takeFocus() {
        if (!visible)
            return;
        if (openedOnItsOwn)
            screen.forceActiveFocus();
        else
            backTile.forceActiveFocus();
    }

    onVisibleChanged: takeFocus()
    onOpenedOnItsOwnChanged: takeFocus()
}
