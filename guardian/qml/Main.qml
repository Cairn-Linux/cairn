// SPDX-License-Identifier: Apache-2.0
pragma ComponentBehavior: Bound
import QtQuick
import Cairn.Brand
import Cairn.Guardian

// The Guardian's window for one child's Steam (ADR-0032). It names the
// child, says in plain words what is happening, and has at most two
// buttons. The child's Steam itself is in a window of its own beside it.
Window {
    id: window

    required property SteamForChild visit

    readonly property string child: visit.childName
    readonly property int margin: Tokens.headingSize
    readonly property bool asksFirst: visit.step === SteamForChild.Ready || visit.step === SteamForChild.NotOpened

    title: qsTr("Steam for %1").arg(child)
    // The size follows the words, and only the words. Bound as the most and
    // the least the window may be, so a resize from KWin, such as when the
    // password box closes, cannot cut the buttons off.
    width: Tokens.headingSize * 22
    height: minimumHeight
    minimumWidth: width
    maximumWidth: width
    minimumHeight: content.implicitHeight + margin * 2
    maximumHeight: minimumHeight
    color: Tokens.ground
    visible: true

    onClosing: close => {
        close.accepted = visit.leave();
    }

    function heading() {
        switch (visit.step) {
        case SteamForChild.Ready:
            return qsTr("Open %1's Steam").arg(child);
        case SteamForChild.ClosingOwnSteam:
            return visit.ownSteamOpen ? qsTr("Closing your own Steam") : qsTr("Opening %1's Steam").arg(child);
        case SteamForChild.Asking:
        case SteamForChild.Waiting:
            return qsTr("Opening %1's Steam").arg(child);
        case SteamForChild.Open:
            return qsTr("%1's Steam is open").arg(child);
        case SteamForChild.Closing:
            return qsTr("Closing %1's Steam").arg(child);
        case SteamForChild.Closed:
            return qsTr("%1's Steam is closed").arg(child);
        case SteamForChild.Stuck:
            return qsTr("%1's Steam did not close properly").arg(child);
        case SteamForChild.NotOpened:
            return qsTr("%1's Steam did not open").arg(child);
        }
        return "";
    }

    function reason() {
        switch (visit.reason) {
        case SteamForChild.Busy:
            return qsTr("A child's Steam is already open on this computer. Press Done in its window first.");
        case SteamForChild.LoggedIn:
            return qsTr("%1 is logged in. %1 needs to log out first.").arg(child);
        case SteamForChild.StillEnding:
            return qsTr("%1's last login has not finished ending. Try again in a minute. If it keeps happening, restart the computer.").arg(child);
        case SteamForChild.NotSetUp:
            return qsTr("This computer's Cairn set-up is missing a part this needs. Run the set-up script again.");
        case SteamForChild.NotAChild:
            return qsTr("%1 is not a child's account on this computer.").arg(child);
        case SteamForChild.NotAtTheComputer:
            return qsTr("It opens only from your own login, on this computer's own screen.");
        case SteamForChild.NoPassword:
            return qsTr("It needs your password. The box was closed, or the password was not accepted.");
        case SteamForChild.NotAllowed:
            return qsTr("This account may not open a child's Steam. A grown-up's account can.");
        case SteamForChild.OwnSteamStillOpen:
            return qsTr("Your own Steam did not close. Quit it from Steam's own menu, then try again.");
        }
        return qsTr("The reason is in the system log. In a terminal: journalctl -t cairn-steam-for");
    }

    function paragraphs() {
        const ownSteamBack = visit.ownSteamComingBack ? [qsTr("Your own Steam is starting again.")] : [];
        switch (visit.step) {
        case SteamForChild.Ready:
            return [qsTr("%1's own Steam opens in a window on your desktop. There you can sign %1 in to Steam, switch the account, or change Steam's settings.").arg(child), qsTr("%1 cannot log in until you press Done.").arg(child)].concat(visit.ownSteamOpen ? [qsTr("Your own Steam closes first, with any game or download in it. It starts again when %1's Steam closes.").arg(child)] : []);
        case SteamForChild.ClosingOwnSteam:
            return visit.ownSteamOpen ? [qsTr("It saves first, which can take half a minute.")] : [];
        case SteamForChild.Asking:
            return [qsTr("Confirm with your password, or your fingerprint, in the box that opened.")];
        case SteamForChild.Waiting:
            return [qsTr("%1's last login is still ending. This takes a few seconds.").arg(child)];
        case SteamForChild.Open:
            return [qsTr("It is in a window of its own. The first time, Steam can take a minute or two to appear."), qsTr("When you have finished, press Done here, not the close button on Steam's window. Steam then saves and closes, which can take half a minute."), qsTr("Press Done before you log out, restart or shut down. Locking the screen closes %1's Steam too, and stops any download in it.").arg(child)];
        case SteamForChild.Closing:
            return [qsTr("Steam saves first, which can take half a minute.")];
        case SteamForChild.Closed:
            return [qsTr("%1 can log in again.").arg(child)].concat(ownSteamBack);
        case SteamForChild.Stuck:
            return [qsTr("A program of %1's would not end. %1 cannot log in until the computer restarts. Restart it when you can.").arg(child)].concat(ownSteamBack);
        case SteamForChild.NotOpened:
            return [reason()].concat(ownSteamBack);
        }
        return [];
    }

    function mainButtonText() {
        switch (visit.step) {
        case SteamForChild.Ready:
            return qsTr("Open");
        case SteamForChild.Open:
            return qsTr("Done");
        case SteamForChild.Closed:
        case SteamForChild.Stuck:
            return qsTr("Close");
        case SteamForChild.NotOpened:
            return qsTr("Try again");
        }
        return "";
    }

    function pressMainButton() {
        switch (visit.step) {
        case SteamForChild.Ready:
        case SteamForChild.NotOpened:
            visit.open();
            break;
        case SteamForChild.Open:
            visit.done();
            break;
        case SteamForChild.Closed:
        case SteamForChild.Stuck:
            window.close();
            break;
        }
    }

    Column {
        id: content

        x: window.margin
        y: window.margin
        width: window.width - window.margin * 2
        spacing: Tokens.bodySize

        Text {
            objectName: "heading"
            width: parent.width
            wrapMode: Text.WordWrap
            text: window.heading()
            font.family: Tokens.fontFamily
            font.pixelSize: Tokens.headingSize
            font.weight: Tokens.weightBold
            lineHeight: Tokens.headingLineHeight
            color: Tokens.text
            Accessible.role: Accessible.Heading
            Accessible.name: text
        }

        Repeater {
            model: window.paragraphs()

            Text {
                required property string modelData

                width: content.width
                wrapMode: Text.WordWrap
                text: modelData
                font.family: Tokens.fontFamily
                font.pixelSize: Tokens.bodySize
                lineHeight: Tokens.bodyLineHeight
                color: Tokens.text
                Accessible.role: Accessible.StaticText
                Accessible.name: text
            }
        }

        Row {
            spacing: Tokens.bodySize
            topPadding: Tokens.bodySize
            // From the words, not from the button: a hidden row hides the
            // button too, which would then never show the row again.
            visible: window.mainButtonText() !== ""

            TextButton {
                id: mainButton

                objectName: "mainButton"
                text: window.mainButtonText()
                visible: text !== ""
                focus: true
                onActivated: window.pressMainButton()
                onVisibleChanged: {
                    if (visible)
                        forceActiveFocus();
                }
            }

            TextButton {
                objectName: "wayOutButton"
                quiet: true
                text: window.visit.step === SteamForChild.Ready ? qsTr("Cancel") : qsTr("Close")
                visible: window.asksFirst
                onActivated: window.close()
            }
        }
    }
}
