// SPDX-License-Identifier: Apache-2.0
import QtQuick
import Cairn.Brand
import Cairn.Launcher

// Shown instead of whatever the app printed when a launch went wrong, or when
// a window opened that nobody asked for; and, with other words, when a tile
// has nothing set up yet. Calm, short, and at most one thing to do: go back.
Rectangle {
    id: screen

    // The tile that was launched, or the window that opened, for the one
    // sentence that names it.
    required property string appTitle
    // A window opened on its own. Only closing that window ends this, so
    // there is no Back tile.
    required property bool openedOnItsOwn
    // The tile has nothing set up yet. Not a grown-up's job, so it says so.
    required property bool comingSoon

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
            text: screen.comingSoon ? qsTr("%1 is coming soon.").arg(screen.appTitle) : qsTr("Something needs a grown-up.")
            font.family: Tokens.fontFamily
            font.pixelSize: Tokens.displaySize
            font.weight: Tokens.weightBold
            color: Tokens.text
        }

        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: screen.comingSoon ? qsTr("It is not ready yet. Try something else.") : screen.openedOnItsOwn ? qsTr("%1 opened on its own.").arg(screen.appTitle) : qsTr("%1 did not start.").arg(screen.appTitle)
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
