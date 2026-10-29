// SPDX-License-Identifier: Apache-2.0
import QtQuick
import Cairn.Brand
import Cairn.Family

// What happens to a child's new games that a Save could not copy yet, when
// they are written down for the child's next logout (ADR-0041): why they
// wait, then that they come by themselves. The child sees none of it.
Text {
    id: note

    // A GamesSaving.Held, the child's name, and how many games the Save
    // took off their list.
    required property int held
    required property string name
    required property int removed

    // Why the games wait, for each held word a logout settles. Each says
    // what was so at the Save, so it stays true after the child logs out,
    // even when the copy then has to wait for another logout.
    function reason() {
        switch (note.held) {
        case GamesSaving.LoggedIn:
            return qsTr("%1 was logged in, so %1's new games could not be copied yet.").arg(note.name);
        case GamesSaving.StillRunning:
            return qsTr("A program of %1's was still running from %1's last login, so %1's new games could not be copied yet.").arg(note.name);
        case GamesSaving.Stopped:
            return qsTr("Copying %1's games was stopped before it began.").arg(note.name);
        case GamesSaving.NotStarted:
            return qsTr("The copy for %1 could not start.").arg(note.name);
        }
        return qsTr("%1's new games could not be copied yet.").arg(note.name);
    }

    wrapMode: Text.WordWrap
    font.family: Tokens.fontFamily
    font.pixelSize: Tokens.bodySize
    lineHeight: Tokens.bodyLineHeight
    color: Tokens.paperLabel
    // What comes next, and, only when the Save took a game off, that it
    // has gone already. A log-out a program outlived needs a restart, so
    // it is never "nothing else to do".
    function nextSentence() {
        if (note.held === GamesSaving.Stuck)
            return qsTr("One of %1's programs would not close. Restart the computer. %1's new games are copied the next time %1 logs out.").arg(note.name);
        const said = note.reason() + " " + qsTr("%1's new games are copied the next time %1 logs out. You do not need to do anything else.").arg(note.name);
        if (note.removed <= 0)
            return said;
        return said + " " + qsTr("The games taken off are already gone from %1's list.").arg(note.name);
    }

    text: note.nextSentence()
    Accessible.role: Accessible.AlertMessage
    Accessible.name: text
}
