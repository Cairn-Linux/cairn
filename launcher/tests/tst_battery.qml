// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtTest
import Cairn.Brand
import Cairn.Launcher

// The battery on the bar above the tiles (ADR-0034). The window is the real
// Main.qml; its UPowerDevice asks no one, so each test sets the reading on
// BatteryLook the way UPower's would arrive.
TestCase {
    id: testCase

    name: "LauncherBattery"
    when: windowShown
    property Window launcher
    property GridView grid
    property BatteryLook look
    property Item battery
    property Item header
    property Item logOutTile
    property Item tileWindow

    readonly property string lowSentence: "The battery is low. Ask a grown-up for the charger."
    readonly property string almostEmptySentence: "The battery is almost empty. Ask a grown-up for the charger."

    Component {
        id: launcherComponent

        Main {}
    }

    function init() {
        launcher = createTemporaryObject(launcherComponent, testCase, {
            "manifestPath": Qt.resolvedUrl("fixtures/manifest.json")
        });
        verify(launcher !== null);
        grid = findChild(launcher, "tileGrid");
        look = findChild(launcher, "batteryLook");
        battery = findChild(launcher, "battery");
        header = findChild(launcher, "header");
        logOutTile = findChild(launcher, "logOutTile");
        tileWindow = findChild(launcher, "tileWindow");
        verify(grid !== null && look !== null && battery !== null);
        verify(header !== null && logOutTile !== null && tileWindow !== null);
        launcher.requestActivate();
        tryCompare(launcher, "active", true);
        tryCompare(grid, "count", 6);
        tryVerify(() => grid.currentItem !== null);
        tryCompare(grid.currentItem, "activeFocus", true);
    }

    // A laptop at 42 percent, discharging, not low, unless the test says
    // otherwise. The codes are UPower's.
    function readBattery(reading) {
        look.isPresent = reading.isPresent ?? true;
        look.percentage = reading.percentage ?? 42;
        look.state = reading.state ?? 2;
        look.warningLevel = reading.warningLevel ?? 1;
        look.type = reading.type ?? 2;
    }

    function giveLogOut() {
        const program = Qt.resolvedUrl("fixtures/log-out").toString().replace("file://", "");
        launcher.logOutProgram = program;
        tryVerify(() => logOutTile.visible);
    }

    function focusTile(index) {
        grid.currentIndex = index;
        tryVerify(() => grid.currentItem !== null);
        grid.currentItem.forceActiveFocus();
        tryCompare(grid.currentItem, "activeFocus", true);
    }

    function bottomOf(item) {
        return item.mapToItem(null, 0, item.height).y;
    }

    // A desktop or a virtual machine: the bar is as it was before the battery.
    function test_noBatteryNoBattery() {
        readBattery({
            "isPresent": false,
            "percentage": 0,
            "state": 0,
            "type": 0
        });
        compare(battery.visible, false);
        compare(header.visible, false);
        compare(tileWindow.y, Tokens.headingSize - tileWindow.ring);
        giveLogOut();
        compare(battery.visible, false);
        compare(header.visible, true);
        verify(bottomOf(logOutTile) <= tileWindow.y);
    }

    function test_aBatteryShowsAboveTheTiles() {
        readBattery({});
        tryCompare(battery, "visible", true);
        compare(battery.Accessible.role, Accessible.Indicator);
        compare(battery.Accessible.name, "Battery, 42 percent");
        const fill = findChild(battery, "batteryFill");
        fuzzyCompare(fill.width, fill.room * 0.42, 0.01);
        compare(findChild(battery, "batteryPlug").visible, false);
        compare(findChild(battery, "batterySentence").visible, false);
        verify(bottomOf(battery) <= tileWindow.y);
        // Level with the tiles' left edge.
        fuzzyCompare(battery.mapToItem(null, 0, 0).x, grid.mapToItem(null, 0, 0).x, 0.5);
    }

    function test_theFillFollowsTheLevel() {
        readBattery({
            "percentage": 100
        });
        const fill = findChild(battery, "batteryFill");
        fuzzyCompare(fill.width, fill.room, 0.01);
        look.percentage = 0;
        compare(fill.width, 0);
        compare(battery.Accessible.name, "Battery, 0 percent");
    }

    function test_theChargerShowsAPlug_data() {
        return [
            {
                tag: "charging",
                state: 1,
                percentage: 42,
                name: "Battery charging, 42 percent"
            },
            {
                tag: "on the charger, held",
                state: 5,
                percentage: 80,
                name: "Battery on the charger, 80 percent"
            },
            {
                tag: "full",
                state: 4,
                percentage: 100,
                name: "Battery full"
            }
        ];
    }

    function test_theChargerShowsAPlug(data) {
        readBattery({
            "state": data.state,
            "percentage": data.percentage
        });
        tryCompare(battery, "visible", true);
        compare(findChild(battery, "batteryPlug").visible, true);
        compare(findChild(battery, "batterySentence").visible, false);
        compare(battery.Accessible.name, data.name);
    }

    function test_lowSaysSoInOneSentence_data() {
        return [
            {
                tag: "low",
                warningLevel: 3,
                text: testCase.lowSentence
            },
            {
                tag: "critical",
                warningLevel: 4,
                text: testCase.almostEmptySentence
            },
            {
                tag: "action",
                warningLevel: 5,
                text: testCase.almostEmptySentence
            }
        ];
    }

    function test_lowSaysSoInOneSentence(data) {
        readBattery({
            "percentage": 4,
            "warningLevel": data.warningLevel
        });
        const sentence = findChild(battery, "batterySentence");
        tryCompare(sentence, "visible", true);
        compare(sentence.text, data.text);
        compare(sentence.Accessible.role, Accessible.StaticText);
        compare(sentence.Accessible.name, data.text);
        compare(findChild(battery, "batteryPlug").visible, false);
        compare(battery.Accessible.name, "Battery, 4 percent");
    }

    // UPower changes State before WarningLevel; the sentence goes with the
    // first, and the plug comes.
    function test_pluggingInTakesTheSentenceAway() {
        readBattery({
            "percentage": 15,
            "warningLevel": 3
        });
        const sentence = findChild(battery, "batterySentence");
        tryCompare(sentence, "visible", true);
        look.state = 1;
        compare(sentence.visible, false);
        compare(findChild(battery, "batteryPlug").visible, true);
        look.warningLevel = 1;
        compare(sentence.visible, false);
    }

    // At the launcher's smallest window, beside Log out, the sentence is
    // still whole and never smaller than a child's smallest text.
    function test_theSentenceFitsBesideLogOut_data() {
        return [
            {
                tag: "smallest window, low",
                width: 0,
                warningLevel: 3
            },
            {
                tag: "smallest window, almost empty",
                width: 0,
                warningLevel: 4
            },
            {
                tag: "1366, almost empty",
                width: 1366,
                warningLevel: 4
            }
        ];
    }

    function test_theSentenceFitsBesideLogOut(data) {
        if (data.width > 0)
            launcher.width = data.width;
        compare(launcher.width, data.width > 0 ? data.width : launcher.minimumWidth);
        giveLogOut();
        readBattery({
            "percentage": 4,
            "warningLevel": data.warningLevel
        });
        const sentence = findChild(battery, "batterySentence");
        tryCompare(sentence, "visible", true);
        waitForRendering(sentence);
        verify(!sentence.truncated);
        verify(sentence.fontInfo.pixelSize >= Tokens.minChild);
        verify(sentence.lineCount <= 2);
        verify(sentence.contentWidth <= sentence.width + 0.5);
        const right = sentence.mapToItem(header, sentence.width, 0).x;
        verify(right <= logOutTile.x - Tokens.headingSize + 0.5);
    }

    // Without Log out, as in a grown-up's run under Plasma on a laptop, the
    // bar is up for the battery alone and the tiles sit under it.
    function test_aBatteryWithoutLogOut() {
        readBattery({
            "warningLevel": 3,
            "percentage": 18
        });
        tryCompare(header, "visible", true);
        compare(logOutTile.visible, false);
        compare(battery.width, header.width);
        verify(bottomOf(header) <= tileWindow.y);
        compare(findChild(battery, "batterySentence").visible, true);
    }

    // The battery has nothing to press, so the focus never lands on it: Up
    // from the top row still reaches Log out, and with no Log out it wraps.
    function test_upStillReachesLogOut() {
        readBattery({});
        keyClick(Qt.Key_Up);
        verify(grid.currentIndex !== 0);
        tryCompare(grid.currentItem, "activeFocus", true);
        compare(battery.activeFocus, false);
        giveLogOut();
        focusTile(1);
        keyClick(Qt.Key_Up);
        tryCompare(logOutTile, "activeFocus", true);
        compare(battery.activeFocus, false);
        keyClick(Qt.Key_Down);
        tryCompare(grid.currentItem, "activeFocus", true);
        compare(grid.currentIndex, 1);
        for (let i = 0; i < 8; ++i) {
            keyClick(Qt.Key_Tab);
            compare(battery.activeFocus, false);
        }
    }

    // The bar goes with the tiles: not over the Terminal, nor the question
    // before logging out.
    function test_onlyAtTheTiles() {
        readBattery({
            "warningLevel": 3,
            "percentage": 18
        });
        tryCompare(battery, "visible", true);
        focusTile(5);
        keyClick(Qt.Key_Return);
        tryCompare(findChild(launcher, "terminalScreen"), "visible", true);
        compare(battery.visible, false);
        keyClick(Qt.Key_Escape);
        tryCompare(battery, "visible", true);
        giveLogOut();
        mouseClick(logOutTile);
        tryCompare(findChild(launcher, "logOutScreen"), "visible", true);
        compare(battery.visible, false);
        keyClick(Qt.Key_Escape);
        tryCompare(battery, "visible", true);
    }
}
