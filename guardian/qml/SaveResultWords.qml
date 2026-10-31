// SPDX-License-Identifier: Apache-2.0
import QtQuick
import Cairn.Family

// The words for what a Save did for each child (ADR-0041): what their list
// holds now, what held their new games back, which game was not given and
// why, and what the copy did with Steam's tools (ADR-0046). GamesSaving
// decides every value; this only picks the words.
QtObject {
    id: words

    required property GamesSaving saving

    // Why a child's new games were not all given, for each held word.
    // `removed` is how many games the Save took off the child's list, when
    // it is known.
    function heldSentence(held, name, removed) {
        switch (held) {
        case GamesSaving.NotHeld:
            return "";
        case GamesSaving.LoggedIn:
            if (removed > 0)
                return qsTr("%1 is logged in, so their new games could not be copied. They come after %1 logs out and you press Save again. The games taken off are already gone from %1's list.").arg(name);
            return qsTr("%1 is logged in, so their new games could not be copied. They come after %1 logs out and you press Save again.").arg(name);
        case GamesSaving.VisitOpen:
            return qsTr("Steam for a child is open from a grown-up's login, so %1's new games could not be copied. Press Done in that window, then press Save again.").arg(name);
        case GamesSaving.ChildLoggingOut:
            return qsTr("A child is being logged out, so %1's new games could not be copied. Wait a moment, then press Save again.").arg(name);
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

    // What the child's copy did with Steam's tools. Nothing when there is
    // nothing to say: the child had every tool, or their copy was held
    // before it came to the tools.
    function toolsSentence(tools, name) {
        switch (tools) {
        case GamesSaving.ToolsCopied:
            return qsTr("Copied the tools Steam runs games with into %1's Steam, from the copies Cairn keeps of the grown-ups' tools, so %1's first game does not have to download them. A tool Cairn has no copy of, or a newer version Valve has made since, still downloads the first time a game needs it, which can make that game wait several minutes.").arg(name);
        case GamesSaving.ToolsMissing:
            return qsTr("%1's Steam may not have every tool Steam runs games with yet. The first game that needs a missing one waits while %1's Steam downloads it, which can take several minutes. If that game does not start, log %1 out and back in, then try it again. The record is in the system log. In a terminal: journalctl -t cairn-steam-visit").arg(name);
        case GamesSaving.NoToolsToCopy:
            return qsTr("Cairn had no copies of the tools Steam runs games with to give %1's Steam. If %1's Steam does not have them yet, %1's first game waits while it downloads them, which can take several minutes. If that game does not start, log %1 out and back in, then try it again.").arg(name);
        }
        return "";
    }

    // Everything a Save did for one child, in a few sentences. A look for
    // new games changes no list, so it says only what held one back.
    function resultSentence(result) {
        const said = [];
        if (words.saving.kind === GamesSaving.Save && result.added >= 0) {
            if (result.added === 0 && result.removed === 0)
                said.push(qsTr("%1's list is saved, with no change.").arg(result.name));
            else
                said.push(qsTr("%1's list is saved. Added: %2. Taken off: %3.").arg(result.name).arg(result.added).arg(result.removed));
        }
        const held = words.heldSentence(result.held, result.name, result.removed);
        if (held !== "")
            said.push(held);
        for (let i = 0; i < result.skipped.length; ++i)
            said.push(words.skipSentence(result.skipped[i], result.name));
        if (result.skipped.length > 0)
            said.push(qsTr("The games not given that are still shared stay chosen, so pressing Save again tries them again."));
        const tools = words.toolsSentence(result.tools, result.name);
        if (tools !== "")
            said.push(tools);
        return said.join(" ");
    }
}
