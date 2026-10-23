// SPDX-License-Identifier: Apache-2.0
import QtQuick
import Cairn.Brand
import Cairn.Family

// The Guardian tool, "Family" in the menu (ADR-0040). So far it has one
// page, Children. Later pages, such as Games, go beside it, with a way to
// move between them above the page.
Window {
    id: window

    required property ChildrenPage page

    readonly property int margin: Tokens.headingSize

    title: qsTr("Family")
    width: Tokens.headingSize * 26
    height: Tokens.headingSize * 34
    minimumWidth: Tokens.headingSize * 18
    minimumHeight: Tokens.headingSize * 12
    color: Tokens.ground
    visible: true

    // Scrolls just enough that the item shows whole, with a little room
    // around it; its top wins when it is taller than the window.
    function showItem(item) {
        let inPage = item;
        while (inPage && inPage !== childrenView)
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
        const lowest = Math.max(0, flickable.contentHeight - flickable.height);
        flickable.contentY = Math.max(0, Math.min(y, lowest));
    }

    // The keyboard never lands on something out of sight.
    onActiveFocusItemChanged: window.showItem(window.activeFocusItem)

    // The page scrolls when there are more children than fit.
    Flickable {
        id: flickable

        objectName: "scroller"
        anchors.fill: parent
        contentWidth: width
        contentHeight: childrenView.implicitHeight + window.margin * 2
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        ChildrenView {
            id: childrenView

            objectName: "childrenView"
            x: window.margin
            y: window.margin
            width: window.width - window.margin * 2
            page: window.page

            // Where focus went, and what the change did, which wins when
            // the window is too short for both.
            onOutcomeShown: {
                childrenView.forceLayout();
                window.showItem(window.activeFocusItem);
                window.showItem(childrenView.outcomeBox);
            }
        }
    }

    // Where the window is on the page, when the page is longer than it.
    Rectangle {
        objectName: "scrollIndicator"
        visible: flickable.visibleArea.heightRatio < 1
        x: window.width - width - Tokens.focusOffset
        y: flickable.visibleArea.yPosition * window.height
        width: Tokens.focusWidth * 2
        height: flickable.visibleArea.heightRatio * window.height
        radius: Tokens.radiusPill
        color: Tokens.machine
        Accessible.ignored: true
    }
}
