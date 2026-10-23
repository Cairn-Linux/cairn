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

    // The page scrolls when there are more children than fit.
    Flickable {
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
        }
    }
}
