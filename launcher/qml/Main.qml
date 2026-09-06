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

    GridView {
        id: grid

        objectName: "tileGrid"
        anchors.fill: parent
        anchors.margins: Tokens.headingSize
        cellWidth: width / 3
        cellHeight: height / 2
        interactive: false
        focus: !grownUp.visible && !window.terminalOpen
        visible: !grownUp.visible && !window.terminalOpen
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
