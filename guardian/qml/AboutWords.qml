// SPDX-License-Identifier: Apache-2.0
import QtQuick
import Cairn.Brand

// A block of words on the About page. It is plain text, so nothing in it
// is ever read as markup, and a long path breaks where it must rather than
// run out of the window. Tab stops on it, with the ring around it, so the
// page can be read with the keyboard alone, and it has a name and a role
// for a screen reader.
Text {
    id: words

    // A key pressed while the keyboard is here, for the page to scroll by.
    signal keyPressed(var event)

    wrapMode: Text.Wrap
    textFormat: Text.PlainText
    font.family: Tokens.fontFamily
    font.pixelSize: Tokens.bodySize
    lineHeight: Tokens.bodyLineHeight
    color: Tokens.text
    activeFocusOnTab: true
    Accessible.role: Accessible.StaticText
    Accessible.name: text
    Accessible.focusable: true
    Keys.onPressed: event => words.keyPressed(event)

    Rectangle {
        anchors.fill: parent
        anchors.margins: -Tokens.focusOffset - Tokens.focusWidth
        radius: Tokens.radiusSm + Tokens.focusOffset + Tokens.focusWidth
        color: Qt.alpha(Tokens.focus, 0)
        border.width: Tokens.focusWidth
        border.color: Tokens.focus
        visible: words.activeFocus
        Accessible.ignored: true
    }
}
