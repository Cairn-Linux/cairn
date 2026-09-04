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

    // A click focuses the tile and launches it; tile 4 quits cleanly, so the
    // launcher goes Starting, Running, Idle and the tiles stay on screen.
    function test_mouseFocusAndLaunch() {
        const tile = grid.itemAtIndex(4);
        verify(tile !== null);
        mouseClick(tile);
        tryCompare(grid, "currentIndex", 4);
        tryCompare(tile, "activeFocus", true);
        compare(tile.Accessible.role, Accessible.Button);
        verify(tile.Accessible.name.length > 0);
        compare(tile.Accessible.focusable, true);
        tryCompare(stateSpy, "count", 3);
        compare(appLauncher.state, AppLauncher.Idle);
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

    // Starting, Running, Idle: three changes prove the program really ran.
    function test_cleanLaunchReturnsToTiles() {
        focusTile(1);
        keyClick(Qt.Key_Return);
        tryCompare(stateSpy, "count", 3);
        compare(appLauncher.state, AppLauncher.Idle);
        compare(appLauncher.title, "Quits cleanly");
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
}
