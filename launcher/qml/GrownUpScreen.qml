// SPDX-License-Identifier: Apache-2.0
import QtQuick
import Cairn.Brand
import Cairn.Launcher

// Shown instead of whatever the app printed when a launch went wrong, or when
// a window opened that nobody asked for; and, with other words, when a tile
// has nothing set up yet. Calm, short, and at most one thing to do: go back;
// and, when the launcher knows, one line saying what will help (ADR-0045).
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
    // What will help, when the launcher knows: an AppLauncher.GrownUpStep.
    // C++ decides which; this screen only says it.
    required property int grownUpStep

    signal dismissed

    color: Tokens.ground

    Accessible.role: Accessible.Dialog
    Accessible.name: heading.text
    Accessible.description: grownUpStep !== AppLauncher.None ? sentence.text + " " + step.text : sentence.text

    Keys.onEscapePressed: screen.dismissed()

    Column {
        anchors.centerIn: parent
        spacing: Tokens.headingSize
        width: parent.width - Tokens.headingSize * 2

        // Both sentences may hold a title, or the title of a window that
        // opened on its own, so both are plain text, as on the tile.
        Text {
            id: heading

            objectName: "grownUpHeading"
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: screen.comingSoon ? qsTr("%1 is coming soon.").arg(screen.appTitle) : qsTr("Something needs a grown-up.")
            textFormat: Text.PlainText
            font.family: Tokens.fontFamily
            font.pixelSize: Tokens.displaySize
            font.weight: Tokens.weightBold
            color: Tokens.text
        }

        Text {
            id: sentence

            objectName: "grownUpSentence"
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: screen.comingSoon ? qsTr("It is not ready yet. Try something else.") : screen.openedOnItsOwn ? qsTr("%1 opened on its own.").arg(screen.appTitle) : qsTr("%1 did not start.").arg(screen.appTitle)
            textFormat: Text.PlainText
            font.family: Tokens.fontFamily
            font.pixelSize: Tokens.headingSize
            color: Tokens.text
        }

        // One line, never a reason, only when the launcher knows what will
        // help (ADR-0045).
        Text {
            id: step

            objectName: "grownUpStep"
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            visible: screen.grownUpStep !== AppLauncher.None
            text: screen.grownUpStep === AppLauncher.LogOutAndBackIn ? qsTr("Log out and back in, then try it again.") : ""
            textFormat: Text.PlainText
            font.family: Tokens.fontFamily
            font.pixelSize: Tokens.headingSize
            color: Tokens.text
            Accessible.role: Accessible.StaticText
            Accessible.name: text
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
