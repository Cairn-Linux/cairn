// SPDX-License-Identifier: Apache-2.0
pragma ComponentBehavior: Bound
import QtQuick
import Cairn.Brand
import Cairn.Launcher

Window {
    id: window

    // Set from the command line (--manifest). Empty means the built-in tiles.
    property string manifestPath: ""

    title: qsTr("Cairn")
    visibility: Window.Windowed
    color: Tokens.ground
    width: minimumWidth
    height: minimumHeight
    minimumWidth: Tokens.displaySize * 5 * 3 + Tokens.headingSize * 2
    minimumHeight: Tokens.displaySize * 4 * 2 + Tokens.headingSize * 2

    TileModel {
        id: tiles

        manifestPath: window.manifestPath
    }

    AppLauncher {
        id: launcher

        objectName: "launcher"
    }

    GridView {
        id: grid

        objectName: "tileGrid"
        anchors.fill: parent
        anchors.margins: Tokens.headingSize
        cellWidth: width / 3
        cellHeight: height / 2
        interactive: false
        focus: !grownUp.visible
        visible: !grownUp.visible
        keyNavigationEnabled: true
        keyNavigationWraps: true
        currentIndex: 0
        model: tiles

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
            required property list<string> exec

            width: grid.cellWidth - Tokens.headingSize
            height: grid.cellHeight - Tokens.headingSize
            focus: GridView.isCurrentItem
            onActiveFocusChanged: {
                if (activeFocus)
                    grid.currentIndex = index;
            }
            onActivated: launcher.launch(title, exec)
        }
    }

    GrownUpScreen {
        id: grownUp

        objectName: "grownUpScreen"
        anchors.fill: parent
        visible: launcher.state === AppLauncher.Failed
        appTitle: launcher.title
        onDismissed: {
            launcher.dismiss();
            grid.forceActiveFocus();
            if (grid.currentItem)
                grid.currentItem.forceActiveFocus();
        }
    }
}
