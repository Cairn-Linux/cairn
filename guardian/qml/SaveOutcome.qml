// SPDX-License-Identifier: Apache-2.0
pragma ComponentBehavior: Bound
import QtQuick
import Cairn.Brand
import Cairn.Family

// What a Save on the Games page is doing, then what it did for each child,
// or why nothing was saved, in the house's plain sentences (ADR-0041).
// GamesSaving decides every value; this only picks the words for it.
Rectangle {
    id: box

    required property GamesSaving saving

    height: column.implicitHeight + Tokens.bodySize * 2
    visible: box.saving.step !== GamesSaving.Ready
    radius: Tokens.radiusSm
    color: Tokens.card
    border.color: Tokens.machine
    border.width: Tokens.strokeHairline

    component Sentence: Text {
        width: column.width
        wrapMode: Text.WordWrap
        font.family: Tokens.fontFamily
        font.pixelSize: Tokens.bodySize
        lineHeight: Tokens.bodyLineHeight
        color: Tokens.paperLabel
        Accessible.role: Accessible.AlertMessage
        Accessible.name: text
    }

    function stepSentence() {
        const name = box.saving.progressName;
        switch (box.saving.step) {
        case GamesSaving.Asking:
            return qsTr("Confirm with your password, or your fingerprint, in the box that opened.");
        case GamesSaving.Saving:
            // A look names no child, so it has no list to write or copy.
            if (box.saving.kind === GamesSaving.Look)
                return qsTr("Looking at the shared folder for games and their pictures.");
            switch (box.saving.progress) {
            case GamesSaving.Writing:
                return qsTr("Saving the children's lists.");
            case GamesSaving.CopyingGames:
                return qsTr("Copying games for %1. This can take a few minutes, and %1 cannot log in until it ends.").arg(name);
            case GamesSaving.WaitingForLogout:
                return qsTr("Waiting for %1's last login to end.").arg(name);
            }
            return qsTr("Looking at the shared folder for games and their pictures.");
        case GamesSaving.Saved:
            return box.savedSentence();
        case GamesSaving.NotSaved:
            return box.reasonSentence();
        }
        return "";
    }

    // A Save that could not give every child every new game never opens
    // by saying that each child sees their games.
    function savedSentence() {
        let said = qsTr("Saved. Each child sees their games the next time they log in.");
        if (box.saving.kind === GamesSaving.Look)
            said = qsTr("The shared folder was read again. Every game in it shows below.");
        else if (!box.saving.allGiven)
            said = qsTr("Saved, but not every new game could be given. The sentences below say what each child has now, and what comes next.");
        if (!box.saving.catalogueKept)
            return said;
        return said + " " + qsTr("The list of shared games could not be made again, so the page shows the one from before. The reason is in the system log. In a terminal: journalctl -t cairn-family");
    }

    function reasonSentence() {
        if (box.saving.kind === GamesSaving.Look)
            return box.lookReasonSentence();
        switch (box.saving.reason) {
        case GamesSaving.Dismissed:
        case GamesSaving.NotAuthorized:
            return qsTr("Nothing was saved. It needs your password: the box was closed, or the password was not accepted. Your changes are still here.");
        case GamesSaving.CommandNotStarted:
            return qsTr("Cairn's family command could not be started, so nothing was saved. Your changes are still here. The reason is in the system log. In a terminal: journalctl -t cairn-guardian");
        case GamesSaving.NotAGrownUp:
            return qsTr("This account may not change the children's games. A grown-up's account can.");
        case GamesSaving.NotRoot:
            return qsTr("Cairn's family command did not get the rights it needs, so nothing was saved. Close Family and open it again. If it happens again, the reason is in the system log. In a terminal: journalctl -t cairn-family -t cairn-guardian");
        case GamesSaving.Usage:
            return qsTr("This window asked Cairn's family command for something it does not do, so nothing was saved. The window and the command may come from different versions of Cairn. The reason is in the system log. In a terminal: journalctl -t cairn-family -t cairn-guardian");
        case GamesSaving.NotAChild:
            return qsTr("One of the children is not a child's account on this computer any more, so nothing was saved. The page shows the children there are now.");
        case GamesSaving.AnotherSave:
            return qsTr("Another Save is still running, perhaps from another login, so nothing was saved. Wait until it ends, then press Save again.");
        }
        return qsTr("Cairn's family command did not answer as expected. Some lists may have been saved, and the page shows what is saved now. The reason is in the system log. In a terminal: journalctl -t cairn-family -t cairn-guardian");
    }

    // Why a look for new games did not run to its end. It saves no list,
    // so these never speak of saving or of changes.
    function lookReasonSentence() {
        switch (box.saving.reason) {
        case GamesSaving.Dismissed:
        case GamesSaving.NotAuthorized:
            return qsTr("The shared folder was not looked at again. It needs your password: the box was closed, or the password was not accepted.");
        case GamesSaving.NotAGrownUp:
            return qsTr("This account may not look for new games for the children. A grown-up's account can.");
        case GamesSaving.AnotherSave:
            return qsTr("A Save is still running, perhaps from another login, so the shared folder was not looked at again. Wait until it ends, then press Look for new games again.");
        case GamesSaving.CommandNotStarted:
            return qsTr("Cairn's family command could not be started, so the shared folder was not looked at again. The reason is in the system log. In a terminal: journalctl -t cairn-guardian");
        case GamesSaving.Usage:
            return qsTr("This window asked Cairn's family command for something it does not do, so the shared folder was not looked at again. The window and the command may come from different versions of Cairn. The reason is in the system log. In a terminal: journalctl -t cairn-family -t cairn-guardian");
        case GamesSaving.NotRoot:
            return qsTr("Cairn's family command did not get the rights it needs, so the shared folder was not looked at again. Close Family and open it again. If it happens again, the reason is in the system log. In a terminal: journalctl -t cairn-family -t cairn-guardian");
        }
        return qsTr("Cairn's family command did not answer as expected. No child's list was changed, and the page shows the games there are now. The reason is in the system log. In a terminal: journalctl -t cairn-family -t cairn-guardian");
    }

    // Why a child's new games were not all given, for each held word.
    function heldSentence(held, name) {
        switch (held) {
        case GamesSaving.NotHeld:
            return "";
        case GamesSaving.LoggedIn:
            return qsTr("%1 is logged in, so their new games could not be copied. They come after %1 logs out and you press Save again. The games taken off are already gone from %1's list.").arg(name);
        case GamesSaving.VisitOpen:
            return qsTr("Steam for a child is open from a grown-up's login, so %1's new games could not be copied. Press Done in that window, then press Save again.").arg(name);
        case GamesSaving.CopyStillRunning:
            return qsTr("Another copy of games is still running, so %1's new games could not be copied. Wait a few minutes, then press Save again.").arg(name);
        case GamesSaving.StillRunning:
            return qsTr("A program of %1's was still running from their last login, so their new games could not be copied. Wait a minute, then press Save again. If it happens again, restart the computer first.").arg(name);
        case GamesSaving.Stopped:
            return qsTr("Copying %1's games was stopped before it began. Press Save again.").arg(name);
        case GamesSaving.NotSetUp:
            return qsTr("No games were copied for %1: the list that keeps a child from logging in during a copy is not set up on this computer. The reason is in the system log. In a terminal: journalctl -t cairn-family -t cairn-steam-visit").arg(name);
        case GamesSaving.NoLibrary:
            return qsTr("%1's Steam has not run yet, so it has no place for games. Open Steam for %1 and sign in, then press Save again.").arg(name);
        case GamesSaving.NoShared:
            return qsTr("There is no shared game folder, /var/lib/cairn/steam-library, so no games were copied for %1. Restarting the computer makes the folder again; then press Save again. If it happens again, the reason is in the system log. In a terminal: journalctl -t cairn-family -t cairn-steam-visit").arg(name);
        case GamesSaving.ListsShared:
            return qsTr("%1's Steam lists the shared game folder as its own, so no games were copied for %1. Open Steam for %1, remove that folder in Steam's Settings, under Storage, then press Save again.").arg(name);
        case GamesSaving.UnreadableConfig:
            return qsTr("No games were copied for %1: their Steam's list of game folders is not a plain file, so Cairn does not read it. It is libraryfolders.vdf, in .local/share/Steam/config or .local/share/Steam/steamapps in %1's home. Once it is a plain file again, press Save again. The record is in the system log. In a terminal: journalctl -t cairn-family -t cairn-steam-visit").arg(name);
        case GamesSaving.NoReflink:
            return qsTr("No games were copied for %1: this computer's disk cannot make a copy that shares the original's space, and full copies would fill it.").arg(name);
        case GamesSaving.CopyStopped:
            return qsTr("Copying %1's games stopped early. Pressing Save again is safe. If it fails again, the reason is in the system log. In a terminal: journalctl -t cairn-family -t cairn-steam-visit").arg(name);
        case GamesSaving.NotStarted:
            return qsTr("The copy for %1 could not start. Pressing Save again is safe. If it happens again, the reason is in the system log. In a terminal: journalctl -t cairn-family").arg(name);
        case GamesSaving.NotWritten:
            return qsTr("%1's list could not be written after the copy, so it holds none of the new games, and the counts say what it holds. Pressing Save again is safe. If it fails again, the reason is in the system log. In a terminal: journalctl -t cairn-family").arg(name);
        }
        return qsTr("Not all of %1's new games could be given. The reason is in the system log. In a terminal: journalctl -t cairn-family -t cairn-steam-visit").arg(name);
    }

    // Why one game was not given to a child, for each reason word.
    function skipSentence(skip, name) {
        const title = skip.title !== "" ? skip.title : qsTr("Game %1").arg(skip.appId);
        switch (skip.reason) {
        case GamesSaving.NotShared:
            return qsTr("%1 was not given to %2: it is no longer in the shared folder.").arg(title).arg(name);
        case GamesSaving.NoCatalogue:
            return qsTr("%1 was not given to %2: the list of shared games could not be read.").arg(title).arg(name);
        case GamesSaving.UnreadableManifest:
            return qsTr("%1 was not given to %2: Steam's file for it cannot be read.").arg(title).arg(name);
        case GamesSaving.OtherGame:
            return qsTr("%1 was not given to %2: Steam's file for it names another game.").arg(title).arg(name);
        case GamesSaving.OddFolder:
            return qsTr("%1 was not given to %2: Steam's file for it names its folder in a way Cairn does not copy.").arg(title).arg(name);
        case GamesSaving.BetaBranch:
            return qsTr("%1 was not given to %2: it is on a beta branch, which %2's Steam account may not have.").arg(title).arg(name);
        case GamesSaving.NotInstalled:
            return qsTr("%1 was not given to %2: it is not fully installed, or an update is waiting. Let your own Steam finish it.").arg(title).arg(name);
        case GamesSaving.Downloading:
            return qsTr("%1 was not given to %2: Steam is still downloading it.").arg(title).arg(name);
        case GamesSaving.NoFolder:
            return qsTr("%1 was not given to %2: its folder is missing from the shared folder.").arg(title).arg(name);
        case GamesSaving.FolderThere:
            return qsTr("%1 was not given to %2: %2's Steam already has a folder for it, which may hold %2's saves, so Cairn leaves it alone.").arg(title).arg(name);
        case GamesSaving.ChangedDuringCopy:
            return qsTr("%1 was not given to %2: Steam changed it during the copy.").arg(title).arg(name);
        case GamesSaving.CopyDidNotWork:
            return qsTr("%1 was not given to %2: the copy did not work.").arg(title).arg(name);
        case GamesSaving.UnreadableFiles:
            return qsTr("%1 was not given to %2: some of its files can be read only by your own account, so they cannot be copied.").arg(title).arg(name);
        }
        return qsTr("%1 was not given to %2: Cairn does not copy it.").arg(title).arg(name);
    }

    // Everything a Save did for one child, in a few sentences. A look for
    // new games changes no list, so it says only what held one back.
    function resultSentence(result) {
        const said = [];
        if (box.saving.kind === GamesSaving.Save && result.added >= 0) {
            if (result.added === 0 && result.removed === 0)
                said.push(qsTr("%1's list is saved, with no change.").arg(result.name));
            else
                said.push(qsTr("%1's list is saved. Added: %2. Taken off: %3.").arg(result.name).arg(result.added).arg(result.removed));
        }
        // A child whose games wait for their logout gets the LaterNote
        // instead.
        const held = result.later ? "" : box.heldSentence(result.held, result.name);
        if (held !== "")
            said.push(held);
        for (let i = 0; i < result.skipped.length; ++i)
            said.push(box.skipSentence(result.skipped[i], result.name));
        if (result.skipped.length > 0)
            said.push(qsTr("The games not given that are still shared stay chosen, so pressing Save again tries them again."));
        return said.join(" ");
    }

    Column {
        id: column

        x: Tokens.bodySize
        y: Tokens.bodySize
        width: box.width - Tokens.bodySize * 2
        spacing: Tokens.bodySize / 2

        Sentence {
            objectName: "saveOutcome"
            text: box.stepSentence()
        }

        Repeater {
            objectName: "saveResults"
            model: box.saving.step === GamesSaving.Saved || box.saving.step === GamesSaving.NotSaved ? box.saving.results : []

            Column {
                id: result

                required property var modelData

                objectName: "result_" + modelData.login
                width: column.width
                spacing: Tokens.bodySize / 2

                Sentence {
                    objectName: "resultSentence"
                    text: box.resultSentence(result.modelData)
                    visible: text !== ""
                }

                LaterNote {
                    objectName: "laterNote"
                    width: column.width
                    visible: result.modelData.later === true
                    held: result.modelData.held
                    name: result.modelData.name
                }
            }
        }
    }
}
