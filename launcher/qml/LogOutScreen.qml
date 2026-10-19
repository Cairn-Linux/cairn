// SPDX-License-Identifier: Apache-2.0
import QtQuick
import Cairn.Brand
import Cairn.Launcher

// Asked once before the child's session ends (ADR-0021), so a stray press
// does not end it. Back is where the focus starts: pressing Enter twice by
// accident keeps the child where they were.
//
// Once the child says yes, the session takes a few seconds to end while
// Steam saves. The screen says so and has nothing left to press: on the
// laptop (2026-10-18) a screen that stayed the same read as a press that did
// nothing, and a child pressed again and again. If the session is still here
// when LogOut stops waiting, the question comes back with what to do next.
Rectangle {
    id: screen

    // The child said yes and the session is ending.
    required property bool leaving
    // The session did not end in time, and the child can choose again.
    required property bool didNotFinish

    signal stayed
    signal confirmed

    color: Tokens.ground

    Accessible.role: Accessible.Dialog
    Accessible.name: heading.text
    Accessible.description: sentence.text

    // Escape answers as Back does, and while the session ends there is no
    // Back.
    Keys.onEscapePressed: {
        if (!screen.leaving)
            screen.stayed();
    }

    // Decided from the flag, not from the tiles' visibility: when the flag
    // changes this runs before their binding has caught up. While leaving,
    // the screen itself has the focus, so keys land here and do nothing, and
    // a screen reader reads what is happening.
    function takeFocus() {
        if (!visible)
            return;
        if (leaving)
            screen.forceActiveFocus();
        else
            backTile.forceActiveFocus();
    }

    onVisibleChanged: takeFocus()
    onLeavingChanged: takeFocus()

    Column {
        anchors.centerIn: parent
        spacing: Tokens.headingSize
        width: parent.width - Tokens.headingSize * 2

        Text {
            id: heading

            objectName: "logOutHeading"
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: screen.leaving ? qsTr("Logging out.") : screen.didNotFinish ? qsTr("Log out did not finish.") : qsTr("Log out now?")
            font.family: Tokens.fontFamily
            font.pixelSize: Tokens.displaySize
            font.weight: Tokens.weightBold
            color: Tokens.text
        }

        Text {
            id: sentence

            objectName: "logOutSentence"
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: screen.leaving ? qsTr("This takes a few seconds.") : screen.didNotFinish ? qsTr("Try again, or ask a grown-up.") : qsTr("The next person can choose their name.")
            font.family: Tokens.fontFamily
            font.pixelSize: Tokens.headingSize
            color: Tokens.text
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Tokens.headingSize
            visible: !screen.leaving

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
