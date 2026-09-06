// SPDX-License-Identifier: Apache-2.0
pragma ComponentBehavior: Bound
import QtQuick
import Cairn.Brand
import Cairn.Launcher
import Footpath

Window {
    id: window

    // Set from the command line (--manifest). Empty means the built-in tiles.
    property string manifestPath: ""
    // The Terminal tile is open; the tiles are hidden behind it.
    property bool terminalOpen: false

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

    // Windows of other programs, from the compositor. Nothing arrives on
    // platforms without the protocol; the launcher then only watches processes.
    ForeignWindowList {
        id: windows

        onWindowOpened: (identifier, appId, title) => launcher.windowOpened(identifier, appId, title)
        onWindowClosed: identifier => launcher.windowClosed(identifier)
    }

    AppLauncher {
        id: launcher

        objectName: "launcher"
        ownAppId: windows.ownAppId
    }

    // Footpath, the terminal (ADR-0014). The tiles are its doors; open there
    // comes back here as a launch request and goes the same way a tile does.
    TerminalSession {
        id: session

        objectName: "terminalSession"
        onLaunchRequested: (title, exec) => launcher.launch(title, exec)
        onLeft: window.closeTerminal()
    }

    DoorsFromTiles {
        tiles: tiles
        session: session
    }

    function openTerminal() {
        session.reset();
        terminalOpen = true;
        terminalScreen.takeFocus();
    }

    function closeTerminal() {
        terminalOpen = false;
        focusTiles();
    }

    function focusTiles() {
        grid.forceActiveFocus();
        if (grid.currentItem)
            grid.currentItem.forceActiveFocus();
    }

    // The window onto the tiles (ADR-0015): two whole rows, and when there are
    // more, the top of the third, so a child sees there is more. The grid is
    // as tall as all its rows and slides under the window by whole rows.
    Item {
        id: tileWindow

        objectName: "tileWindow"
        // Room for the focus ring around the outermost tiles, inside the clip.
        readonly property real ring: Tokens.focusOffset + Tokens.focusWidth

        anchors.fill: parent
        anchors.margins: Tokens.headingSize - ring
        clip: true
        visible: !grownUp.visible && !window.terminalOpen

        GridScroller {
            id: scroller

            objectName: "gridScroller"
            columns: 3
            visibleRows: 2
            count: grid.count
            currentIndex: grid.currentIndex
        }

        GridView {
            id: grid

            objectName: "tileGrid"
            readonly property real windowHeight: tileWindow.height - 2 * tileWindow.ring

            x: tileWindow.ring
            width: parent.width - 2 * tileWindow.ring
            height: cellHeight * Math.max(1, scroller.rows)
            y: tileWindow.ring - scroller.firstRow * cellHeight
            cellWidth: width / scroller.columns
            cellHeight: scroller.scrolls ? windowHeight / (scroller.visibleRows + 0.5) : windowHeight / scroller.visibleRows
            interactive: false
            focus: tileWindow.visible
            keyNavigationEnabled: true
            keyNavigationWraps: true
            currentIndex: 0
            model: tiles

            // The one motion that earns its place: the rows moving.
            Behavior on y {
                NumberAnimation {
                    duration: Tokens.motionRow
                }
            }

            Keys.onTabPressed: event => {
                const step = (event.modifiers & Qt.ShiftModifier) ? count - 1 : 1;
                currentIndex = (currentIndex + step) % count;
                event.accepted = true;
            }
            Keys.onBacktabPressed: event => {
                currentIndex = (currentIndex + count - 1) % count;
                event.accepted = true;
            }

            WheelHandler {
                onWheel: event => grid.currentIndex = scroller.indexAfterWheel(event.angleDelta.y)
            }

            delegate: Tile {
                required property int index
                required property list<string> exec
                required property bool opensTerminal

                width: grid.cellWidth - Tokens.headingSize
                height: grid.cellHeight - Tokens.headingSize
                focus: GridView.isCurrentItem
                onActiveFocusChanged: {
                    if (activeFocus)
                        grid.currentIndex = index;
                }
                onActivated: opensTerminal ? window.openTerminal() : launcher.launch(title, exec)
            }
        }
    }

    Terminal {
        id: terminalScreen

        objectName: "terminalScreen"
        anchors.fill: parent
        visible: window.terminalOpen && !grownUp.visible
        session: session
        onExited: window.closeTerminal()
        // Its look is the frame's: every value from the brand tokens.
        groundColor: Tokens.ink
        textColor: Tokens.sand
        hintColor: Tokens.sky
        folderColor: Tokens.sky
        noteColor: Tokens.paper
        makeColor: Tokens.make
        practiceColor: Tokens.practice
        machineColor: Tokens.fjord
        fontFamily: Tokens.fontFamilyMono
        fontSize: Tokens.terminalSize
        lineHeight: Tokens.terminalLineHeight
        margin: Tokens.headingSize
        chipRadius: Tokens.radiusSm / 2
    }

    GrownUpScreen {
        id: grownUp

        objectName: "grownUpScreen"
        anchors.fill: parent
        visible: launcher.needsGrownUp
        appTitle: launcher.title
        openedOnItsOwn: launcher.state === AppLauncher.Interrupted
        onDismissed: launcher.dismiss()
        onVisibleChanged: {
            if (visible)
                return;
            if (window.terminalOpen)
                terminalScreen.takeFocus();
            else
                window.focusTiles();
        }
    }
}
