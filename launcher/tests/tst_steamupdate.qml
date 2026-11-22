// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtTest
import Cairn.Brand
import Cairn.Launcher

// While Steam updates itself, the tiles of the games that start in Steam
// wait (ADR-0061): their pictures or titles fade, the mark builds over the
// middle of each, all in step, and a press on one opens nothing. Steam is
// the tests' own stand-in (tst_navigation.cpp): its logs in a folder of
// theirs and a process list with a `steam` in it.
TestCase {
    id: testCase

    name: "SteamUpdateTiles"
    when: windowShown

    property Window launcher
    property GridView grid
    property AppLauncher appLauncher
    property SteamUpdateWatch watch
    property Timer clock
    readonly property var gameTiles: [5, 6, 7]
    readonly property string waitsText: "Waits for a Steam update. It opens when Steam is done."

    Component {
        id: launcherComponent

        Main {}
    }

    Component {
        id: windowComponent

        Window {
            width: 800
            height: 600
            visible: true
        }
    }

    Component {
        id: startingComponent

        StartingScreen {}
    }

    Component {
        id: tileComponent

        Tile {}
    }

    SignalSpy {
        id: heldSpy

        target: testCase.appLauncher
        signalName: "heldForSteam"
    }

    // The five manifest tiles, three Steam games, then the Terminal.
    function init() {
        steamStandIn.forget();
        launcher = createTemporaryObject(launcherComponent, testCase, {
            "manifestPath": Qt.resolvedUrl("fixtures/manifest.json"),
            "gamesPath": Qt.resolvedUrl("fixtures/games.json")
        });
        verify(launcher !== null);
        grid = findChild(launcher, "tileGrid");
        appLauncher = findChild(launcher, "launcher");
        watch = findChild(launcher, "steamUpdate");
        clock = findChild(launcher, "stoneClock");
        verify(grid !== null && appLauncher !== null && watch !== null && clock !== null);
        appLauncher.settleMilliseconds = 2000;
        appLauncher.launchGraceMilliseconds = 200;
        appLauncher.steadyMilliseconds = 0;
        heldSpy.clear();
        launcher.requestActivate();
        tryCompare(launcher, "active", true);
        tryCompare(grid, "count", 9);
        for (let i = 0; i < grid.count; ++i)
            tryVerify(() => grid.itemAtIndex(i) !== null);
        compare(watch.active, true);
        compare(watch.updating, false);
        compare(clock.running, false);
    }

    // No test file after this one starts with a Steam that is updating.
    function cleanup() {
        steamStandIn.forget();
    }

    function steamUpdates() {
        steamStandIn.startsUpdating();
        watch.processRoot = steamStandIn.steamRunning();
        compare(watch.updating, true);
    }

    function stonesOf(index) {
        return findChild(grid.itemAtIndex(index), "steamChipStones");
    }

    function test_onlyTheGamesThatStartInSteamWait() {
        steamUpdates();
        for (let i = 0; i < grid.count; ++i) {
            const tile = grid.itemAtIndex(i);
            const waits = gameTiles.includes(i);
            const chip = findChild(tile, "steamChip");
            verify(chip !== null);
            compare(tile.waitsForSteam, waits, tile.title);
            tryCompare(chip, "opacity", waits ? 1 : 0);
            compare(chip.visible, waits);
            compare(chip.Accessible.ignored, true);
            const faded = waits ? Tokens.coveredOpacity : 1;
            tryCompare(findChild(tile, "tilePicture"), "opacity", faded);
            tryCompare(findChild(tile, "tileTitle"), "opacity", faded);
            // The card, its edge and the name a screen reader says stay.
            compare(tile.opacity, 1);
            compare(tile.Accessible.name, tile.title);
            compare(tile.Accessible.description, waits ? waitsText : "");
        }
        compare(grid.itemAtIndex(5).color, Tokens.games);
        compare(grid.itemAtIndex(5).border.width, Tokens.strokeHairline);
        compare(grid.itemAtIndex(8).opensTerminal, true);
    }

    // The mark with one base-stone height of clear space on every side,
    // 120 by 114 of its units, at half the starting screen's size.
    function test_theChipIsTheMarkWithItsClearSpaceInTheMiddleOfTheTile() {
        steamUpdates();
        const tile = grid.itemAtIndex(5);
        const chip = findChild(tile, "steamChip");
        const stones = stonesOf(5);
        const unit = Tokens.displaySize * 3 / 2 / Tokens.markHeight;
        compare(stones.unit, unit);
        fuzzyCompare(chip.width, 120 * unit, 0.01);
        fuzzyCompare(chip.height, 114 * unit, 0.01);
        // Centred to the whole pixel.
        fuzzyCompare(chip.x + chip.width / 2, tile.width / 2, 1);
        fuzzyCompare(chip.y + chip.height / 2, tile.height / 2, 1);
        compare(chip.color, Tokens.pebble);
        compare(chip.radius, Tokens.radiusSm);
        compare(chip.border.width, 0);
        const clear = Tokens.markStones[Tokens.markStones.length - 1][3] * unit;
        fuzzyCompare(stones.x + stones.stonesLeft * unit, clear, 0.01);
        fuzzyCompare(stones.y + stones.stonesTop * unit, clear, 0.01);
        fuzzyCompare(chip.width - stones.x - stones.stonesRight * unit, clear, 0.01);
        fuzzyCompare(chip.height - stones.y - stones.stonesBottom * unit, clear, 0.01);
    }

    // One clock steps every chip, so all of them show the same step.
    function test_everyWaitingTileBuildsInStep() {
        steamUpdates();
        compare(clock.running, true);
        compare(clock.interval, Tokens.motionStone);
        compare(launcher.stoneStep, 0);
        for (const i of gameTiles)
            compare(stonesOf(i).stonesUp, 1);
        tryVerify(() => launcher.stoneStep >= 2, 3000);
        const up = stonesOf(5).stonesUp;
        verify(up >= 2);
        for (const i of gameTiles)
            compare(stonesOf(i).stonesUp, up);
    }

    function test_theClockStopsWhenNoTileWaits() {
        steamUpdates();
        compare(clock.running, true);
        watch.processRoot = steamStandIn.noSteamRunning();
        compare(watch.updating, false);
        compare(clock.running, false);
        const tile = grid.itemAtIndex(5);
        compare(tile.waitsForSteam, false);
        tryCompare(findChild(tile, "steamChip"), "opacity", 0);
        compare(findChild(tile, "steamChip").visible, false);
        tryCompare(findChild(tile, "tileTitle"), "opacity", 1);
        compare(tile.Accessible.description, "");
    }

    // Behind the Terminal the tiles are not in front: nothing is read, the
    // answer stays, and the clock stops until the tiles come back.
    function test_theClockStopsWhileTheTilesAreNotInFront() {
        steamUpdates();
        launcher.openTerminal();
        compare(watch.active, false);
        compare(clock.running, false);
        compare(watch.updating, true);
        launcher.closeTerminal();
        compare(watch.active, true);
        compare(clock.running, true);
    }

    // A press on a waiting tile opens nothing, and every chip starts again
    // from no stones, together.
    function test_aPressOnAWaitingTileOpensNothingAndTheStonesStartAgain() {
        steamUpdates();
        tryVerify(() => launcher.stoneStep >= 2, 3000);
        grid.currentIndex = 5;
        grid.currentItem.forceActiveFocus();
        tryCompare(grid.currentItem, "activeFocus", true);
        keyClick(Qt.Key_Return);
        compare(heldSpy.count, 1);
        compare(appLauncher.state, AppLauncher.Idle);
        compare(grid.visible, true);
        compare(findChild(launcher, "startingScreen").visible, false);
        for (const i of gameTiles)
            compare(stonesOf(i).stonesUp, 0);
        tryCompare(stonesOf(5), "stonesUp", 1);
        for (const i of gameTiles)
            compare(stonesOf(i).stonesUp, 1);
        mouseClick(grid.itemAtIndex(6));
        compare(heldSpy.count, 2);
        grid.itemAtIndex(7).Accessible.pressAction();
        compare(heldSpy.count, 3);
        compare(appLauncher.state, AppLauncher.Idle);
    }

    function test_aPressOnATileThatDoesNotWaitOpensIt() {
        steamUpdates();
        mouseClick(grid.itemAtIndex(1));
        compare(heldSpy.count, 0);
        compare(appLauncher.state, AppLauncher.Starting);
        compare(watch.active, false);
        wait(300); // let the launching process be reaped.
    }

    // The tiles still show the wait, but a read at the press says Steam has
    // just finished: the wait ends and the game opens.
    function test_aPressAsSteamFinishesOpensTheGame() {
        steamUpdates();
        steamStandIn.startsItself();
        mouseClick(grid.itemAtIndex(5));
        compare(heldSpy.count, 0);
        compare(watch.updating, false);
        compare(appLauncher.state, AppLauncher.Starting);
        compare(appLauncher.title, "Game one");
        wait(300); // let the launching process be reaped.
    }

    // A tile nobody tells to wait, such as Log out or Back, never does.
    function test_aTileWaitsOnlyWhenTold() {
        const window = createTemporaryObject(windowComponent, testCase);
        const tile = createTemporaryObject(tileComponent, window.contentItem, {
            "title": "Log out",
            "kind": TileModel.Machine,
            "accessibleName": "Log out",
            "width": 300,
            "height": 100
        });
        compare(tile.waitsForSteam, false);
        compare(findChild(tile, "steamChip").visible, false);
        compare(findChild(tile, "tileTitle").opacity, 1);
        compare(tile.Accessible.description, "");
    }

    // The starting screen draws the same stones, at twice the size, and
    // builds them as it always has: a stone more each step, bottom first,
    // the whole mark for a step, no mark for a step.
    function test_theStartingScreenBuildsItsStonesAsBefore() {
        const window = createTemporaryObject(windowComponent, testCase);
        const starting = createTemporaryObject(startingComponent, window.contentItem, {
            "appTitle": "Arthur's Birthday",
            "gettingReady": false,
            "width": 800,
            "height": 600
        });
        const stones = findChild(starting, "startingStones");
        verify(stones !== null);
        const unit = Tokens.displaySize * 3 / Tokens.markHeight;
        compare(stones.unit, unit);
        fuzzyCompare(stones.width, Tokens.markWidth * unit, 0.01);
        fuzzyCompare(stones.height, Tokens.markHeight * unit, 0.01);
        const expected = [1, 2, 3, 4, 4, 0];
        for (let step = 0; step < expected.length; ++step) {
            starting.step = step;
            compare(starting.stonesUp, expected[step], "step " + step);
        }
        starting.step = 0;
        tryCompare(starting, "stonesUp", 2);
    }
}
