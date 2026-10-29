// SPDX-License-Identifier: Apache-2.0
pragma ComponentBehavior: Bound
import QtQuick
import Cairn.Brand
import Cairn.Family

// The copy at a child's logout, for the child picked on the Games page
// (ADR-0041): that their new games still wait for it, or how it went the
// last time, until the next Save for them. GamesPage reads both from the
// files root writes; this only picks the words.
Column {
    id: note

    required property GamesPage page
    // The Save's sentences, so a game left out reads the same here as
    // after a Save.
    required property SaveOutcome sentences

    readonly property string name: note.page.chosenName

    spacing: Tokens.bodySize / 2

    component Words: Text {
        width: note.width
        wrapMode: Text.WordWrap
        font.family: Tokens.fontFamily
        font.pixelSize: Tokens.bodySize
        lineHeight: Tokens.bodyLineHeight
        color: Tokens.text
        Accessible.role: Accessible.StaticText
        Accessible.name: text
    }

    // Why the copy at the child's last logout held their new games back.
    // The copy has ended and those games are off the list, so pressing Save
    // again does nothing: these say what to do first, and the closing
    // sentence gives the one way to try again. A held word a logout settles
    // keeps the games waiting and never reaches a finished copy.
    function heldSentence(held) {
        switch (held) {
        case GamesSaving.NotHeld:
            return "";
        case GamesSaving.VisitOpen:
            return qsTr("Steam for a child was open from a grown-up's login, so %1's new games could not be copied. If that window is still open, press Done in it first.").arg(note.name);
        case GamesSaving.CopyStillRunning:
            return qsTr("Another copy of games was still running, so %1's new games could not be copied.").arg(note.name);
        case GamesSaving.NotSetUp:
        case GamesSaving.NoReflink:
            return note.sentences.heldSentence(held, note.name);
        case GamesSaving.NoLibrary:
            return qsTr("%1's Steam has not run yet, so it has no place for games. Open Steam for %1 and sign in first.").arg(note.name);
        case GamesSaving.NoShared:
            return qsTr("There is no shared game folder, /var/lib/cairn/steam-library, so no games were copied for %1. Restarting the computer makes the folder again. If it happens again, the reason is in the system log. In a terminal: journalctl -t cairn-family -t cairn-steam-visit").arg(note.name);
        case GamesSaving.ListsShared:
            return qsTr("%1's Steam lists the shared game folder as its own, so no games were copied for %1. Open Steam for %1 and remove that folder in Steam's Settings, under Storage, first.").arg(note.name);
        case GamesSaving.UnreadableConfig:
            return qsTr("No games were copied for %1: their Steam's list of game folders is not a plain file, so Cairn does not read it. It is libraryfolders.vdf, in .local/share/Steam/config or .local/share/Steam/steamapps in %1's home. Make it a plain file again first. The record is in the system log. In a terminal: journalctl -t cairn-family -t cairn-steam-visit").arg(note.name);
        case GamesSaving.CopyStopped:
            return qsTr("Copying %1's games stopped early. Trying again is safe. If it fails again, the reason is in the system log. In a terminal: journalctl -t cairn-family -t cairn-steam-visit").arg(note.name);
        case GamesSaving.NotWritten:
            return qsTr("%1's list could not be written after the copy, so it holds none of the new games. Trying again is safe. If it fails again, the reason is in the system log. In a terminal: journalctl -t cairn-family").arg(note.name);
        }
        return qsTr("Not all of %1's new games could be given. The reason is in the system log. In a terminal: journalctl -t cairn-family -t cairn-steam-visit").arg(note.name);
    }

    // What the copy at the child's last logout did, in a few sentences.
    function finishedSentence() {
        const finished = note.page.finished;
        if (finished.result === undefined)
            return "";
        const when = finished.when.toLocaleDateString(Qt.locale(), Locale.LongFormat);
        if (finished.clean)
            return qsTr("%1's new games were copied when %1 logged out on %2.").arg(note.name).arg(when);
        const result = finished.result;
        const said = [qsTr("When %1 logged out on %2:").arg(note.name).arg(when)];
        if (result.added > 0)
            said.push(qsTr("Added to %1's list: %2.").arg(note.name).arg(result.added));
        const held = note.heldSentence(result.held);
        if (held !== "")
            said.push(held);
        for (let i = 0; i < result.skipped.length; ++i)
            said.push(note.sentences.skipSentence(result.skipped[i], note.name));
        said.push(qsTr("The games that were not given are not on %1's list now. To try them again, put them back on the list and press Save.").arg(note.name));
        return said.join(" ");
    }

    Words {
        objectName: "waitingNote"
        visible: note.page.chosenWaiting
        text: qsTr("%1's new games are copied the next time %1 logs out.").arg(note.name)
    }

    Words {
        objectName: "finishedNote"
        visible: text !== ""
        text: note.finishedSentence()
    }
}
