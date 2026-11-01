// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtTest
import Cairn.Brand
import Cairn.Guardian

TestCase {
    id: testCase

    name: "SteamWindow"
    when: windowShown

    // The free height of the smallest screen Cairn is tested on, 1280 by
    // 800, as Qt under Wayland gives it, with Plasma's panel counted as
    // free. Every window here is on it unless a test gives another, so the
    // tests do not depend on the test machine's own screen.
    readonly property int screenHeight: 800
    // The same screen at 200%, as the test VM's Plasma has it (#231).
    readonly property int shortScreen: 400

    Component {
        id: windowComponent

        Main {}
    }

    // Stands for Plasma's password box, or Steam's own window: a window of
    // another program that takes the front, and covers this one, while it
    // is up.
    Component {
        id: otherWindowComponent

        Window {
            transientParent: null
            width: 1280
            height: 800
            visible: true
        }
    }

    // On the 1280 by 800 screen, unless `screenHeight` gives another
    // screen's free height.
    function makeWindow(scenario, ownSteamRunning, screenHeight) {
        const window = createTemporaryObject(windowComponent, testCase, {
            "visit": standIns.visit(scenario, ownSteamRunning),
            "screenHeight": screenHeight === undefined ? testCase.screenHeight : screenHeight,
            // A window of its own, as it is in Plasma, so it is active
            // only when it has the focus itself.
            "transientParent": null
        });
        verify(window !== null);
        window.requestActivate();
        tryCompare(window, "active", true);
        return window;
    }

    // Presses the main button where the window shows it. The window places
    // its words and buttons when it next draws, so the press waits for that.
    function pressMainButton(window) {
        waitForPolish(findChild(window, "content"));
        waitForPolish(findChild(window, "buttons"));
        mouseClick(findChild(window, "mainButton"));
    }

    function heading(window) {
        return findChild(window, "heading").text;
    }

    // Everything the window says, heading first, as one string.
    function words(window) {
        const paragraphs = Array.from(findChild(window, "content").children).filter(item => item.objectName === "paragraph");
        return [heading(window)].concat(paragraphs.map(item => item.text)).join(" ");
    }

    // Each piece of the window that is shown, and its edges in the window:
    // the heading and paragraphs, then the buttons under them.
    function pieces(window) {
        const shown = [];
        for (const item of findChild(window, "content").children) {
            if (item.visible && item.height > 0)
                shown.push(item);
        }
        const buttons = findChild(window, "buttons");
        if (buttons.visible) {
            for (const button of buttons.children) {
                if (button.visible)
                    shown.push(button);
            }
        }
        return shown.map(item => {
            const corner = item.mapToItem(window.contentItem, item.width, item.height);
            return {
                "name": item.objectName + " " + (item.text || ""),
                "right": corner.x,
                "bottom": corner.y
            };
        });
    }

    // What is cut off at the window's edge, if anything.
    function cutOff(window) {
        return pieces(window).filter(piece => piece.right > window.width || piece.bottom > window.height).map(piece => piece.name);
    }

    // Whether an item, with `room` around it for a focus ring, shows whole
    // inside the window and inside every item above it that clips. Within
    // a pixel, as the window draws whole pixels.
    function shownWhole(window, item, room) {
        for (let above = item.parent; above; above = above.parent) {
            if (!above.clip && above !== window.contentItem)
                continue;
            const corner = item.mapToItem(above, -room, -room);
            if (corner.x < -1 || corner.y < -1 || corner.x + item.width + room * 2 > above.width + 1 || corner.y + item.height + room * 2 > above.height + 1)
                return false;
        }
        return true;
    }

    // The window shows all of what it says, with its buttons, is no taller
    // than that, and fits the 1280 by 800 screen without scrolling.
    function verifyFits(window, picture) {
        const step = window.visit.step;
        // The column places its pieces when the window next draws. Under
        // KWin the window can take its new height a moment after that.
        waitForPolish(findChild(window, "content"));
        waitForPolish(window.contentItem);
        tryVerify(() => cutOff(window).length === 0, 2000);
        compare(cutOff(window), [], "Cut off in step " + step);
        const scroller = findChild(window, "wordsScroller");
        verify(scroller.contentHeight <= scroller.height, "The words scroll in step " + step);
        verify(!findChild(window, "scrollIndicator").visible, "A bar beside words that fit in step " + step);
        const last = pieces(window).pop();
        verify(window.height >= last.bottom + window.margin, "No margin under the last piece in step " + step);
        verify(window.height <= last.bottom + window.margin + 1, "More than a margin under the last piece in step " + step + ": " + window.height);
        if (picture !== undefined && standIns.screenshotPath() !== "") {
            wait(200);
            grabImage(window.contentItem).save(standIns.screenshotPath().replace(".png", "-" + picture + ".png"));
        }
    }

    function test_itNamesTheChildAndOffersToOpen() {
        const window = makeWindow("open", false);
        compare(window.title, "Steam for nobody-here");
        compare(heading(window), "Open nobody-here's Steam");
        verify(words(window).includes("nobody-here cannot log in until you press Done."));
        verify(!words(window).includes("Your own Steam"));
        const main = findChild(window, "mainButton");
        compare(main.text, "Open");
        tryCompare(main, "activeFocus", true);
        const wayOut = findChild(window, "wayOutButton");
        verify(wayOut.visible);
        compare(wayOut.text, "Cancel");
    }

    function test_itSaysTheGuardiansOwnSteamClosesFirst() {
        const window = makeWindow("open", true);
        verify(words(window).includes("Your own Steam closes first, with any game or download in it."));
    }

    // The keyboard alone takes a visit from open to closed.
    function test_enterOpensAndDoneCloses() {
        const window = makeWindow("open", false);
        const main = findChild(window, "mainButton");
        const wayOut = findChild(window, "wayOutButton");
        tryCompare(main, "activeFocus", true);
        keyClick(Qt.Key_Return);
        tryCompare(findChild(window, "heading"), "text", "nobody-here's Steam is open");
        verify(words(window).includes("Press Done before you log out, restart or shut down."));
        compare(main.text, "Done");
        verify(main.visible);
        verify(!wayOut.visible);
        tryCompare(main, "activeFocus", true);
        keyClick(Qt.Key_Return);
        tryCompare(findChild(window, "heading"), "text", "nobody-here's Steam is closed");
        verify(words(window).includes("nobody-here can log in again."));
        compare(main.text, "Close");
    }

    function test_itSaysYourOwnSteamIsComingBack() {
        const window = makeWindow("steam-quits", true);
        pressMainButton(window);
        tryCompare(findChild(window, "heading"), "text", "nobody-here's Steam is closed");
        verify(words(window).includes("Your own Steam is starting again."));
    }

    function test_aRefusalSaysWhyAndOffersTryAgain() {
        const window = makeWindow("busy", false);
        pressMainButton(window);
        tryCompare(findChild(window, "heading"), "text", "nobody-here's Steam did not open");
        verify(words(window).includes("A child's Steam is already open on this computer."));
        compare(findChild(window, "mainButton").text, "Try again");
        compare(findChild(window, "wayOutButton").text, "Close");
    }

    // Provisioning is copying games, which is not a visit (ADR-0033).
    function test_aCopyOfTheGamesSaysToTryAgainSoon() {
        const window = makeWindow("copying", false);
        pressMainButton(window);
        tryCompare(findChild(window, "heading"), "text", "nobody-here's Steam did not open");
        verify(words(window).includes("Games are being copied into a child's Steam on this computer. Try again in a minute."));
        verify(!words(window).includes("already open"));
        compare(findChild(window, "mainButton").text, "Try again");
    }

    // A grown-up is logging a child out from the Games page (ADR-0041).
    function test_aLogoutSaysToTryAgainInAMoment() {
        const window = makeWindow("logging-out", false);
        pressMainButton(window);
        tryCompare(findChild(window, "heading"), "text", "nobody-here's Steam did not open");
        verify(words(window).includes("A child is being logged out on this computer. Try again in a moment."));
        verify(!words(window).includes("already open"));
    }

    // Plasma's password box gives the same answer for Cancel as for a wrong
    // password, so the window names both.
    function test_aClosedPasswordBoxSaysWhatItNeeds() {
        const window = makeWindow("not-authorized", false);
        pressMainButton(window);
        tryCompare(findChild(window, "heading"), "text", "nobody-here's Steam did not open");
        verify(words(window).includes("It needs your password. The box was closed, or the password was not accepted."));
        compare(findChild(window, "mainButton").text, "Try again");
    }

    function test_somethingUnexplainedSaysWhereToLook() {
        const window = makeWindow("no-such-scenario", false);
        pressMainButton(window);
        tryCompare(findChild(window, "heading"), "text", "nobody-here's Steam did not open");
        verify(words(window).includes("journalctl -t cairn-steam-for"));
    }

    function test_aStuckCloseSaysToRestart() {
        const window = makeWindow("stuck", false);
        pressMainButton(window);
        tryCompare(findChild(window, "heading"), "text", "nobody-here's Steam is open");
        pressMainButton(window);
        tryCompare(findChild(window, "heading"), "text", "nobody-here's Steam did not close properly");
        verify(words(window).includes("nobody-here cannot log in until the computer restarts."));
    }

    // While a visit is open, the close button closes the visit first.
    function test_theCloseButtonWaitsForTheVisitToClose() {
        const window = makeWindow("open", false);
        pressMainButton(window);
        tryCompare(findChild(window, "heading"), "text", "nobody-here's Steam is open");
        window.close();
        verify(window.visible);
        tryCompare(window.visit, "step", SteamForChild.Closed);
    }

    function test_everyButtonHasAName() {
        const window = makeWindow("busy", false);
        for (const name of ["mainButton", "wayOutButton"]) {
            const button = findChild(window, name);
            compare(button.Accessible.name, button.text);
            verify(button.text !== "");
        }
    }

    // Every message the window can show fits it, with the buttons under it.

    function test_theFirstMessageFits_data() {
        return [
            {
                "tag": "own Steam closed",
                "ownSteam": false
            },
            {
                "tag": "own Steam running",
                "ownSteam": true
            }
        ];
    }

    function test_theFirstMessageFits(data) {
        const window = makeWindow("open", data.ownSteam);
        verifyFits(window, data.ownSteam ? "ready-own-steam" : "ready");
    }

    function test_everyReasonItDidNotOpenFits_data() {
        const refusals = ["busy", "copying", "logging-out", "logged-in", "refused-still-running", "refused-not-set-up", "refused-not-a-child", "refused-no-desktop", "dismissed", "not-authorized", "refused-not-a-grown-up", "no-such-scenario"];
        const rows = [];
        for (const scenario of refusals) {
            rows.push({
                "tag": scenario,
                "scenario": scenario,
                "ownSteam": false
            });
            rows.push({
                "tag": scenario + ", own Steam back",
                "scenario": scenario,
                "ownSteam": true
            });
        }
        return rows;
    }

    function test_everyReasonItDidNotOpenFits(data) {
        const window = makeWindow(data.scenario, data.ownSteam);
        pressMainButton(window);
        tryCompare(window.visit, "step", SteamForChild.NotOpened);
        if (data.ownSteam)
            tryCompare(window.visit, "ownSteamComingBack", true);
        verify(findChild(window, "mainButton").visible);
        verify(findChild(window, "wayOutButton").visible);
        verifyFits(window, "not-opened-" + data.scenario.replace("refused-", "") + (data.ownSteam ? "-own-steam" : ""));
    }

    function test_ownSteamThatWillNotCloseFits() {
        const window = makeWindow("open", true);
        standIns.ownSteamWillNotQuit(true);
        pressMainButton(window);
        tryCompare(window.visit, "step", SteamForChild.ClosingOwnSteam);
        verifyFits(window, "closing-own-steam");
        tryCompare(window.visit, "step", SteamForChild.NotOpened);
        compare(window.visit.reason, SteamForChild.OwnSteamStillOpen);
        verifyFits(window, "not-opened-own-steam-still-open");
    }

    // #217: it named no place to quit Steam from, and said Steam was
    // starting again when it had never stopped.
    function test_ownSteamThatWillNotCloseSaysWhereToQuitIt() {
        const window = makeWindow("open", true);
        standIns.ownSteamWillNotQuit(true);
        pressMainButton(window);
        tryCompare(window.visit, "step", SteamForChild.NotOpened);
        verify(words(window).includes("Right-click the Steam icon near the clock and choose Exit Steam. When its icon has gone, try again."));
        verify(!window.visit.ownSteamComingBack);
        verify(!words(window).includes("starting again"));
    }

    function test_askingFits() {
        const window = makeWindow("asking", false);
        pressMainButton(window);
        tryCompare(window.visit, "step", SteamForChild.Asking);
        verifyFits(window, "asking");
    }

    function test_everyStepOfAVisitFits() {
        const window = makeWindow("slow-waiting", false);
        pressMainButton(window);
        tryCompare(window.visit, "step", SteamForChild.Waiting);
        verifyFits(window, "waiting");
        tryCompare(window.visit, "step", SteamForChild.Open, 6000);
        verifyFits(window, "open");
    }

    function test_closingAndClosedFit() {
        const window = makeWindow("slow-close", true);
        pressMainButton(window);
        tryCompare(window.visit, "step", SteamForChild.Open);
        pressMainButton(window);
        tryCompare(window.visit, "step", SteamForChild.Closing);
        verifyFits(window, "closing");
        tryCompare(window.visit, "step", SteamForChild.Closed, 6000);
        tryCompare(window.visit, "ownSteamComingBack", true);
        verifyFits(window, "closed-own-steam");
    }

    function test_aStuckCloseFits() {
        const window = makeWindow("stuck", true);
        pressMainButton(window);
        tryCompare(window.visit, "step", SteamForChild.Open);
        pressMainButton(window);
        tryCompare(window.visit, "step", SteamForChild.Stuck);
        tryCompare(window.visit, "ownSteamComingBack", true);
        verifyFits(window, "stuck-own-steam");
    }

    // #172: the window shrank while the password box was up, and did not
    // grow again for the message after it, so Try again and Close were cut
    // off. Here the longer message comes first, as it did then.
    function test_itGrowsAgainAfterThePasswordBoxCloses() {
        const window = makeWindow("dismissed-slowly", true);
        standIns.ownSteamWillNotQuit(true);
        pressMainButton(window);
        tryCompare(window.visit, "step", SteamForChild.NotOpened);
        verifyFits(window, "grows-1-own-steam-still-open");
        standIns.ownSteamWillNotQuit(false);
        pressMainButton(window);
        tryCompare(window.visit, "step", SteamForChild.Asking);
        verifyFits(window, "grows-2-asking");
        const box = createTemporaryObject(otherWindowComponent, testCase);
        box.requestActivate();
        tryCompare(box, "active", true);
        tryCompare(window.visit, "step", SteamForChild.NotOpened);
        compare(window.visit.reason, SteamForChild.NoPassword);
        box.destroy();
        verifyFits(window, "grows-3-no-password");
    }

    // #172 as KWin can cause it: the window is given the height it had for
    // the short message while the password box was up, after it has grown
    // for the longer one. The window puts its own height back.
    function test_itPutsItsHeightBackAfterAStaleResize() {
        const window = makeWindow("dismissed-slowly", true);
        standIns.ownSteamWillNotQuit(true);
        pressMainButton(window);
        tryCompare(window.visit, "step", SteamForChild.NotOpened);
        standIns.ownSteamWillNotQuit(false);
        pressMainButton(window);
        tryCompare(window.visit, "step", SteamForChild.Asking);
        waitForPolish(findChild(window, "content"));
        const askingHeight = window.height;
        tryCompare(window.visit, "step", SteamForChild.NotOpened);
        waitForPolish(findChild(window, "content"));
        verify(window.height > askingHeight, "No taller after the box closed: " + window.height);
        standIns.resizeFromOutside(window, window.width, askingHeight);
        tryCompare(window, "height", window.wordsHeight);
        verifyFits(window);
        standIns.resizeFromOutside(window, window.width, window.wordsHeight + 100);
        tryCompare(window, "height", window.wordsHeight);
    }

    // #231: at 200% on a 1280 by 800 screen the window ran past the bottom
    // of the screen, with Open and Done under the panel. It now leaves room
    // for its title bar and the panel, its words scroll beside a thin bar,
    // and the buttons stay whole under them, and work.
    function test_aShortScreenKeepsTheButtonsInSight_data() {
        return [
            {
                "tag": "ready",
                "open": false
            },
            {
                "tag": "open",
                "open": true
            }
        ];
    }

    function test_aShortScreenKeepsTheButtonsInSight(data) {
        const window = makeWindow("open", true, testCase.shortScreen);
        if (data.open) {
            pressMainButton(window);
            tryCompare(window.visit, "step", SteamForChild.Open);
        }
        const step = window.visit.step;
        compare(step, data.open ? SteamForChild.Open : SteamForChild.Ready);
        waitForPolish(findChild(window, "content"));
        waitForPolish(findChild(window, "buttons"));
        tryCompare(window, "height", window.fitHeight);
        verify(window.height + Tokens.headingSize * 3 <= testCase.shortScreen, "No room for the title bar and the panel: " + window.height);
        const scroller = findChild(window, "wordsScroller");
        verify(scroller.contentHeight > scroller.height, "The words fit, so this tests nothing");
        verify(scroller.clip, "The words draw past their part of the window");
        verify(findChild(window, "scrollIndicator").visible);
        const main = findChild(window, "mainButton");
        for (const button of findChild(window, "buttons").children) {
            if (button.visible)
                verify(shownWhole(window, button, Tokens.focusOffset + Tokens.focusWidth), button.text + " is cut off");
        }
        verify(scroller.mapToItem(window.contentItem, 0, scroller.height).y <= main.mapToItem(window.contentItem, 0, 0).y, "The words run under the buttons");
        if (standIns.screenshotPath() !== "") {
            wait(200);
            grabImage(window.contentItem).save(standIns.screenshotPath().replace(".png", "-short-" + data.tag + ".png"));
        }
        // The wheel scrolls the words, as a parent would. Scrolled to the
        // end, the last paragraph shows whole.
        mouseWheel(scroller, scroller.width / 2, scroller.height / 2, 0, -120);
        tryVerify(() => scroller.contentY > 1, 2000, "The wheel does not scroll the words");
        scroller.contentY = scroller.contentHeight - scroller.height;
        const paragraphs = Array.from(findChild(window, "content").children).filter(item => item.objectName === "paragraph");
        verify(shownWhole(window, paragraphs[paragraphs.length - 1], 0), "The last paragraph does not show whole");
        // Open goes on to the password, and Done closes the visit.
        mouseClick(main);
        tryVerify(() => window.visit.step !== step);
    }

    // #243: on a short screen the words scrolled with the wheel or a drag
    // only. While a button under them has the keyboard, Up and Down scroll
    // them a line, and Page Up and Page Down their part's height less a
    // line, as far as they go. Tab and Enter work as before.
    function test_theKeysScrollTheWordsOnAShortScreen_data() {
        return [
            {
                "tag": "ready",
                "open": false
            },
            {
                "tag": "open",
                "open": true
            }
        ];
    }

    function test_theKeysScrollTheWordsOnAShortScreen(data) {
        const window = makeWindow("open", true, testCase.shortScreen);
        if (data.open) {
            pressMainButton(window);
            tryCompare(window.visit, "step", SteamForChild.Open);
        }
        const step = window.visit.step;
        waitForPolish(findChild(window, "content"));
        waitForPolish(findChild(window, "buttons"));
        tryCompare(window, "height", window.fitHeight);
        const scroller = findChild(window, "wordsScroller");
        const line = Tokens.bodySize * Tokens.bodyLineHeight;
        const page = scroller.height - line;
        const end = scroller.contentHeight - scroller.height;
        verify(end > page, "The words are less than a page longer than their part, so this tests too little");
        const main = findChild(window, "mainButton");
        tryCompare(main, "activeFocus", true);
        compare(scroller.contentY, 0);
        keyClick(Qt.Key_Down);
        fuzzyCompare(scroller.contentY, line, 0.01, "Down does not scroll the words a line");
        keyClick(Qt.Key_Up);
        keyClick(Qt.Key_Up);
        compare(scroller.contentY, 0, "Up does not stop at the top");
        keyClick(Qt.Key_PageDown);
        fuzzyCompare(scroller.contentY, page, 0.01, "Page Down does not scroll the part's height less a line");
        for (let i = 0; i < Math.ceil(end / page); ++i)
            keyClick(Qt.Key_PageDown);
        compare(scroller.contentY, end, "Page Down does not stop at the end");
        keyClick(Qt.Key_PageUp);
        fuzzyCompare(scroller.contentY, end - page, 0.01, "Page Up does not scroll the part's height less a line");
        // Beside Open, Cancel passes them on too, and Tab moves between the
        // two as before.
        if (!data.open) {
            const wayOut = findChild(window, "wayOutButton");
            keyClick(Qt.Key_Tab);
            tryCompare(wayOut, "activeFocus", true);
            keyClick(Qt.Key_PageUp);
            compare(scroller.contentY, 0, "Page Up does not scroll the words from Cancel");
            keyClick(Qt.Key_Tab);
            tryCompare(main, "activeFocus", true);
        }
        // No key pressed the button, and Enter still does.
        compare(window.visit.step, step, "A key pressed the button");
        keyClick(Qt.Key_Return);
        tryVerify(() => window.visit.step !== step);
    }

    // The tests above give the window a screen of their own. Left alone,
    // it takes the free height Qt gives for its screen (#231).
    function test_itTakesTheFreeHeightQtGives() {
        const window = createTemporaryObject(windowComponent, testCase, {
            "visit": standIns.visit("open", false),
            "transientParent": null
        });
        verify(window !== null);
        compare(window.screenHeight, window.Screen.desktopAvailableHeight, "The window does not take the free height Qt gives");
    }

    // When the child's Steam does not open, the window comes in front of
    // whatever was there, such as the Guardian's own Steam updating itself.
    function test_itComesToTheFrontWhenItDidNotOpen() {
        const window = makeWindow("dismissed-slowly", false);
        pressMainButton(window);
        tryCompare(window.visit, "step", SteamForChild.Asking);
        const other = createTemporaryObject(otherWindowComponent, testCase);
        other.requestActivate();
        tryCompare(other, "active", true);
        tryCompare(window.visit, "step", SteamForChild.NotOpened);
        tryCompare(window, "active", true);
    }

    // While the password box is up, the window stays behind it.
    function test_itLeavesThePasswordBoxInFront() {
        const window = makeWindow("asking", false);
        pressMainButton(window);
        const other = createTemporaryObject(otherWindowComponent, testCase);
        other.requestActivate();
        tryCompare(other, "active", true);
        tryCompare(window.visit, "step", SteamForChild.Asking);
        wait(200);
        verify(other.active);
        verify(!window.active);
    }
}
