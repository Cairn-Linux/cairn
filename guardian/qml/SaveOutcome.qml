// SPDX-License-Identifier: Apache-2.0
pragma ComponentBehavior: Bound
import QtQuick
import Cairn.Brand
import Cairn.Family

// What a Save on the Games page is doing, then what it did for each child,
// or why nothing was saved, in the house's plain sentences (ADR-0041). The
// Save's progress window shows it. GamesSaving decides every value; this
// only picks the words for it.
Column {
    id: box

    required property GamesSaving saving

    spacing: Tokens.bodySize / 2

    component Sentence: Text {
        width: box.width
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
            // A Save and a look both take Steam's tools from the parent's
            // own Steam (ADR-0046).
            if (box.saving.progress === GamesSaving.SharingTools)
                return qsTr("Getting the tools Steam runs games with from your own Steam. This usually takes a few seconds.");
            // A look names no child, so it has no list to write or copy.
            if (box.saving.kind === GamesSaving.Look)
                return qsTr("Looking at the shared folder for games and their pictures.");
            switch (box.saving.progress) {
            case GamesSaving.Writing:
                return qsTr("Saving the children's lists.");
            case GamesSaving.CopyingGames:
                return qsTr("Copying games for %1. This can take a few minutes, and %1 cannot log in until it ends.").arg(name);
            case GamesSaving.CopyingTools:
                return qsTr("Copying the tools Steam runs games with into %1's Steam. This usually takes a few seconds, and %1 cannot log in until it ends.").arg(name);
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
        const said = [qsTr("Saved. Each child sees their games the next time they log in.")];
        if (box.saving.kind === GamesSaving.Look)
            said[0] = qsTr("The shared folder was read again. Every game in it shows on the page.");
        else if (!box.saving.allGiven)
            said[0] = qsTr("Saved, but not every new game could be given. The sentences below say what each child has now, and what comes next.");
        if (box.saving.catalogueKept)
            said.push(qsTr("The list of shared games could not be made again, so the page shows the one from before. The reason is in the system log. In a terminal: journalctl -t cairn-family"));
        if (box.saving.toolsLeftOut)
            said.push(qsTr("Not every tool Steam runs games with could be taken from your own Steam, so a child who got new games may not have every tool yet. Their Steam downloads what is missing the first time a game needs it, which can take several minutes. If that game does not start, log the child out and back in, then try it again. The reason is in the system log. In a terminal: journalctl -t cairn-family"));
        return said.join(" ");
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

    SaveResultWords {
        id: words

        saving: box.saving
    }

    Sentence {
        objectName: "saveOutcome"
        text: box.stepSentence()
    }

    Repeater {
        objectName: "saveResults"
        model: box.saving.step === GamesSaving.Saved || box.saving.step === GamesSaving.NotSaved ? box.saving.results : []

        Sentence {
            required property var modelData

            objectName: "result_" + modelData.login
            text: words.resultSentence(modelData)
            visible: text !== ""
        }
    }
}
