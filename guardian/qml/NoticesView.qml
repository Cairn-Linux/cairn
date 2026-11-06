// SPDX-License-Identifier: Apache-2.0
pragma ComponentBehavior: Bound
import QtQuick
import Cairn.Brand
import Cairn.Family

// The Notices page (ADR-0054): wrong passwords typed on this computer in
// the last 30 days, for every account, as the journal still has them. Each
// notice is plain text: the names in it are accounts' own names, and
// nothing in it is ever read as markup.
//
// The keyboard reads it too. Each notice, and the words under them, takes
// the keyboard in turn with Tab, and the window scrolls to show it; Down
// and Up go to the next notice and the one before; Page Down and Page Up
// scroll the page, and the keyboard goes to a notice in sight.
Column {
    id: view

    required property NoticesPage page
    // The page's scroller, for Page Up and Page Down.
    required property Flickable flickable

    spacing: Tokens.bodySize

    // The notices, then the words under them, as the keyboard visits them.
    function stops() {
        const list = [];
        for (let i = 0; i < rows.count; ++i)
            list.push(rows.itemAt(i));
        if (footer.visible)
            list.push(footer);
        return list;
    }

    // Whether the item shows whole in the scroller.
    function inSight(item) {
        const top = item.mapToItem(view.flickable, 0, 0).y;
        return top >= 0 && top + item.height <= view.flickable.height;
    }

    // Down and Up go from one stop to the next; the page keys scroll, and
    // a stop that left the window hands the keyboard to the first one in
    // sight, from the top after Page Down and from the bottom after Page
    // Up. Any other key is left to the window.
    function keyPressed(event, item) {
        const list = view.stops();
        const at = list.indexOf(item);
        if (event.key === Qt.Key_Down || event.key === Qt.Key_Up) {
            const next = at + (event.key === Qt.Key_Down ? 1 : -1);
            if (next >= 0 && next < list.length)
                list[next].forceActiveFocus(Qt.TabFocusReason);
            event.accepted = true;
            return;
        }
        if (!scrollKeys.scroll(event.key))
            return;
        event.accepted = true;
        if (view.inSight(item))
            return;
        const ordered = event.key === Qt.Key_PageUp ? list.slice().reverse() : list;
        const shown = ordered.find(stop => view.inSight(stop));
        if (shown !== undefined)
            shown.forceActiveFocus(Qt.OtherFocusReason);
    }

    ScrollKeys {
        id: scrollKeys

        flickable: view.flickable
    }

    // What the page says when it has no list to show.
    function status() {
        switch (view.page.state) {
        case NoticesPage.Reading:
            return qsTr("Reading this computer's record of password tries.");
        case NoticesPage.Unreadable:
            return qsTr("Family cannot read this computer's record of password tries from your account. An account in the wheel group can.");
        case NoticesPage.Failed:
            return qsTr("Family could not read this computer's record of password tries. The reason is in the system log. In a terminal: journalctl -t cairn-guardian");
        }
        if (view.page.notices.length === 0)
            return qsTr("No wrong passwords in the last %1 days.").arg(view.page.days);
        return "";
    }

    Text {
        objectName: "noticesHeading"
        width: view.width
        wrapMode: Text.WordWrap
        textFormat: Text.PlainText
        text: qsTr("Wrong passwords in the last %1 days").arg(view.page.days)
        font.family: Tokens.fontFamily
        font.pixelSize: Tokens.headingSize
        font.weight: Tokens.weightBold
        lineHeight: Tokens.headingLineHeight
        color: Tokens.text
        Accessible.role: Accessible.Heading
        Accessible.name: text
    }

    Text {
        objectName: "noticesStatus"
        width: view.width
        visible: text !== ""
        wrapMode: Text.WordWrap
        textFormat: Text.PlainText
        text: view.status()
        font.family: Tokens.fontFamily
        font.pixelSize: Tokens.bodySize
        lineHeight: Tokens.bodyLineHeight
        color: Tokens.text
        Accessible.role: Accessible.StaticText
        Accessible.name: text
    }

    Text {
        objectName: "recordStart"
        width: view.width
        visible: text !== ""
        wrapMode: Text.WordWrap
        textFormat: Text.PlainText
        text: view.page.state === NoticesPage.Ready ? view.page.recordStart : ""
        font.family: Tokens.fontFamily
        font.pixelSize: Tokens.bodySize
        lineHeight: Tokens.bodyLineHeight
        color: Tokens.paperLabel
        Accessible.role: Accessible.StaticText
        Accessible.name: text
    }

    Repeater {
        id: rows

        objectName: "noticeRows"
        model: view.page.state === NoticesPage.Ready ? view.page.notices : []

        Rectangle {
            id: card

            required property string modelData

            objectName: "noticeCard"
            width: view.width
            height: words.implicitHeight + Tokens.bodySize * 2
            radius: Tokens.radiusSm
            color: Tokens.card
            border.color: Tokens.line
            border.width: Tokens.strokeHairline
            activeFocusOnTab: true
            Accessible.role: Accessible.StaticText
            Accessible.name: card.modelData
            Accessible.focusable: true
            Keys.onPressed: event => view.keyPressed(event, card)

            Rectangle {
                anchors.fill: parent
                anchors.margins: -Tokens.focusOffset - Tokens.focusWidth
                radius: card.radius + Tokens.focusOffset + Tokens.focusWidth
                color: Qt.alpha(Tokens.focus, 0)
                border.width: Tokens.focusWidth
                border.color: Tokens.focus
                visible: card.activeFocus
                Accessible.ignored: true
            }

            Text {
                id: words

                objectName: "noticeText"
                x: Tokens.bodySize
                y: Tokens.bodySize
                width: parent.width - Tokens.bodySize * 2
                wrapMode: Text.WordWrap
                textFormat: Text.PlainText
                text: card.modelData
                font.family: Tokens.fontFamily
                font.pixelSize: Tokens.bodySize
                lineHeight: Tokens.bodyLineHeight
                color: Tokens.text
                Accessible.ignored: true
            }
        }
    }

    Text {
        objectName: "notShown"
        width: view.width
        visible: text !== ""
        wrapMode: Text.WordWrap
        textFormat: Text.PlainText
        text: view.page.state === NoticesPage.Ready ? view.page.notShown : ""
        font.family: Tokens.fontFamily
        font.pixelSize: Tokens.bodySize
        lineHeight: Tokens.bodyLineHeight
        color: Tokens.paperLabel
        Accessible.role: Accessible.StaticText
        Accessible.name: text
    }

    Text {
        id: footer

        objectName: "noticesFooter"
        width: view.width
        visible: view.page.state !== NoticesPage.Reading
        wrapMode: Text.WordWrap
        textFormat: Text.PlainText
        text: view.page.footer
        font.family: Tokens.fontFamily
        font.pixelSize: Tokens.bodySize
        lineHeight: Tokens.bodyLineHeight
        color: Tokens.paperLabel
        activeFocusOnTab: true
        Accessible.role: Accessible.StaticText
        Accessible.name: text
        Accessible.focusable: true
        Keys.onPressed: event => view.keyPressed(event, footer)

        Rectangle {
            anchors.fill: parent
            anchors.margins: -Tokens.focusOffset - Tokens.focusWidth
            radius: Tokens.radiusSm + Tokens.focusOffset + Tokens.focusWidth
            color: Qt.alpha(Tokens.focus, 0)
            border.width: Tokens.focusWidth
            border.color: Tokens.focus
            visible: footer.activeFocus
            Accessible.ignored: true
        }
    }
}
