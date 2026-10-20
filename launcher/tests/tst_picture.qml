// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtTest
import Cairn.Brand
import Cairn.Launcher

// A tile with a picture of its game (#138, ADR-0035), and the game tiles
// from the games file in the real Main.qml (ADR-0033). The stand-in picture
// is the brand's own mark; no test uses a game's real picture.
TestCase {
    id: testCase

    name: "TilePicture"
    when: windowShown

    readonly property url standInPicture: Qt.resolvedUrl("../../brand/mark.svg")

    // A TestCase is never visible itself, so a tile is shown in a window of
    // its own, where whether its title shows can be read.
    Component {
        id: windowComponent

        Window {
            width: 409
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

    // The size of a games tile on the kids' laptop, 1366x768, once the grid
    // scrolls.
    function makeTile(picture) {
        const window = createTemporaryObject(windowComponent, testCase);
        verify(window !== null);
        const tile = createTemporaryObject(tileComponent, window.contentItem, {
            "title": "Game one",
            "kind": TileModel.Games,
            "accessibleName": "Game one",
            "picture": picture,
            "width": 409,
            "height": 223
        });
        verify(tile !== null);
        return tile;
    }

    function test_aPictureTakesThePlaceOfTheTitle() {
        const tile = makeTile(standInPicture);
        const picture = findChild(tile, "tilePicture");
        const label = findChild(tile, "tileTitle");
        verify(picture !== null && label !== null);
        tryCompare(picture, "status", Image.Ready);
        verify(picture.visible);
        compare(label.visible, false);
        // Inside the rounded corners on every side, and drawn inside its box.
        compare(picture.x, Tokens.radiusTile);
        compare(picture.y, Tokens.radiusTile);
        compare(picture.width, tile.width - 2 * Tokens.radiusTile);
        compare(picture.height, tile.height - 2 * Tokens.radiusTile);
        verify(picture.paintedWidth <= picture.width + 0.5);
        verify(picture.paintedHeight <= picture.height + 0.5);
        // Decoded at the size it is drawn, not the file's own.
        compare(picture.sourceSize.width, picture.width);
        compare(picture.sourceSize.height, picture.height);
        // A screen reader still says the game's name.
        compare(tile.Accessible.name, "Game one");
        compare(picture.Accessible.ignored, true);
        // The card is unchanged: a games tile keeps its colour and edge.
        compare(tile.color, Tokens.games);
        compare(tile.border.width, Tokens.strokeHairline);
    }

    function test_aPictureThatCannotBeDrawnShowsTheTitle_data() {
        return [
            {
                tag: "missing",
                picture: Qt.resolvedUrl("fixtures/no-such-picture.png"),
                warning: /Cannot open/
            },
            {
                tag: "not-a-picture",
                picture: Qt.resolvedUrl("fixtures/games.json"),
                warning: /Error decoding/
            }
        ];
    }

    function test_aPictureThatCannotBeDrawnShowsTheTitle(data) {
        ignoreWarning(data.warning);
        const tile = makeTile(data.picture);
        const picture = findChild(tile, "tilePicture");
        const label = findChild(tile, "tileTitle");
        tryCompare(picture, "status", Image.Error);
        compare(label.visible, true);
    }

    function test_aTileWithoutAPictureShowsItsTitle() {
        const tile = makeTile("");
        const picture = findChild(tile, "tilePicture");
        const label = findChild(tile, "tileTitle");
        compare(picture.status, Image.Null);
        compare(label.visible, true);
        compare(label.text, "Game one");
    }

    // Five tiles from the manifest, three from the games file, then the
    // Terminal.
    function test_gameTilesComeAfterTheManifestsAndBeforeTheTerminal() {
        const launcher = createTemporaryObject(launcherComponent, testCase, {
            "manifestPath": Qt.resolvedUrl("fixtures/manifest.json"),
            "gamesPath": Qt.resolvedUrl("fixtures/games.json")
        });
        verify(launcher !== null);
        const grid = findChild(launcher, "tileGrid");
        verify(grid !== null);
        tryCompare(grid, "count", 9);
        const titles = [];
        for (let i = 0; i < grid.count; ++i) {
            tryVerify(() => grid.itemAtIndex(i) !== null);
            titles.push(grid.itemAtIndex(i).title);
        }
        compare(titles, ["Quits badly", "Quits cleanly", "Missing program", "Nothing set up", "Practice", "Game one", "Game two", "Game three", "Terminal"]);
        for (let game = 5; game < 8; ++game) {
            const tile = grid.itemAtIndex(game);
            compare(tile.kind, TileModel.Games);
            compare(tile.exec, ["steam", "-applaunch", String(1001 + game - 5)]);
            compare(tile.picture.toString(), "");
        }
        compare(grid.itemAtIndex(8).opensTerminal, true);
    }
}
