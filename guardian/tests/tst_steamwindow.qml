// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtTest
import Cairn.Guardian

TestCase {
    id: testCase

    name: "SteamWindow"
    when: windowShown

    Component {
        id: windowComponent

        Main {}
    }

    function makeWindow(scenario, ownSteamRunning) {
        const window = createTemporaryObject(windowComponent, testCase, {
            "visit": standIns.visit(scenario, ownSteamRunning)
        });
        verify(window !== null);
        window.requestActivate();
        tryCompare(window, "active", true);
        return window;
    }

    function heading(window) {
        return findChild(window, "heading").text;
    }

    // Everything the window says, heading first, as one string.
    function words(window) {
        return [heading(window)].concat(window.paragraphs()).join(" ");
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
        mouseClick(findChild(window, "mainButton"));
        tryCompare(findChild(window, "heading"), "text", "nobody-here's Steam is closed");
        verify(words(window).includes("Your own Steam is starting again."));
    }

    function test_aRefusalSaysWhyAndOffersTryAgain() {
        const window = makeWindow("busy", false);
        mouseClick(findChild(window, "mainButton"));
        tryCompare(findChild(window, "heading"), "text", "nobody-here's Steam did not open");
        verify(words(window).includes("A child's Steam is already open on this computer."));
        compare(findChild(window, "mainButton").text, "Try again");
        compare(findChild(window, "wayOutButton").text, "Close");
    }

    // Plasma's password box gives the same answer for Cancel as for a wrong
    // password, so the window names both.
    function test_aClosedPasswordBoxSaysWhatItNeeds() {
        const window = makeWindow("not-authorized", false);
        mouseClick(findChild(window, "mainButton"));
        tryCompare(findChild(window, "heading"), "text", "nobody-here's Steam did not open");
        verify(words(window).includes("It needs your password. The box was closed, or the password was not accepted."));
        compare(findChild(window, "mainButton").text, "Try again");
    }

    function test_somethingUnexplainedSaysWhereToLook() {
        const window = makeWindow("no-such-scenario", false);
        mouseClick(findChild(window, "mainButton"));
        tryCompare(findChild(window, "heading"), "text", "nobody-here's Steam did not open");
        verify(words(window).includes("journalctl -t cairn-steam-for"));
    }

    function test_aStuckCloseSaysToRestart() {
        const window = makeWindow("stuck", false);
        mouseClick(findChild(window, "mainButton"));
        tryCompare(findChild(window, "heading"), "text", "nobody-here's Steam is open");
        mouseClick(findChild(window, "mainButton"));
        tryCompare(findChild(window, "heading"), "text", "nobody-here's Steam did not close properly");
        verify(words(window).includes("nobody-here cannot log in until the computer restarts."));
    }

    // While a visit is open, the close button closes the visit first.
    function test_theCloseButtonWaitsForTheVisitToClose() {
        const window = makeWindow("open", false);
        mouseClick(findChild(window, "mainButton"));
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
}
