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
    // Set from the command line (--games): the games file provisioning makes
    // from the shared Steam library (ADR-0033). Empty means no game tiles.
    property string gamesPath: ""
    // The Terminal tile is open; the tiles are hidden behind it.
    property bool terminalOpen: false
    // Set from the command line (--log-out). Empty means no Log out.
    property string logOutProgram: ""
    // Set from the command line (--scope-apps): every program in its own scope.
    property bool scopeApps: false
    // The file cairn-give-up writes, set by main.cpp. Empty watches nothing.
    property string giveUpFile: ""
    // The Steam client was started at login (ADR-0027, ADR-0029).
    property bool steamStartedAtLogin: false
    // The child chose Log out and is being asked whether they meant it.
    property bool logOutAsked: false
    // UPower's name on the system bus, set by main.cpp. Empty reads no
    // battery, as in the tests.
    property string batteryService: ""

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
        gamesPath: window.gamesPath
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
        steamStartedAtLogin: window.steamStartedAtLogin
        // Steam's updater's windows are kept out of the child's reach (#183).
        onWindowToHide: identifier => windows.minimize(identifier)
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

    // The battery, as UPower reads it and decides when it is low (ADR-0034).
    UPowerDevice {
        id: upower

        objectName: "upower"
        service: window.batteryService
    }

    BatteryLook {
        id: batteryLook

        objectName: "batteryLook"
        isPresent: upower.isPresent
        percentage: upower.percentage
        state: upower.state
        warningLevel: upower.warningLevel
        type: upower.type
    }

    // The child's leave key (Super+Q, ADR-0019) asks the window in front to
    // close. When that is the launcher it means one step back, never the end
    // of the child's session; CloseRequest decides which.
    onClosing: close => {
        const answer = closeRequest.answer(window.visibility === Window.FullScreen, window.terminalOpen, window.logOutAsked, logOut.state, launcher.state);
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
        logOut.dismiss();
        logOutAsked = false;
        logOutTile.forceActiveFocus();
    }

    // While the program runs, the question says it is logging out, and
    // pressing again starts nothing more (LogOut). If the program cannot
    // start, the child is not left on a question that does nothing: they go
    // back to where they chose Log out.
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
    // child's own way to end their turn (ADR-0021) at the right, and the
    // battery at the left (ADR-0034). Up from the top row of tiles reaches
    // Log out; Down goes back. The battery has nothing to press.
    Item {
        id: header

        objectName: "header"
        readonly property bool used: logOut.available || batteryLook.shown

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: Tokens.headingSize
        // Level with the right edge of the tiles, which stop short of the
        // window's by the gap between tiles.
        anchors.rightMargin: Tokens.headingSize * 2
        height: used ? Tokens.headingSize * 2 : 0
        visible: used && tileWindow.visible

        Battery {
            objectName: "battery"
            anchors.left: parent.left
            anchors.right: logOut.available ? logOutTile.left : parent.right
            anchors.rightMargin: logOut.available ? Tokens.headingSize : 0
            height: parent.height
            visible: batteryLook.shown
            level: batteryLook.level
            supply: batteryLook.supply
            onCharger: batteryLook.onCharger
            warning: batteryLook.warning
        }

        Tile {
            id: logOutTile

            objectName: "logOutTile"
            visible: logOut.available
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

        anchors.top: header.used ? header.bottom : parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: Tokens.headingSize - ring
        clip: true
        visible: !grownUp.visible && !starting.visible && !window.terminalOpen && !window.logOutAsked

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

            // Motion that tells the child something (ADR-0028): the rows
            // moving into view.
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
                required picture

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
        visible: window.terminalOpen && !grownUp.visible && !starting.visible
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
        leaving: logOut.state === LogOut.Leaving
        didNotFinish: logOut.state === LogOut.DidNotFinish
        onStayed: window.stayLoggedIn()
        onConfirmed: window.logOutNow()
    }

    // From the press until the app's window is up (ADR-0028), in front of
    // the tiles or the Terminal, so a child sees the press did something.
    StartingScreen {
        id: starting

        objectName: "startingScreen"
        anchors.fill: parent
        visible: launcher.state === AppLauncher.Starting
        appTitle: launcher.title
        gettingReady: launcher.gettingReady
        onVisibleChanged: {
            if (visible || grownUp.visible)
                return;
            if (window.terminalOpen)
                terminalScreen.takeFocus();
            else
                window.focusTiles();
        }
    }

    GrownUpScreen {
        id: grownUp

        objectName: "grownUpScreen"
        anchors.fill: parent
        visible: launcher.needsGrownUp || launcher.state === AppLauncher.ComingSoon
        appTitle: launcher.title
        openedOnItsOwn: launcher.state === AppLauncher.Interrupted
        comingSoon: launcher.state === AppLauncher.ComingSoon
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
