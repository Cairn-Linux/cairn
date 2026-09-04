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

    Component {
        id: launcherComponent

        Main {}
    }

    function init() {
        launcher = createTemporaryObject(launcherComponent, testCase);
        verify(launcher !== null);
        grid = findChild(launcher, "tileGrid");
        verify(grid !== null);
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
            },
            {
                tag: "return",
                start: 0,
                key: Qt.Key_Return,
                modifiers: Qt.NoModifier,
                expected: 0
            },
            {
                tag: "enter",
                start: 0,
                key: Qt.Key_Enter,
                modifiers: Qt.NoModifier,
                expected: 0
            },
            {
                tag: "space",
                start: 0,
                key: Qt.Key_Space,
                modifiers: Qt.NoModifier,
                expected: 0
            }
        ];
    }

    function test_navigation(data) {
        focusTile(data.start);
        keyClick(data.key, data.modifiers);
        tryCompare(grid, "currentIndex", data.expected);
        tryCompare(grid.currentItem, "activeFocus", true);
    }

    function test_mouseFocus() {
        const tile = grid.itemAtIndex(4);
        verify(tile !== null);
        mouseClick(tile);
        tryCompare(grid, "currentIndex", 4);
        tryCompare(tile, "activeFocus", true);
        compare(tile.Accessible.role, Accessible.Button);
        verify(tile.Accessible.name.length > 0);
        compare(tile.Accessible.focusable, true);
    }
}
