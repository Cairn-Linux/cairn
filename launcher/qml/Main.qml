// SPDX-License-Identifier: Apache-2.0
pragma ComponentBehavior: Bound
import QtQuick
import Cairn.Brand
import Cairn.Launcher

Window {
    id: window

    title: qsTr("Cairn")
    visibility: Window.Windowed
    color: Tokens.ground
    width: minimumWidth
    height: minimumHeight
    minimumWidth: Tokens.displaySize * 5 * 3 + Tokens.headingSize * 2
    minimumHeight: Tokens.displaySize * 4 * 2 + Tokens.headingSize * 2

    GridView {
        id: grid

        objectName: "tileGrid"
        anchors.fill: parent
        anchors.margins: Tokens.headingSize
        cellWidth: width / 3
        cellHeight: height / 2
        interactive: false
        focus: true
        keyNavigationEnabled: true
        keyNavigationWraps: true
        currentIndex: 0
        model: TileModel {}

        Keys.onTabPressed: event => {
            const step = (event.modifiers & Qt.ShiftModifier) ? count - 1 : 1;
            currentIndex = (currentIndex + step) % count;
            event.accepted = true;
        }
        Keys.onBacktabPressed: event => {
            currentIndex = (currentIndex + count - 1) % count;
            event.accepted = true;
        }

        delegate: Tile {
            required property int index

            width: grid.cellWidth - Tokens.headingSize
            height: grid.cellHeight - Tokens.headingSize
            focus: GridView.isCurrentItem
            onActiveFocusChanged: {
                if (activeFocus)
                    grid.currentIndex = index;
            }
        }
    }
}
