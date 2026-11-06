// SPDX-License-Identifier: Apache-2.0
pragma ComponentBehavior: Bound
import QtQuick
import Cairn.Brand
import Cairn.Family

// The Notices page (ADR-0054): wrong passwords typed on this computer in
// the last 30 days, for every account, as the journal still has them. Each
// notice is plain text: the names in it are accounts' own names, and
// nothing in it is ever read as markup.
Column {
    id: view

    required property NoticesPage page

    spacing: Tokens.bodySize

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
        objectName: "noticeRows"
        model: view.page.state === NoticesPage.Ready ? view.page.notices : []

        Rectangle {
            id: card

            required property string modelData

            width: view.width
            height: words.implicitHeight + Tokens.bodySize * 2
            radius: Tokens.radiusSm
            color: Tokens.card
            border.color: Tokens.line
            border.width: Tokens.strokeHairline
            Accessible.role: Accessible.StaticText
            Accessible.name: card.modelData

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
        Accessible.role: Accessible.StaticText
        Accessible.name: text
    }
}
