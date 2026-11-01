// SPDX-License-Identifier: Apache-2.0
import QtQuick
import Cairn.Brand
import Cairn.Family

// The Guardian tool, "Family" in the menu (ADR-0040). Three pages,
// Children, Games and Your account (ADR-0043), with a switch between them
// above the page, which stays in place while the page scrolls.
Window {
    id: window

    required property ChildrenPage page
    required property GamesPage gamesPage
    required property AccountPage accountPage

    // The page that shows: "children", "games" or "account".
    property string showing: "children"
    readonly property Item shownView: window.showing === "games" ? gamesView : window.showing === "account" ? accountView : childrenView

    // The item the keyboard is on, kept while another window has it.
    property Item keyboardItem: null

    readonly property int margin: Tokens.headingSize

    // A game card being turned, and where it was in the window before the
    // change, until the page is laid out again (#219).
    property Item heldCard: null
    property real heldCardY: 0

    title: qsTr("Family")
    width: Tokens.headingSize * 26
    height: Tokens.headingSize * 34
    minimumWidth: Tokens.headingSize * 18
    minimumHeight: Tokens.headingSize * 12
    color: Tokens.ground
    visible: true

    // Shows a page from its top. The Games page reads its files again, so
    // a child just added or a game just listed is there.
    function show(name) {
        if (window.showing === name)
            return;
        if (name === "games")
            window.gamesPage.reload();
        if (name === "account")
            window.accountPage.reload();
        window.showing = name;
        // A card turned just before is on the page left behind.
        window.heldCard = null;
        flickable.contentY = 0;
    }

    // Scrolls just enough that the item shows whole, with a little room
    // around it; its top wins when it is taller than the window. Only an
    // item on the page scrolls; the switch above it is always in sight.
    function showItem(item) {
        let inPage = item;
        while (inPage && inPage !== flickable.contentItem)
            inPage = inPage.parent;
        if (!inPage || !item.visible)
            return;
        const room = Tokens.bodySize;
        const top = item.mapToItem(flickable.contentItem, 0, 0).y - room;
        const bottom = top + item.height + room * 2;
        let y = flickable.contentY;
        if (bottom > y + flickable.height)
            y = bottom - flickable.height;
        if (top < y)
            y = top;
        window.scrollTo(y);
    }

    // Scrolls the page to y, or as far as it goes.
    function scrollTo(y) {
        const lowest = Math.max(0, flickable.contentHeight - flickable.height);
        flickable.contentY = Math.max(0, Math.min(y, lowest));
    }

    // The keyboard never lands on something out of sight: each time it
    // moves to another item, the page scrolls to show it. A control that
    // waits while a change runs hands the keyboard to the window, so its
    // return is a move too. While another window is in front, such as KDE
    // Wallet's box, nothing here has the keyboard; when Family comes back
    // to the front, the keyboard comes back to the item it was on, and the
    // page stays where it was. A button may have scrolled it on purpose,
    // to show what the button changed.
    onActiveFocusItemChanged: {
        const item = window.activeFocusItem;
        if (item === null || item === window.keyboardItem)
            return;
        window.keyboardItem = item;
        window.showItem(item);
    }

    // In each frame the window first lays out what changed, then says so
    // here, then draws. A held card has moved by as much as the page grew
    // or shrank above it, and by any scroll its tap's focus made, so the
    // page scrolls by as much, and the card is drawn where it was (#219).
    onAfterAnimating: {
        const card = window.heldCard;
        if (!card)
            return;
        window.heldCard = null;
        window.scrollTo(flickable.contentY + card.mapToItem(window.contentItem, 0, 0).y - window.heldCardY);
    }

    // A child added on the Children page is on the Games page too.
    Connections {
        target: window.page

        function onListedChildrenChanged() {
            window.gamesPage.reload();
        }
    }

    Rectangle {
        id: switchBar

        width: window.width
        height: tabs.height + window.margin
        color: Tokens.ground

        Row {
            id: tabs

            objectName: "pageSwitch"
            x: window.margin
            y: window.margin / 2
            spacing: Tokens.bodySize / 2

            Accessible.role: Accessible.PageTabList
            Accessible.name: qsTr("Pages")

            // The keyboard starts here, on the button of the page the
            // window opens on, so its ring shows and Tab and Shift+Tab go
            // on from it (#242).
            TextButton {
                id: childrenTab

                objectName: "childrenTab"
                focus: true
                text: qsTr("Children")
                quiet: window.showing !== "children"
                Accessible.role: Accessible.PageTab
                Accessible.checkable: true
                Accessible.checked: window.showing === "children"
                KeyNavigation.right: gamesTab
                onActivated: window.show("children")
            }

            TextButton {
                id: gamesTab

                objectName: "gamesTab"
                text: qsTr("Games")
                quiet: window.showing !== "games"
                Accessible.role: Accessible.PageTab
                Accessible.checkable: true
                Accessible.checked: window.showing === "games"
                KeyNavigation.left: childrenTab
                KeyNavigation.right: accountTab
                onActivated: window.show("games")
            }

            TextButton {
                id: accountTab

                objectName: "accountTab"
                text: qsTr("Your account")
                quiet: window.showing !== "account"
                Accessible.role: Accessible.PageTab
                Accessible.checkable: true
                Accessible.checked: window.showing === "account"
                KeyNavigation.left: gamesTab
                onActivated: window.show("account")
            }
        }

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: Tokens.strokeHairline
            color: Tokens.line
            Accessible.ignored: true
        }
    }

    // The page scrolls when there is more on it than fits.
    Flickable {
        id: flickable

        objectName: "scroller"
        y: switchBar.height
        width: window.width
        height: window.height - switchBar.height
        contentWidth: width
        contentHeight: window.shownView.implicitHeight + window.margin * 2
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        ChildrenView {
            id: childrenView

            objectName: "childrenView"
            x: window.margin
            y: window.margin
            width: window.width - window.margin * 2
            visible: window.showing === "children"
            page: window.page

            // Where focus went, and what the change did, which wins when
            // the window is too short for both.
            onOutcomeShown: {
                childrenView.forceLayout();
                window.showItem(window.activeFocusItem);
                window.showItem(childrenView.outcomeBox);
            }
        }

        GamesView {
            id: gamesView

            objectName: "gamesView"
            x: window.margin
            y: window.margin
            width: window.width - window.margin * 2
            visible: window.showing === "games"
            page: window.gamesPage

            // Where the card is before the change, to keep it there.
            onCardTurning: card => {
                window.heldCard = card;
                window.heldCardY = card.mapToItem(window.contentItem, 0, 0).y;
            }
        }

        AccountView {
            id: accountView

            objectName: "accountView"
            x: window.margin
            y: window.margin
            width: window.width - window.margin * 2
            visible: window.showing === "account"
            page: window.accountPage

            // What the change did, after focus has gone back.
            onOutcomeShown: box => {
                accountView.forceLayout();
                Qt.callLater(window.showItem, box);
            }
        }
    }

    // Where the window is on the page, when the page is longer than it.
    ScrollMark {
        objectName: "scrollIndicator"
        flickable: flickable
        x: window.width - width - Tokens.focusOffset
    }

    // A Save's own window, over everything else, the page switch too.
    SaveProgress {
        objectName: "saveProgress"
        anchors.fill: parent
        saving: window.gamesPage.saving
        onClosed: Qt.callLater(gamesView.restoreFocus)
    }
}
