// SPDX-License-Identifier: Apache-2.0
import QtQuick

// Where the keyboard was among the Children page's rows, kept while the
// rows are made again, so it can go back there. The rows are also made
// again when the family was changed outside Family, with no change of the
// parent's own to send focus back after (2026-11-12). The keyboard goes
// back to the same control: another button in the row would send another
// change at the next Space or Enter.
QtObject {
    id: place

    // The login of the child whose row had the keyboard, or "", and the
    // name that row gives the control that had it.
    property string login: ""
    property string control: ""

    // Notes the row, when the item that has the keyboard is in it.
    function note(row, login, holder) {
        for (let inside = holder; inside; inside = inside.parent) {
            if (inside === row) {
                place.login = login;
                place.control = holder.objectName;
            }
        }
    }

    // The control of this name in the item, or null. A row names each of
    // its buttons and its field.
    function named(item, name) {
        if (item.objectName === name)
            return item;
        for (let i = 0; i < item.children.length; ++i) {
            const found = place.named(item.children[i], name);
            if (found)
                return found;
        }
        return null;
    }

    // Gives the keyboard to the control noted, in the row now made for
    // that child. A row without that control leaves the keyboard with the
    // window: a child moved to another level outside has other buttons.
    function giveBack(row) {
        const control = place.control === "" ? null : place.named(row, place.control);
        if (control)
            control.forceActiveFocus(Qt.TabFocusReason);
    }

    function forget() {
        place.login = "";
        place.control = "";
    }
}
