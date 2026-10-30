// SPDX-License-Identifier: Apache-2.0
import QtQuick
import Cairn.Guardian

// The words of the Guardian's window for one child's Steam (ADR-0032), for
// each step of the visit. SteamForChild decides the step and the reason;
// this only picks the words.
QtObject {
    id: words

    required property SteamForChild visit

    readonly property string child: visit.childName

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
        case SteamForChild.Copying:
            return qsTr("Games are being copied into a child's Steam on this computer. Try again in a minute.");
        case SteamForChild.LoggingOut:
            return qsTr("A child is being logged out on this computer. Try again in a moment.");
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
}
