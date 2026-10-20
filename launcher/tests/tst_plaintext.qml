// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtTest
import Cairn.Launcher

// A game's title is its name in Steam, which a game's maker or a Guardian
// can set to anything, so the launcher shows every title as plain text.
// Read as markup, the <img> below would make the child's launcher load a
// picture from the network (non-negotiable 1). The address is the loopback
// discard port, so nothing leaves this computer even if a test fails.
TestCase {
    id: testCase

    name: "PlainText"
    when: windowShown

    readonly property string markup: "<b>Putt</b><img src=\"http://127.0.0.1:9/x.png\">"

    Component {
        id: windowComponent

        Window {
            width: 1200
            height: 223
            visible: true
        }
    }

    Component {
        id: tileComponent

        Tile {}
    }

    Component {
        id: launcherComponent

        Main {}
    }

    Component {
        id: plainComponent

        Text {
            textFormat: Text.PlainText
        }
    }

    TextMetrics {
        id: metrics
    }

    // The label shows `expected` exactly as written: plain text, laid out
    // just as the same characters are when nothing is read as markup.
    function verifyPlain(label, expected) {
        verify(label !== null);
        compare(label.textFormat, Text.PlainText, label.objectName + " is plain text");
        compare(label.text, expected, label.objectName + " keeps its text");
        const plain = createTemporaryObject(plainComponent, label.parent, {
            "text": expected,
            "width": label.width,
            "wrapMode": label.wrapMode,
            "maximumLineCount": label.maximumLineCount,
            "elide": label.elide,
            "lineHeight": label.lineHeight,
            "font": label.font
        });
        verify(plain !== null);
        compare(label.lineCount, plain.lineCount);
        fuzzyCompare(label.contentWidth, plain.contentWidth, 0.5);
        fuzzyCompare(label.contentHeight, plain.contentHeight, 0.5);
    }

    // Wide enough for the whole title on one line, so its drawn width is
    // the width of every character in it, tags and all.
    function test_aTileShowsMarkupInItsTitleAsWritten() {
        const window = createTemporaryObject(windowComponent, testCase);
        verify(window !== null);
        const tile = createTemporaryObject(tileComponent, window.contentItem, {
            "title": markup,
            "kind": TileModel.Games,
            "accessibleName": markup,
            "width": window.width,
            "height": window.height
        });
        verify(tile !== null);
        const label = findChild(tile, "tileTitle");
        verifyPlain(label, markup);
        compare(label.visible, true);
        compare(label.lineCount, 1);
        metrics.font = label.font;
        metrics.text = markup;
        fuzzyCompare(label.contentWidth, metrics.advanceWidth, 1);
    }

    // The same title from the games file, on its tile in the real launcher,
    // then on the starting and grown-up screens that name it.
    function test_aTitleFromTheGamesFileIsShownAsWrittenEverywhere() {
        const launcher = createTemporaryObject(launcherComponent, testCase, {
            "manifestPath": Qt.resolvedUrl("fixtures/manifest.json"),
            "gamesPath": Qt.resolvedUrl("fixtures/games-markup.json")
        });
        verify(launcher !== null);
        const grid = findChild(launcher, "tileGrid");
        const appLauncher = findChild(launcher, "launcher");
        const starting = findChild(launcher, "startingScreen");
        const grownUp = findChild(launcher, "grownUpScreen");
        verify(grid !== null && appLauncher !== null && starting !== null && grownUp !== null);
        tryCompare(grid, "count", 7);
        tryVerify(() => grid.itemAtIndex(5) !== null);
        const tile = grid.itemAtIndex(5);
        compare(tile.title, markup);
        verifyPlain(findChild(tile, "tileTitle"), markup);

        // A start that opens no window: the starting screen, then the
        // grown-up screen. A harmless program stands in for Steam.
        appLauncher.settleMilliseconds = 2000;
        appLauncher.launchGraceMilliseconds = 200;
        appLauncher.steadyMilliseconds = 0;
        appLauncher.launch(tile.title, ["/bin/sh", "-c", "exit 0"]);
        compare(appLauncher.state, AppLauncher.Starting);
        compare(starting.visible, true);
        verifyPlain(findChild(starting, "startingSentence"), markup + " is starting.");
        tryCompare(grownUp, "visible", true);
        verifyPlain(findChild(grownUp, "grownUpSentence"), markup + " did not start.");
        appLauncher.dismiss();
        tryCompare(grownUp, "visible", false);

        // A tile with nothing set up names itself in the heading.
        ignoreWarning("No program is set up for the tile " + markup + ".");
        appLauncher.launch(markup, []);
        compare(appLauncher.state, AppLauncher.ComingSoon);
        compare(grownUp.visible, true);
        verifyPlain(findChild(grownUp, "grownUpHeading"), markup + " is coming soon.");
        appLauncher.dismiss();
        tryCompare(grownUp, "visible", false);

        // A window that opens on its own is named by its own title.
        ignoreWarning(markup + " opened a window on its own.");
        appLauncher.windowOpened("w1", "other", markup);
        compare(appLauncher.state, AppLauncher.Interrupted);
        compare(grownUp.visible, true);
        verifyPlain(findChild(grownUp, "grownUpSentence"), markup + " opened on its own.");
        appLauncher.windowClosed("w1");
        tryCompare(grownUp, "visible", false);
        wait(300); // let the launching process be reaped.
    }
}
