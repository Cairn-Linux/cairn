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
    // Set from the command line (--log-out). Empty means no Log out.
    property string logOutProgram: ""
    // Set from the command line (--scope-apps): every program in its own scope.
    property bool scopeApps: false
    // The file cairn-give-up writes, set by main.cpp. Empty watches nothing.
    property string giveUpFile: ""
    // The child chose Log out and is being asked whether they meant it.
    property bool logOutAsked: false

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

        onWindowOpened: (identifier, appId, title, hidden, belongsTo) => launcher.windowOpened(identifier, appId, title, hidden, belongsTo)
        onWindowChanged: (identifier, hidden, belongsTo) => launcher.windowChanged(identifier, hidden, belongsTo)
        onWindowClosed: identifier => launcher.windowClosed(identifier)
    }

    // The grown-up's give-up key always brings the tiles back (ADR-0026).
    GiveUpWatch {
        path: window.giveUpFile
        onPressed: launcher.giveUp()
    }

    AppLauncher {
        id: launcher

        objectName: "launcher"
        ownAppId: windows.ownAppId
        scoped: window.scopeApps
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

    CloseRequest {
        id: closeRequest
    }

    LogOut {
        id: logOut

        objectName: "logOut"
        program: window.logOutProgram
    }

    // The child's leave key (Super+Q, ADR-0019) asks the window in front to
    // close. When that is the launcher it means one step back, never the end
    // of the child's session; CloseRequest decides which.
    onClosing: close => {
        const answer = closeRequest.answer(window.visibility === Window.FullScreen, window.terminalOpen, window.logOutAsked, launcher.state);
        close.accepted = answer === CloseRequest.Close;
        if (answer === CloseRequest.LeaveTerminal)
            window.closeTerminal();
        else if (answer === CloseRequest.Dismiss)
            launcher.dismiss();
        else if (answer === CloseRequest.StayLoggedIn)
            window.stayLoggedIn();
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

    function askToLogOut() {
        logOutAsked = true;
    }

    function stayLoggedIn() {
        logOutAsked = false;
        logOutTile.forceActiveFocus();
    }

    // If the program cannot start, the child is not left on a question that
    // does nothing: they go back to where they chose Log out.
    function logOutNow() {
        if (!logOut.start())
            stayLoggedIn();
    }

    function focusTiles() {
        grid.forceActiveFocus();
        if (grid.currentItem)
            grid.currentItem.forceActiveFocus();
    }

    // Above the tiles, where it is always in view and never scrolls away: the
    // child's own way to end their turn (ADR-0021). Up from the top row of
    // tiles reaches it; Down goes back.
    Item {
        id: header

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: Tokens.headingSize
        // Level with the right edge of the tiles, which stop short of the
        // window's by the gap between tiles.
        anchors.rightMargin: Tokens.headingSize * 2
        height: logOut.available ? Tokens.headingSize * 2 : 0
        visible: logOut.available && tileWindow.visible

        Tile {
            id: logOutTile

            objectName: "logOutTile"
            anchors.right: parent.right
            width: Tokens.displaySize * 4
            height: parent.height
            title: qsTr("Log out")
            kind: TileModel.Machine
            accessibleName: qsTr("Log out")
            Keys.onDownPressed: window.focusTiles()
            onActivated: window.askToLogOut()
        }
    }

    // The window onto the tiles (ADR-0015): two whole rows, and when there are
    // more, the top of the third, so a child sees there is more. The grid is
    // as tall as all its rows and slides under the window by whole rows.
    Item {
        id: tileWindow

        objectName: "tileWindow"
        // Room for the focus ring around the outermost tiles, inside the clip.
        readonly property real ring: Tokens.focusOffset + Tokens.focusWidth

        anchors.top: logOut.available ? header.bottom : parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: Tokens.headingSize - ring
        clip: true
        visible: !grownUp.visible && !window.terminalOpen && !window.logOutAsked

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

            // From the top row, Up goes to Log out when there is one;
            // otherwise the grid wraps as before.
            Keys.onUpPressed: event => {
                if (logOut.available && currentIndex < scroller.columns)
                    logOutTile.forceActiveFocus();
                else
                    event.accepted = false;
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
        gamesColor: Tokens.games
        machineColor: Tokens.fjord
        fontFamily: Tokens.fontFamilyMono
        fontSize: Tokens.terminalSize
        lineHeight: Tokens.terminalLineHeight
        margin: Tokens.headingSize
        chipRadius: Tokens.radiusSm / 2
        chipLineWidth: Tokens.strokeHairline
    }

    LogOutScreen {
        id: logOutScreen

        objectName: "logOutScreen"
        anchors.fill: parent
        visible: window.logOutAsked && !grownUp.visible
        onStayed: window.stayLoggedIn()
        onConfirmed: window.logOutNow()
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
