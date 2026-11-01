// SPDX-License-Identifier: Apache-2.0
import QtQuick
import Cairn.Brand

// Up, Down, Page Up and Page Down for words that scroll, passed on by a
// button under them that has the keyboard (#243). Up and Down move the
// words a line; Page Up and Page Down move them as far as their part is
// tall, less a line, so the line at its edge stays in sight. The words
// stop at either end, so words that fit never move. Unlike a button's
// press, a held key goes on scrolling: each of its repeats comes here.
QtObject {
    id: keys

    required property Flickable flickable

    // Moves the words for one of the four keys, as far as they go, and
    // says whether the key was one of them. Any other key is left to the
    // button and the window.
    function scroll(key) {
        const line = Tokens.bodySize * Tokens.bodyLineHeight;
        const page = keys.flickable.height - line;
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
