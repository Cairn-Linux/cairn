// SPDX-License-Identifier: Apache-2.0
import QtQuick
import Cairn.Brand

// Up, Down, Page Up and Page Down for words that scroll, passed on by a
// button under them that has the keyboard (#243). Up and Down move the
// words one line of body text; Page Up and Page Down move them as far as
// their part is tall, less a line, so a line cut off at its edge shows
// whole after the move. The words stop at either end, so words that fit
// never move. Unlike a button's press, a held key goes on scrolling: each
// of its repeats comes here.
QtObject {
    id: keys

    required property Flickable flickable

    // The font of body text, to measure its line by.
    readonly property FontMetrics bodyFont: FontMetrics {
        font.family: Tokens.fontFamily
        font.pixelSize: Tokens.bodySize
    }

    // Moves the words for one of the four keys, as far as they go, and
    // says whether the key was one of them. Any other key is left to the
    // button and the window.
    function scroll(key) {
        // A line as Text draws it: its line height multiplies the font's
        // own height, rounded up to a whole pixel, not the font's size.
        const line = Math.ceil(keys.bodyFont.height) * Tokens.bodyLineHeight;
        // Never less than a line: in a part less than a line tall, the
        // part's height less a line would move the words the wrong way.
        const page = Math.max(line, keys.flickable.height - line);
        let step = 0;
        if (key === Qt.Key_Up)
            step = -line;
        else if (key === Qt.Key_Down)
            step = line;
        else if (key === Qt.Key_PageUp)
            step = -page;
        else if (key === Qt.Key_PageDown)
            step = page;
        else
            return false;
        const lowest = Math.max(0, keys.flickable.contentHeight - keys.flickable.height);
        keys.flickable.contentY = Math.max(0, Math.min(keys.flickable.contentY + step, lowest));
        return true;
    }
}
