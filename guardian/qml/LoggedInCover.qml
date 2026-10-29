// SPDX-License-Identifier: Apache-2.0
import QtQuick
import Cairn.Brand
import Cairn.Family

// The card over a logged-in child's greyed games (ADR-0041). It says why
// their games wait, and offers to log them out: a warning first, then the
// password box, then the logout. If the child is not logged out in the
// end, it says why and what to do. ChildLogOut decides every value; this
// only picks the words for it.
Rectangle {
    id: cover

    required property GamesPage page

    readonly property ChildLogOut logOut: cover.page.loggingOut
    readonly property int step: cover.logOut.step
    // The child the card is about.
    readonly property string name: cover.step === ChildLogOut.NotAsking ? cover.page.chosenName : cover.logOut.name

    height: column.implicitHeight + Tokens.bodySize * 2
    radius: Tokens.radiusCard
    color: Tokens.card
    border.color: Tokens.machine
    border.width: Tokens.strokeHairline

    Accessible.role: Accessible.Grouping
    Accessible.name: sentence.text

    // Focus goes to the safe choice in the warning, and to Close after an
    // outcome; otherwise to Log out.
    function takeFocus() {
        if (cover.step === ChildLogOut.Warning)
            notNowButton.forceActiveFocus();
        else if (cover.step === ChildLogOut.NotLoggedOut)
            closeButton.forceActiveFocus();
        else if (cover.step === ChildLogOut.NotAsking && logOutButton.visible)
            logOutButton.forceActiveFocus();
    }

    function sentence() {
        const name = cover.name;
        switch (cover.step) {
        case ChildLogOut.Warning:
            return qsTr("Log %1 out? Anything %1 has not saved in a game or program is lost. Steam is asked to save first.").arg(name);
        case ChildLogOut.Asking:
            return qsTr("Confirm with your password, or your fingerprint, in the box that opened.");
        case ChildLogOut.LoggingOut:
            return qsTr("Logging %1 out. Steam gets a moment to save first.").arg(name);
        case ChildLogOut.NotLoggedOut:
            return cover.reasonSentence(name);
        }
        return qsTr("%1 is still logged in. Log %1 out, then choose their games.").arg(name);
    }

    function reasonSentence(name) {
        switch (cover.logOut.reason) {
        case ChildLogOut.Stuck:
            return qsTr("One of %1's programs would not close, so %1 is not fully logged out. Restart the computer, then choose %1's games.").arg(name);
        case ChildLogOut.StillClosing:
            return qsTr("%1's login was ended but has not closed yet. Wait a minute, then try again. If it happens again, restart the computer.").arg(name);
        case ChildLogOut.StoppedEarly:
            return qsTr("Logging %1 out was stopped before it finished. Try again.").arg(name);
        case ChildLogOut.VisitOpen:
            return qsTr("Steam for a child is open from a grown-up's login, so %1 was not logged out. Press Done in that window, then try again.").arg(name);
        case ChildLogOut.CopyRunning:
            return qsTr("Games are being copied to a child, so %1 was not logged out. Wait a few minutes, then try again.").arg(name);
        case ChildLogOut.NotSetUp:
            return qsTr("%1 was not logged out: the list that keeps a child from logging in meanwhile is not set up on this computer. The reason is in the system log. In a terminal: journalctl -t cairn-family -t cairn-steam-visit").arg(name);
        case ChildLogOut.NotStarted:
            return qsTr("Logging %1 out could not start. Trying again is safe. If it happens again, the reason is in the system log. In a terminal: journalctl -t cairn-family").arg(name);
        case ChildLogOut.Dismissed:
        case ChildLogOut.NotAuthorized:
            return qsTr("%1 was not logged out. It needs your password: the box was closed, or the password was not accepted.").arg(name);
        case ChildLogOut.NotAGrownUp:
            return qsTr("This account may not log a child out. A grown-up's account can.");
        case ChildLogOut.NotAChild:
            return qsTr("%1 is not a child's account on this computer any more, so they were not logged out.").arg(name);
        case ChildLogOut.CommandNotStarted:
            return qsTr("Cairn's family command could not be started, so %1 was not logged out. The reason is in the system log. In a terminal: journalctl -t cairn-guardian").arg(name);
        }
        return qsTr("Cairn's family command did not answer as expected, so %1 may still be logged in. The reason is in the system log. In a terminal: journalctl -t cairn-family -t cairn-steam-visit").arg(name);
    }

    Column {
        id: column

        x: Tokens.bodySize
        y: Tokens.bodySize
        width: cover.width - Tokens.bodySize * 2
        spacing: Tokens.bodySize

        Text {
            id: sentence

            objectName: "loggedInSentence"
            width: column.width
            wrapMode: Text.WordWrap
            text: cover.sentence()
            font.family: Tokens.fontFamily
            font.pixelSize: Tokens.bodySize
            lineHeight: Tokens.bodyLineHeight
            color: Tokens.text
            Accessible.role: Accessible.AlertMessage
            Accessible.name: text
        }

        Flow {
            width: column.width
            spacing: Tokens.bodySize / 2

            TextButton {
                id: logOutButton

                objectName: "logOutChild"
                visible: cover.step === ChildLogOut.NotAsking
                enabled: !cover.page.busy
                text: qsTr("Log %1 out").arg(cover.name)
                onActivated: cover.page.logOut()
            }

            TextButton {
                objectName: "confirmLogOut"
                visible: cover.step === ChildLogOut.Warning
                text: qsTr("Log %1 out").arg(cover.name)
                onActivated: cover.logOut.confirm()
            }

            TextButton {
                id: notNowButton

                objectName: "notNowLogOut"
                visible: cover.step === ChildLogOut.Warning
                quiet: true
                text: qsTr("Not now")
                onActivated: cover.logOut.notNow()
            }

            TextButton {
                id: closeButton

                objectName: "closeLogOut"
                visible: cover.step === ChildLogOut.NotLoggedOut
                quiet: true
                text: qsTr("Close")
                onActivated: cover.logOut.close()
            }
        }
    }
}
