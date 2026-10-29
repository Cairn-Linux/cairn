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
    // The Save's sentences, so a held word or a game left out reads the
    // same here as after a Save.
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
        if (result.added >= 0)
            said.push(qsTr("Games added to %1's list: %2.").arg(note.name).arg(result.added));
        const held = note.sentences.heldSentence(result.held, note.name);
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
