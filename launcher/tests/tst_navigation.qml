// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtTest
import Cairn.Launcher

TestCase {
    id: testCase

    name: "LauncherNavigation"
    when: windowShown
    property Window launcher
    property GridView grid
    property AppLauncher appLauncher
    property Item grownUp

    Component {
        id: launcherComponent

        Main {}
    }

    // Counts state changes so a launch test cannot pass by never launching.
    SignalSpy {
        id: stateSpy

        target: testCase.appLauncher
        signalName: "stateChanged"
    }

    function init() {
        launcher = createTemporaryObject(launcherComponent, testCase, {
            "manifestPath": Qt.resolvedUrl("fixtures/manifest.json")
        });
        verify(launcher !== null);
        grid = findChild(launcher, "tileGrid");
        verify(grid !== null);
        appLauncher = findChild(launcher, "launcher");
        verify(appLauncher !== null);
        appLauncher.settleMilliseconds = 2000;
        appLauncher.launchGraceMilliseconds = 200;
        grownUp = findChild(launcher, "grownUpScreen");
        verify(grownUp !== null);
        stateSpy.clear();
        launcher.requestActivate();
        tryCompare(launcher, "active", true);
        tryCompare(grid, "count", 6);
        tryVerify(() => grid.currentItem !== null);
        compare(grid.currentIndex, 0);
        tryCompare(grid.currentItem, "activeFocus", true);
    }

    function focusTile(index) {
        grid.currentIndex = index;
        tryVerify(() => grid.currentItem !== null);
        grid.currentItem.forceActiveFocus();
        tryCompare(grid.currentItem, "activeFocus", true);
    }

    function test_navigation_data() {
        return [
            {
                tag: "right",
                start: 0,
                key: Qt.Key_Right,
                modifiers: Qt.NoModifier,
                expected: 1
            },
            {
                tag: "down",
                start: 0,
                key: Qt.Key_Down,
                modifiers: Qt.NoModifier,
                expected: 3
            },
            {
                tag: "left",
                start: 1,
                key: Qt.Key_Left,
                modifiers: Qt.NoModifier,
                expected: 0
            },
            {
                tag: "up",
                start: 3,
                key: Qt.Key_Up,
                modifiers: Qt.NoModifier,
                expected: 0
            },
            {
                tag: "right-wrap",
                start: 5,
                key: Qt.Key_Right,
                modifiers: Qt.NoModifier,
                expected: 0
            },
            {
                tag: "left-wrap",
                start: 0,
                key: Qt.Key_Left,
                modifiers: Qt.NoModifier,
                expected: 5
            },
            {
                tag: "down-wrap",
                start: 3,
                key: Qt.Key_Down,
                modifiers: Qt.NoModifier,
                expected: 0
            },
            {
                tag: "up-wrap",
                start: 0,
                key: Qt.Key_Up,
                modifiers: Qt.NoModifier,
                expected: 5
            },
            {
                tag: "tab-wrap",
                start: 5,
                key: Qt.Key_Tab,
                modifiers: Qt.NoModifier,
                expected: 0
            },
            {
                tag: "backtab-wrap",
                start: 0,
                key: Qt.Key_Tab,
                modifiers: Qt.ShiftModifier,
                expected: 5
            },
            {
                tag: "backtab-key-wrap",
                start: 0,
                key: Qt.Key_Backtab,
                modifiers: Qt.ShiftModifier,
                expected: 5
            }
        ];
    }

    function test_navigation(data) {
        focusTile(data.start);
        keyClick(data.key, data.modifiers);
        tryCompare(grid, "currentIndex", data.expected);
        tryCompare(grid.currentItem, "activeFocus", true);
    }

    // A click focuses the tile and launches it; tile 4 quits cleanly and opens
    // no window, so the launcher returns to the tiles when the grace ends and
    // never shows the grown-up screen.
    function test_mouseFocusAndLaunch() {
        const tile = grid.itemAtIndex(4);
        verify(tile !== null);
        mouseClick(tile);
        tryCompare(grid, "currentIndex", 4);
        tryCompare(tile, "activeFocus", true);
        compare(tile.Accessible.role, Accessible.Button);
        verify(tile.Accessible.name.length > 0);
        compare(tile.Accessible.focusable, true);
        verify(stateSpy.count > 0);
        tryCompare(appLauncher, "state", AppLauncher.Idle);
        compare(grownUp.visible, false);
    }

    // Tile 0 quits with an error at once: the grown-up screen appears and takes focus.
    function test_failedLaunchShowsGrownUpScreen_data() {
        return [
            {
                tag: "return",
                key: Qt.Key_Return
            },
            {
                tag: "enter",
                key: Qt.Key_Enter
            },
            {
                tag: "space",
                key: Qt.Key_Space
            }
        ];
    }

    function test_failedLaunchShowsGrownUpScreen(data) {
        focusTile(0);
        keyClick(data.key);
        tryCompare(appLauncher, "state", AppLauncher.Failed);
        tryCompare(grownUp, "visible", true);
        compare(grownUp.appTitle, "Quits badly");
        compare(grid.visible, false);
        const back = findChild(grownUp, "backTile");
        verify(back !== null);
        tryCompare(back, "activeFocus", true);
        verify(back.Accessible.name.length > 0);
    }

    // A tile with nothing set up yet says it is coming soon, with a Back, and
    // asks for no grown-up.
    function test_aTileWithNothingSetUpIsComingSoon() {
        focusTile(3);
        keyClick(Qt.Key_Return);
        tryCompare(appLauncher, "state", AppLauncher.ComingSoon);
        tryCompare(grownUp, "visible", true);
        compare(grownUp.comingSoon, true);
        compare(grownUp.Accessible.name, "Nothing set up is coming soon.");
        compare(appLauncher.needsGrownUp, false);
        const back = findChild(grownUp, "backTile");
        tryCompare(back, "activeFocus", true);
        keyClick(Qt.Key_Escape);
        tryCompare(grownUp, "visible", false);
        compare(appLauncher.state, AppLauncher.Idle);
        tryCompare(grid.currentItem, "activeFocus", true);
    }

    function test_missingProgramShowsGrownUpScreen() {
        focusTile(2);
        keyClick(Qt.Key_Return);
        tryCompare(grownUp, "visible", true);
        compare(grownUp.appTitle, "Missing program");
    }

    function test_nothingSetUpShowsGrownUpScreen() {
        const tile = grid.itemAtIndex(3);
        verify(tile !== null);
        mouseClick(tile);
        tryCompare(grownUp, "visible", true);
        compare(grownUp.appTitle, "Nothing set up");
    }

    // A clean launch that opens no window returns to the tiles when the grace
    // ends; the title proves the tile really launched.
    function test_cleanLaunchReturnsToTiles() {
        focusTile(1);
        keyClick(Qt.Key_Return);
        compare(appLauncher.title, "Quits cleanly");
        tryCompare(appLauncher, "state", AppLauncher.Idle);
        compare(grownUp.visible, false);
        tryCompare(grid.currentItem, "activeFocus", true);
    }

    function test_backReturnsFocusToTheTile_data() {
        return [
            {
                tag: "escape",
                key: Qt.Key_Escape
            },
            {
                tag: "return-on-back",
                key: Qt.Key_Return
            }
        ];
    }

    function test_backReturnsFocusToTheTile(data) {
        focusTile(0);
        keyClick(Qt.Key_Return);
        tryCompare(grownUp, "visible", true);
        keyClick(data.key);
        tryCompare(grownUp, "visible", false);
        compare(appLauncher.state, AppLauncher.Idle);
        compare(grid.currentIndex, 0);
        tryCompare(grid.currentItem, "activeFocus", true);
    }

    // A window that opened on its own, reported as the compositor would report
    // it: the grown-up screen appears with no Back tile and only the window
    // closing ends it.
    function test_windowOnItsOwnShowsGrownUpScreenWithoutBack() {
        appLauncher.windowOpened("w1", "steam", "Steam");
        tryCompare(grownUp, "visible", true);
        compare(appLauncher.state, AppLauncher.Interrupted);
        compare(grownUp.appTitle, "Steam");
        compare(grid.visible, false);
        const back = findChild(grownUp, "backTile");
        verify(back !== null);
        tryCompare(back, "visible", false);
        tryCompare(grownUp, "activeFocus", true);
        keyClick(Qt.Key_Escape);
        compare(grownUp.visible, true);
        appLauncher.windowClosed("w1");
        tryCompare(grownUp, "visible", false);
        compare(appLauncher.state, AppLauncher.Idle);
        tryCompare(grid.currentItem, "activeFocus", true);
    }

    // A window that opens after a launch is the app, not an interruption: the
    // grown-up screen stays away and the launcher returns to the tiles only
    // when that window closes (issue #42).
    function test_aLaunchedWindowIsNotAnInterruption() {
        appLauncher.launchGraceMilliseconds = 5000;
        // Tile 1 exits at once, as steam -applaunch does; the launcher waits
        // for the window rather than returning to the tiles.
        focusTile(1);
        keyClick(Qt.Key_Return);
        tryCompare(appLauncher, "state", AppLauncher.Starting);
        appLauncher.windowOpened("g1", "scummvm", "Putt-Putt Joins the Parade");
        compare(appLauncher.state, AppLauncher.Running);
        compare(grownUp.visible, false);
        compare(grid.visible, true);
        appLauncher.windowClosed("g1");
        compare(appLauncher.state, AppLauncher.Idle);
        wait(300); // let the launching process be reaped.
    }

    function test_ownWindowDoesNotInterrupt() {
        appLauncher.ownAppId = "cairn-launcher";
        appLauncher.windowOpened("w1", "cairn-launcher", "Cairn");
        compare(appLauncher.state, AppLauncher.Idle);
        compare(grownUp.visible, false);
    }

    // The Terminal tile opens the shell inside the window; exit closes it.
    function test_terminalTileOpensTheShellAndExitCloses() {
        const terminal = findChild(launcher, "terminalScreen");
        verify(terminal !== null);
        const input = findChild(launcher, "terminalInput");
        verify(input !== null);
        const tile = grid.itemAtIndex(5);
        verify(tile !== null);
        compare(tile.title, "Terminal");
        focusTile(5);
        keyClick(Qt.Key_Return);
        tryCompare(terminal, "visible", true);
        compare(grid.visible, false);
        tryCompare(input, "activeFocus", true);
        input.text = "exit";
        keyClick(Qt.Key_Return);
        tryCompare(terminal, "visible", false);
        compare(grid.visible, true);
        tryCompare(grid.currentItem, "activeFocus", true);
        compare(stateSpy.count, 0);
    }

    function test_escapeLeavesTheTerminal() {
        const terminal = findChild(launcher, "terminalScreen");
        focusTile(5);
        keyClick(Qt.Key_Return);
        tryCompare(terminal, "visible", true);
        keyClick(Qt.Key_Escape);
        tryCompare(terminal, "visible", false);
        tryCompare(grid.currentItem, "activeFocus", true);
    }

    // In the kiosk the launcher is fullscreen, and a close request (the leave
    // key, ADR-0019) is one step back that never closes it.
    function enterKiosk() {
        launcher.visibility = Window.FullScreen;
        tryCompare(launcher, "visibility", Window.FullScreen);
        launcher.requestActivate();
        tryCompare(launcher, "active", true);
    }

    function test_closeInTheKioskKeepsTheTilesUp() {
        enterKiosk();
        focusTile(4);
        compare(launcher.close(), false);
        compare(launcher.visible, true);
        compare(grid.visible, true);
        compare(grid.currentIndex, 4);
        tryCompare(grid.currentItem, "activeFocus", true);
    }

    function test_closeInTheKioskExitsTheTerminal() {
        enterKiosk();
        const terminal = findChild(launcher, "terminalScreen");
        focusTile(5);
        keyClick(Qt.Key_Return);
        tryCompare(terminal, "visible", true);
        compare(launcher.close(), false);
        tryCompare(terminal, "visible", false);
        compare(launcher.visible, true);
        tryCompare(grid.currentItem, "activeFocus", true);
    }

    function test_closeInTheKioskDismissesAFailedLaunch() {
        enterKiosk();
        focusTile(0);
        keyClick(Qt.Key_Return);
        tryCompare(grownUp, "visible", true);
        compare(launcher.close(), false);
        tryCompare(grownUp, "visible", false);
        compare(appLauncher.state, AppLauncher.Idle);
        tryCompare(grid.currentItem, "activeFocus", true);
    }

    function test_closeInTheKioskKeepsTheGrownUpScreenForAWindowOnItsOwn() {
        enterKiosk();
        appLauncher.windowOpened("w1", "steam", "Steam");
        tryCompare(grownUp, "visible", true);
        compare(launcher.close(), false);
        compare(grownUp.visible, true);
        compare(appLauncher.state, AppLauncher.Interrupted);
        appLauncher.windowClosed("w1");
        tryCompare(grownUp, "visible", false);
    }

    // ---- Log out (ADR-0021) ----

    function giveLogOut(program) {
        launcher.logOutProgram = program;
        tryVerify(() => findChild(launcher, "logOutTile").visible);
    }

    function fixturePath(name) {
        return Qt.resolvedUrl("fixtures/" + name).toString().replace("file://", "");
    }

    // A launcher run without --log-out, as under Plasma, has no Log out, and
    // Up from the top row still wraps as it always did.
    function test_noProgramNoLogOut() {
        compare(findChild(launcher, "logOutTile").visible, false);
        keyClick(Qt.Key_Up);
        verify(grid.currentIndex !== 0);
        tryCompare(grid.currentItem, "activeFocus", true);
    }

    function test_upFromTheTopRowReachesLogOutAndDownComesBack() {
        giveLogOut(fixturePath("log-out"));
        const logOutTile = findChild(launcher, "logOutTile");
        focusTile(1);
        keyClick(Qt.Key_Up);
        tryCompare(logOutTile, "activeFocus", true);
        keyClick(Qt.Key_Down);
        tryCompare(grid.currentItem, "activeFocus", true);
        compare(grid.currentIndex, 1);
        // From the second row, Up is an ordinary move within the tiles.
        focusTile(4);
        keyClick(Qt.Key_Up);
        compare(grid.currentIndex, 1);
        tryCompare(grid.currentItem, "activeFocus", true);
    }

    // Log out asks first, and Back is where the focus starts, so Enter
    // pressed twice by accident keeps the child where they were.
    function test_logOutAsksFirstAndBackStays() {
        giveLogOut(fixturePath("log-out"));
        const logOutTile = findChild(launcher, "logOutTile");
        const screen = findChild(launcher, "logOutScreen");
        mouseClick(logOutTile);
        tryCompare(screen, "visible", true);
        compare(grid.visible, false);
        tryCompare(findChild(screen, "stayTile"), "activeFocus", true);
        keyClick(Qt.Key_Return);
        tryCompare(screen, "visible", false);
        compare(launcher.logOutAsked, false);
        tryCompare(logOutTile, "activeFocus", true);
    }

    function test_escapeStays() {
        giveLogOut(fixturePath("log-out"));
        const screen = findChild(launcher, "logOutScreen");
        mouseClick(findChild(launcher, "logOutTile"));
        tryCompare(screen, "visible", true);
        keyClick(Qt.Key_Escape);
        tryCompare(screen, "visible", false);
        compare(grid.visible, true);
    }

    function test_closeInTheKioskStays() {
        enterKiosk();
        giveLogOut(fixturePath("log-out"));
        const screen = findChild(launcher, "logOutScreen");
        mouseClick(findChild(launcher, "logOutTile"));
        tryCompare(screen, "visible", true);
        compare(launcher.close(), false);
        tryCompare(screen, "visible", false);
        compare(launcher.visible, true);
    }

    // Confirming starts the program; the session ends from there, so the
    // question stays up until it does.
    function test_confirmStartsTheProgram() {
        giveLogOut(fixturePath("log-out"));
        const screen = findChild(launcher, "logOutScreen");
        mouseClick(findChild(launcher, "logOutTile"));
        tryCompare(screen, "visible", true);
        keyClick(Qt.Key_Right);
        tryCompare(findChild(screen, "confirmLogOutTile"), "activeFocus", true);
        keyClick(Qt.Key_Return);
        compare(launcher.logOutAsked, true);
        compare(screen.visible, true);
    }

    // A program that cannot start does not leave the child on a question
    // that does nothing.
    function test_aProgramThatCannotStartGoesBack() {
        giveLogOut("/nonexistent/cairn-log-out");
        const screen = findChild(launcher, "logOutScreen");
        mouseClick(findChild(launcher, "logOutTile"));
        tryCompare(screen, "visible", true);
        mouseClick(findChild(screen, "confirmLogOutTile"));
        tryCompare(screen, "visible", false);
        compare(launcher.logOutAsked, false);
        tryCompare(findChild(launcher, "logOutTile"), "activeFocus", true);
    }

    // Under Plasma the launcher is an ordinary window, and closing it closes it.
    function test_closeWhenWindowedClosesTheWindow() {
        compare(launcher.visibility, Window.Windowed);
        compare(launcher.close(), true);
        compare(launcher.visible, false);
    }

    // open from the shell goes through the same launcher; a bad launch shows
    // the grown-up screen and Back returns to the terminal, not the tiles.
    function test_openFromTheTerminalLaunchesThroughTheLauncher() {
        const terminal = findChild(launcher, "terminalScreen");
        const input = findChild(launcher, "terminalInput");
        const session = findChild(launcher, "terminalSession");
        verify(session !== null);
        focusTile(5);
        keyClick(Qt.Key_Return);
        tryCompare(terminal, "visible", true);
        input.text = "cd make";
        keyClick(Qt.Key_Return);
        tryCompare(session, "location", "/make");
        input.text = "open quits-badly";
        keyClick(Qt.Key_Return);
        tryCompare(appLauncher, "state", AppLauncher.Failed);
        tryCompare(grownUp, "visible", true);
        compare(grownUp.appTitle, "Quits badly");
        compare(terminal.visible, false);
        keyClick(Qt.Key_Escape);
        tryCompare(grownUp, "visible", false);
        tryCompare(terminal, "visible", true);
        tryCompare(input, "activeFocus", true);
        compare(session.location, "/make");
    }

    // Fourteen tiles plus the Terminal: five rows, two showing. The window
    // slides by whole rows and the focused tile is never off screen.
    function openManyTiles() {
        launcher.destroy();
        launcher = createTemporaryObject(launcherComponent, testCase, {
            "manifestPath": Qt.resolvedUrl("fixtures/manifest-many.json")
        });
        verify(launcher !== null);
        grid = findChild(launcher, "tileGrid");
        appLauncher = findChild(launcher, "launcher");
        grownUp = findChild(launcher, "grownUpScreen");
        launcher.requestActivate();
        tryCompare(launcher, "active", true);
        tryCompare(grid, "count", 15);
        tryVerify(() => grid.currentItem !== null);
        tryCompare(grid.currentItem, "activeFocus", true);
    }

    function test_manyTilesScrollByRowWithTheKeys() {
        openManyTiles();
        const scroller = findChild(launcher, "gridScroller");
        const tileWindow = findChild(launcher, "tileWindow");
        verify(scroller !== null && tileWindow !== null);
        compare(scroller.rows, 5);
        compare(scroller.scrolls, true);
        // The third row peeks: cells are shorter than half the window.
        verify(grid.cellHeight < grid.windowHeight / 2);
        verify(grid.cellHeight * 2.5 <= grid.windowHeight + 1);
        keyClick(Qt.Key_Down);
        compare(scroller.firstRow, 0);
        keyClick(Qt.Key_Down);
        tryCompare(grid, "currentIndex", 6);
        compare(scroller.firstRow, 1);
        tryCompare(grid, "y", tileWindow.ring - grid.cellHeight);
        tryCompare(grid.currentItem, "activeFocus", true);
        // The focused tile is inside the window.
        const tileTop = grid.currentItem.y + grid.y;
        verify(tileTop >= 0 && tileTop + grid.currentItem.height <= tileWindow.height);
        keyClick(Qt.Key_Up);
        keyClick(Qt.Key_Up);
        tryCompare(grid, "currentIndex", 0);
        tryCompare(grid, "y", tileWindow.ring);
    }

    function test_wheelMovesTheFocusOneRow() {
        openManyTiles();
        const scroller = findChild(launcher, "gridScroller");
        mouseWheel(grid, grid.width / 2, grid.height / 4, 0, -120);
        tryCompare(grid, "currentIndex", 3);
        mouseWheel(grid, grid.width / 2, grid.height / 4, 0, -120);
        tryCompare(grid, "currentIndex", 6);
        tryCompare(scroller, "firstRow", 1);
        mouseWheel(grid, grid.width / 2, grid.height / 4, 0, 120);
        tryCompare(grid, "currentIndex", 3);
        compare(scroller.firstRow, 1);
    }

    function test_sixTilesFillTwoRowsAndDoNotScroll() {
        const scroller = findChild(launcher, "gridScroller");
        const tileWindow = findChild(launcher, "tileWindow");
        compare(scroller.scrolls, false);
        compare(grid.cellHeight, grid.windowHeight / 2);
        focusTile(5);
        compare(grid.y, tileWindow.ring);
        mouseWheel(grid, grid.width / 2, grid.height / 4, 0, -120);
        compare(grid.currentIndex, 5);
    }
}
