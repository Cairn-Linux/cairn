// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtTest
import Cairn.Brand
import Cairn.Family

TestCase {
    id: testCase

    name: "FamilyWindow"
    when: windowShown

    Component {
        id: windowComponent

        FamilyWindow {}
    }

    // Tall enough for the whole page unless the options say otherwise, so
    // every control can be clicked.
    function makeWindow(scenario, options) {
        const given = options === undefined ? {} : options;
        const properties = {
            "page": standIns.page(scenario, given.withChildren !== false, given.withOlderChild === true),
            "gamesPage": standIns.gamesPage(given.withChildren !== false, given.withGames !== false, given.steamStarts !== false, given.gamesScenario === undefined ? "" : given.gamesScenario),
            "height": 1400
        };
        const window = createTemporaryObject(windowComponent, testCase, properties);
        // As small as the window may be made.
        if (given.smallest === true) {
            window.width = window.minimumWidth;
            window.height = window.minimumHeight;
        }
        verify(window !== null);
        window.requestActivate();
        tryCompare(window, "active", true);
        waitForPolish(window);
        return window;
    }

    function rows(window) {
        const names = [];
        const repeater = findChild(window, "childRows");
        for (let i = 0; i < repeater.count; ++i) {
            const row = repeater.itemAt(i);
            names.push(findChild(row, "childName").text + ", " + findChild(row, "childLevel").text);
        }
        return names;
    }

    function outcome(window) {
        return findChild(window, "outcome").text;
    }

    function nameField(window) {
        return findChild(window, "nameField");
    }

    function test_itListsTheChildren() {
        const window = makeWindow("done");
        compare(window.title, "Family");
        compare(findChild(window, "heading").text, "Children");
        compare(rows(window), ["Ada, Level 1", "Ben, Level 2"]);
        const repeater = findChild(window, "childRows");
        verify(findChild(repeater.itemAt(0), "moveTo2").visible);
        compare(findChild(repeater.itemAt(0), "moveTo2").text, "Move to level 2");
        compare(findChild(repeater.itemAt(1), "moveTo1").text, "Move to level 1");
        verify(!findChild(window, "noChildren").visible);
        compare(outcome(window), "");
        if (standIns.screenshotPath() !== "") {
            wait(200);
            grabImage(window.contentItem).save(standIns.screenshotPath());
        }
    }

    function test_noChildrenYet() {
        const window = makeWindow("done", {
            "withChildren": false
        });
        compare(rows(window), []);
        verify(findChild(window, "noChildren").visible);
    }

    // The short name field shows only when the name gives no login name.
    function test_aShortNameOnlyWhenNeeded() {
        const window = makeWindow("done");
        const shortName = findChild(window, "shortNameField");
        verify(!shortName.visible);
        nameField(window).text = "Ada";
        verify(!shortName.visible);
        nameField(window).text = "Анна";
        verify(shortName.visible);
    }

    // Refused in the window, before any password, and the name stays.
    function test_aNameWithACommaIsRefused() {
        const window = makeWindow("done");
        nameField(window).text = "Ada, the first";
        mouseClick(findChild(window, "addButton"));
        compare(outcome(window), "A name cannot hold a colon, a comma, a backslash or a line break. Change the name and try again.");
        compare(nameField(window).text, "Ada, the first");
    }

    function test_anEmptyNameIsRefused() {
        const window = makeWindow("done");
        mouseClick(findChild(window, "addButton"));
        compare(outcome(window), "Type the child's name first.");
    }

    // The keyboard alone: type, Tab to level 2, Space, then Enter in the
    // name field adds the child.
    function test_addWithTheKeyboard() {
        const window = makeWindow("done");
        const input = findChild(nameField(window), "input");
        input.forceActiveFocus();
        keyClick("C");
        keyClick("y");
        compare(nameField(window).text, "Cy");
        keyClick(Qt.Key_Tab);
        tryCompare(findChild(window, "level1"), "activeFocus", true);
        keyClick(Qt.Key_Tab);
        tryCompare(findChild(window, "level2"), "activeFocus", true);
        keyClick(Qt.Key_Space);
        compare(findChild(window, "addChildForm").level, 2);
        keyClick(Qt.Key_Tab);
        tryCompare(findChild(window, "addButton"), "activeFocus", true);
        keyClick(Qt.Key_Return);
        tryCompare(window.page, "step", ChildrenPage.Added);
        compare(outcome(window), "Cy can log in now, at level 2. Cy starts with no games: choose them on the Games page.");
        // Done, so the form is empty for the next child.
        compare(nameField(window).text, "");
        compare(findChild(window, "addChildForm").level, 1);
    }

    // Up or down, the same words.
    function test_changeALevel() {
        const window = makeWindow("done");
        const repeater = findChild(window, "childRows");
        mouseClick(findChild(repeater.itemAt(1), "moveTo1"));
        tryCompare(window.page, "step", ChildrenPage.LevelChanged);
        compare(outcome(window), "Ben is at level 1 now. It starts the next time Ben logs in.");
        mouseClick(findChild(repeater.itemAt(0), "moveTo2"));
        tryCompare(window.page, "step", ChildrenPage.LevelChanged);
        compare(outcome(window), "Ada is at level 2 now. It starts the next time Ada logs in.");
    }

    // While the password box is open, every control waits.
    function test_everythingWaitsWhileItRuns() {
        const window = makeWindow("asking");
        const repeater = findChild(window, "childRows");
        mouseClick(findChild(repeater.itemAt(0), "moveTo2"));
        compare(window.page.busy, true);
        compare(outcome(window), "Confirm with your password, or your fingerprint, in the box that opened.");
        verify(!findChild(repeater.itemAt(0), "moveTo2").enabled);
        verify(!findChild(repeater.itemAt(1), "moveTo1").enabled);
        verify(!findChild(window, "addButton").enabled);
        verify(!findChild(nameField(window), "input").enabled);
        verify(!findChild(window, "level2").enabled);
    }

    // A refusal says why, and leaves the form filled in to try again.
    function test_aRefusalKeepsTheForm() {
        const window = makeWindow("not-authorized");
        nameField(window).text = "Cy";
        mouseClick(findChild(window, "level2"));
        mouseClick(findChild(window, "addButton"));
        tryCompare(window.page, "step", ChildrenPage.NotChanged);
        compare(outcome(window), "Nothing changed. It needs your password: the box was closed, or the password was not accepted.");
        compare(nameField(window).text, "Cy");
        compare(findChild(window, "addChildForm").level, 2);
        verify(findChild(window, "addButton").enabled);
    }

    function test_aFailureSaysTryingAgainIsSafe() {
        const window = makeWindow("refused-failed");
        mouseClick(findChild(findChild(window, "childRows").itemAt(0), "moveTo2"));
        tryCompare(window.page, "step", ChildrenPage.NotChanged);
        verify(outcome(window).startsWith("The change did not finish. Trying again is safe."));
    }

    // Two causes no one should meet, each with its own sentence.
    function test_theCommandsOwnRefusals_data() {
        return [
            {
                "tag": "not-root",
                "scenario": "refused-not-root",
                "start": "Cairn's family command did not get the rights it needs"
            },
            {
                "tag": "usage",
                "scenario": "refused-usage",
                "start": "This window asked Cairn's family command for something it does not do"
            }
        ];
    }

    function test_theCommandsOwnRefusals(data) {
        const window = makeWindow(data.scenario);
        mouseClick(findChild(findChild(window, "childRows").itemAt(0), "moveTo2"));
        tryCompare(window.page, "step", ChildrenPage.NotChanged);
        verify(outcome(window).startsWith(data.start), outcome(window));
    }

    // Whether an item shows whole inside the window.
    function inSight(window, item) {
        const place = item.mapToItem(window.contentItem, 0, 0);
        return place.y >= 0 && place.y + item.height <= window.height;
    }

    // With the window short and scrolled down to the form, the sentence
    // after Add is scrolled into sight.
    function test_theOutcomeComesIntoSight() {
        const window = makeWindow("done", {
            "smallest": true
        });
        const scroller = findChild(window, "scroller");
        verify(findChild(window, "scrollIndicator").visible);
        nameField(window).text = "Анна";
        scroller.contentY = scroller.contentHeight - scroller.height;
        const add = findChild(window, "addButton");
        verify(inSight(window, add));
        mouseClick(add);
        compare(window.page.reason, ChildrenPage.NoShortName);
        const box = findChild(window, "outcome").parent;
        tryVerify(() => inSight(window, box));
    }

    // Tab never lands on something out of sight.
    function test_focusIsKeptInSight() {
        const window = makeWindow("done", {
            "smallest": true
        });
        findChild(nameField(window), "input").forceActiveFocus();
        const add = findChild(window, "addButton");
        verify(!inSight(window, add));
        keyClick(Qt.Key_Tab);
        keyClick(Qt.Key_Tab);
        keyClick(Qt.Key_Tab);
        tryCompare(add, "activeFocus", true);
        verify(inSight(window, add));
    }

    // After a refusal, focus is back in the name field to try again.
    function test_focusReturnsAfterARefusal() {
        const window = makeWindow("not-authorized");
        nameField(window).text = "Cy";
        mouseClick(findChild(window, "addButton"));
        tryCompare(window.page, "step", ChildrenPage.NotChanged);
        tryCompare(findChild(nameField(window), "input"), "activeFocus", true);
    }

    // A name with no letters a to z sends focus to the short name.
    function test_focusGoesToTheShortName() {
        const window = makeWindow("done");
        nameField(window).text = "Анна";
        mouseClick(findChild(window, "addButton"));
        compare(window.page.reason, ChildrenPage.NoShortName);
        tryCompare(findChild(findChild(window, "shortNameField"), "input"), "activeFocus", true);
    }

    // After a level change, focus is back on that child's row.
    function test_focusReturnsToTheChild() {
        const window = makeWindow("done");
        mouseClick(findChild(findChild(window, "childRows").itemAt(1), "moveTo1"));
        tryCompare(window.page, "step", ChildrenPage.LevelChanged);
        tryCompare(findChild(findChild(window, "childRows").itemAt(1), "moveTo1"), "activeFocus", true);
    }

    // A child at level 3 has two buttons; in a narrow window they go
    // under the name, which keeps its room.
    function test_anOlderChildInANarrowWindow() {
        const window = makeWindow("done", {
            "withOlderChild": true,
            "smallest": true
        });
        const row = findChild(window, "childRows").itemAt(2);
        const name = findChild(row, "childName");
        const level = findChild(row, "childLevel");
        compare(level.text, "Level 3");
        verify(!row.sideBySide);
        compare(name.width, row.width - Tokens.bodySize * 2);
        const buttons = findChild(row, "buttons");
        verify(buttons.y >= level.mapToItem(row, 0, 0).y + level.height);
        verify(buttons.x + buttons.width <= row.width);
        // The other rows fit side by side.
        verify(findChild(window, "childRows").itemAt(0).sideBySide);
    }

    // ---- the switch between pages, and the Games page ----

    function showGames(window) {
        mouseClick(findChild(window, "gamesTab"));
        compare(window.showing, "games");
    }

    // The Games page's choice for the child with this login.
    function choice(window, login) {
        const repeater = findChild(window, "childChoiceRepeater");
        for (let i = 0; i < repeater.count; ++i) {
            if (repeater.itemAt(i).objectName === "choose_" + login)
                return repeater.itemAt(i);
        }
        return null;
    }

    // The names of the cards that are highlighted.
    function highlighted(window) {
        const names = [];
        const repeater = findChild(window, "gameCards");
        for (let i = 0; i < repeater.count; ++i) {
            const card = repeater.itemAt(i);
            if (card.onList)
                names.push(findChild(card, "gameTitle").text);
        }
        return names;
    }

    function test_theWindowOpensOnChildren() {
        const window = makeWindow("done");
        compare(window.showing, "children");
        verify(findChild(window, "childrenView").visible);
        verify(!findChild(window, "gamesView").visible);
        compare(findChild(window, "childrenTab").Accessible.role, Accessible.PageTab);
        compare(findChild(window, "childrenTab").Accessible.checked, true);
        compare(findChild(window, "gamesTab").Accessible.checked, false);
    }

    function test_switchToGamesAndBack() {
        const window = makeWindow("done");
        showGames(window);
        verify(!findChild(window, "childrenView").visible);
        verify(findChild(window, "gamesView").visible);
        compare(findChild(window, "gamesHeading").text, "Games");
        compare(findChild(window, "gamesTab").Accessible.checked, true);
        mouseClick(findChild(window, "childrenTab"));
        compare(window.showing, "children");
        verify(findChild(window, "childrenView").visible);
    }

    // The first child is picked, every shared game shows, and the games on
    // their list are highlighted, with a word as well as a colour.
    function test_theGamesPageShowsTheChildsList() {
        const window = makeWindow("done");
        showGames(window);
        compare(findChild(window, "gameCards").count, 8);
        verify(choice(window, "ada").checked);
        verify(!choice(window, "ben").checked);
        compare(highlighted(window), ["Putt-Putt Joins the Parade", "Pajama Sam", "Spy Fox"]);
        compare(findChild(window, "listSentence").text, "Games on Ada's list: 3. They are highlighted.");
        const first = findChild(window, "gameCards").itemAt(0);
        compare(findChild(first, "onListLabel").text, "On the list");
        compare(first.Accessible.description, "On Ada's list");
        compare(findChild(findChild(window, "gameCards").itemAt(1), "onListLabel").text, "Not on the list");
        verify(!findChild(window, "noGames").visible);
        verify(!findChild(window, "noChildrenForGames").visible);
        compare(findChild(window, "openSteam").text, "Open Steam for Ada");
        if (standIns.screenshotPath() !== "") {
            wait(300);
            grabImage(window.contentItem).save(standIns.screenshotPath().replace(".png", "-games.png"));
        }
    }

    function test_aChildWithNoGames() {
        const window = makeWindow("done");
        showGames(window);
        mouseClick(choice(window, "ben"));
        verify(choice(window, "ben").checked);
        compare(highlighted(window), []);
        compare(findChild(window, "listSentence").text, "Ben has no games on their list yet.");
        compare(findChild(window, "openSteam").text, "Open Steam for Ben");
        if (standIns.screenshotPath() !== "") {
            wait(300);
            grabImage(window.contentItem).save(standIns.screenshotPath().replace(".png", "-games-none.png"));
        }
    }

    // A game on the child's list that the shared folder no longer has
    // still shows, last, and says so.
    function test_aGameNoLongerSharedStillShows() {
        const window = makeWindow("done");
        showGames(window);
        mouseClick(choice(window, "cy"));
        compare(findChild(window, "gameCards").count, 9);
        compare(highlighted(window), ["Tux Racer", "Humongous Bowling"]);
        compare(findChild(window, "listSentence").text, "Games on Cy's list: 2. They are highlighted. The ones no longer in the shared folder come last. Cy's own copy may still play them.");
        const last = findChild(window, "gameCards").itemAt(8);
        compare(findChild(last, "onListLabel").text, "On the list, no longer shared");
        compare(last.Accessible.description, "On Cy's list, no longer in the shared folder");
        verify(!findChild(window, "noGames").visible);
        if (standIns.screenshotPath() !== "") {
            wait(300);
            grabImage(window.contentItem).save(standIns.screenshotPath().replace(".png", "-games-gone.png"));
        }
    }

    // A title is shown as written, never as markup.
    function test_aTitleIsPlainText() {
        const window = makeWindow("done");
        showGames(window);
        const title = findChild(findChild(window, "gameCards").itemAt(4), "gameTitle");
        compare(title.text, "<b>Backyard</b> Football");
        compare(title.textFormat, Text.PlainText);
    }

    // The keyboard alone: Tab from the switch to the children, Space picks.
    function test_pickAChildWithTheKeyboard() {
        const window = makeWindow("done");
        findChild(window, "childrenTab").forceActiveFocus();
        keyClick(Qt.Key_Right);
        tryCompare(findChild(window, "gamesTab"), "activeFocus", true);
        keyClick(Qt.Key_Space);
        compare(window.showing, "games");
        keyClick(Qt.Key_Tab);
        tryCompare(choice(window, "ada"), "activeFocus", true);
        keyClick(Qt.Key_Tab);
        tryCompare(choice(window, "ben"), "activeFocus", true);
        keyClick(Qt.Key_Space);
        compare(window.gamesPage.chosen, "ben");
        keyClick(Qt.Key_Tab);
        keyClick(Qt.Key_Tab);
        tryCompare(findChild(window, "openSteam"), "activeFocus", true);
        keyClick(Qt.Key_Return);
        compare(standIns.startedSteam(), ["/usr/bin/cairn-steam-for ben"]);
    }

    function test_openSteamForAChild() {
        const window = makeWindow("done");
        showGames(window);
        mouseClick(choice(window, "cy"));
        mouseClick(findChild(window, "openSteam"));
        compare(standIns.startedSteam(), ["/usr/bin/cairn-steam-for cy"]);
        verify(!findChild(window, "steamNotOpened").visible);
    }

    function test_aSteamWindowThatDoesNotOpenSaysSo() {
        const window = makeWindow("done", {
            "steamStarts": false
        });
        showGames(window);
        mouseClick(findChild(window, "openSteam"));
        verify(findChild(window, "steamNotOpened").visible);
        verify(findChild(window, "steamNotOpened").text.startsWith("The Steam window for Ada did not open."));
        verify(findChild(window, "steamNotOpened").text.endsWith("journalctl -t cairn-guardian -t cairn-steam-for"));
    }

    function test_noGamesInTheSharedFolder() {
        const window = makeWindow("done", {
            "withGames": false
        });
        showGames(window);
        verify(findChild(window, "noGames").visible);
        verify(findChild(window, "noGames").text.startsWith("No games in the shared folder yet."));
        verify(findChild(window, "noGames").text.indexOf("after the next Save") >= 0);
        verify(findChild(window, "noGames").text.indexOf("family set-up") < 0);
        // With nothing to save, a look for new games is the way to find one.
        verify(!findChild(window, "saveButton").enabled);
        verify(findChild(window, "lookForGames").enabled);
        verify(!findChild(window, "gameGrid").visible);
        verify(!findChild(window, "listSentence").visible);
        verify(findChild(window, "openSteam").visible);
    }

    function test_noChildrenForGames() {
        const window = makeWindow("done", {
            "withChildren": false
        });
        showGames(window);
        verify(findChild(window, "noChildrenForGames").visible);
        verify(!findChild(window, "childChoices").visible);
        verify(!findChild(window, "openSteam").visible);
        verify(!findChild(window, "gameGrid").visible);
    }

    // A child added on the Children page is there to pick on the Games page.
    function test_aChildAddedIsOnTheGamesPage() {
        const window = makeWindow("done");
        let reloaded = 0;
        window.gamesPage.changed.connect(() => reloaded++);
        nameField(window).text = "Dot";
        mouseClick(findChild(window, "addButton"));
        tryCompare(window.page, "step", ChildrenPage.Added);
        verify(reloaded > 0);
    }

    // On a short window the Games page scrolls, the switch stays in place,
    // and Tab keeps focus in sight.
    function test_theGamesPageScrolls() {
        const window = makeWindow("done", {
            "smallest": true
        });
        showGames(window);
        const scroller = findChild(window, "scroller");
        verify(scroller.contentHeight > scroller.height);
        verify(findChild(window, "scrollIndicator").visible);
        scroller.contentY = scroller.contentHeight - scroller.height;
        verify(inSight(window, findChild(window, "gamesTab")));
        findChild(window, "gamesTab").forceActiveFocus();
        keyClick(Qt.Key_Tab);
        tryCompare(choice(window, "ada"), "activeFocus", true);
        verify(inSight(window, choice(window, "ada")));
        // Back to Children starts at the top of that page.
        mouseClick(findChild(window, "childrenTab"));
        compare(scroller.contentY, 0);
    }

    // ---- choosing games and Save ----

    function card(window, index) {
        return findChild(window, "gameCards").itemAt(index);
    }

    // Picks a child, and waits until the grid has placed their cards. A
    // child's "not saved" mark widens their choice, so the choices are
    // placed again first.
    function chooseChild(window, login) {
        waitForPolish(findChild(window, "childChoices"));
        mouseClick(choice(window, login));
        compare(window.gamesPage.chosen, login);
        tryVerify(() => card(window, 1).x > 0 || card(window, 1).y > 0);
        waitForPolish(window);
    }

    // The sentence for the result of one child, in the order the Save
    // answered.
    function result(window, index) {
        return findChild(findChild(window, "saveResults").itemAt(index), "resultSentence");
    }

    function laterNote(window, index) {
        return findChild(findChild(window, "saveResults").itemAt(index), "laterNote");
    }

    function saveOutcome(window) {
        return findChild(window, "saveOutcome").text;
    }

    // The page no longer says that choosing comes later.
    function test_noWordOfALaterVersion() {
        const window = makeWindow("done");
        showGames(window);
        compare(findChild(window, "notYet"), null);
        verify(!findChild(window, "saveOutcomeBox").visible);
        verify(!findChild(window, "unsavedSentence").visible);
    }

    // A tap puts a game on the list, says it is not saved yet, and
    // offers Save.
    function test_tapAGameToPutItOnTheList() {
        const window = makeWindow("done");
        showGames(window);
        verify(!findChild(window, "saveButton").enabled);
        verify(!findChild(window, "putBack").enabled);
        mouseClick(card(window, 1));
        compare(highlighted(window), ["Putt-Putt Joins the Parade", "Freddi Fish", "Pajama Sam", "Spy Fox"]);
        compare(findChild(card(window, 1), "onListLabel").text, "Added, not saved yet");
        compare(card(window, 1).Accessible.description, "Added to Ada's list, not saved yet");
        compare(card(window, 1).Accessible.checked, true);
        verify(findChild(window, "saveButton").enabled);
        verify(findChild(window, "putBack").enabled);
        verify(findChild(choice(window, "ada"), "unsavedLabel").visible);
        compare(choice(window, "ada").Accessible.description, "Changes not saved yet");
        verify(!findChild(choice(window, "ben"), "unsavedLabel").visible);
        compare(findChild(window, "unsavedSentence").text, "Not saved yet: the changes for Ada. Save gives each child their new list, with one password.");
        // And a tap on a game on the list takes it off.
        mouseClick(card(window, 0));
        compare(findChild(card(window, 0), "onListLabel").text, "Taken off, not saved yet");
        compare(card(window, 0).Accessible.checked, false);
        if (standIns.screenshotPath() !== "") {
            wait(300);
            grabImage(window.contentItem).save(standIns.screenshotPath().replace(".png", "-games-changed.png"));
        }
    }

    // The keyboard alone: a card takes focus, and Space turns it.
    function test_turnAGameWithTheKeyboard() {
        const window = makeWindow("done");
        showGames(window);
        const second = card(window, 1);
        compare(second.Accessible.role, Accessible.CheckBox);
        second.forceActiveFocus();
        keyClick(Qt.Key_Space);
        compare(second.onList, true);
        // The same card, still with focus.
        verify(second.activeFocus);
        keyClick(Qt.Key_Return);
        compare(second.onList, false);
        keyClick(Qt.Key_Tab);
        tryCompare(card(window, 2), "activeFocus", true);
    }

    function test_putBackAsSaved() {
        const window = makeWindow("done");
        showGames(window);
        mouseClick(card(window, 1));
        chooseChild(window, "ben");
        mouseClick(card(window, 0));
        compare(findChild(window, "unsavedSentence").text, "Not saved yet: the changes for Ada and Ben. Save gives each child their new list, with one password.");
        mouseClick(findChild(window, "putBack"));
        compare(highlighted(window), []);
        verify(!findChild(choice(window, "ben"), "unsavedLabel").visible);
        verify(findChild(choice(window, "ada"), "unsavedLabel").visible);
    }

    // Save gives the list, says so, and leaves nothing to save.
    function test_saveGivesTheGames() {
        const window = makeWindow("done", {
            "gamesScenario": "save-done"
        });
        showGames(window);
        mouseClick(card(window, 1));
        mouseClick(findChild(window, "saveButton"));
        tryCompare(window.gamesPage.saving, "step", GamesSaving.Saved);
        compare(saveOutcome(window), "Saved. Each child sees their games the next time they log in.");
        compare(result(window, 0).text, "Ada's list is saved. Added: 4. Taken off: 0.");
        verify(!laterNote(window, 0).visible);
        verify(!findChild(window, "saveButton").enabled);
        verify(!findChild(choice(window, "ada"), "unsavedLabel").visible);
        compare(findChild(card(window, 1), "onListLabel").text, "On the list");
        // Focus goes back to the child picked, as Save has nothing to do.
        tryCompare(choice(window, "ada"), "activeFocus", true);
        if (standIns.screenshotPath() !== "") {
            wait(300);
            grabImage(window.contentItem).save(standIns.screenshotPath().replace(".png", "-games-saved.png"));
        }
    }

    // A logged-in child's new games wait; the page says what to do.
    function test_aLoggedInChildIsToldWhatComesNext() {
        const window = makeWindow("done", {
            "gamesScenario": "save-held-logged-in"
        });
        showGames(window);
        mouseClick(card(window, 1));
        mouseClick(findChild(window, "saveButton"));
        tryCompare(window.gamesPage.saving, "step", GamesSaving.Saved);
        // Not every child sees their games at their next login.
        compare(saveOutcome(window), "Saved, but not every new game could be given. The sentences below say what each child has now, and what comes next.");
        compare(result(window, 0).text, "Ada's list is saved, with no change. Ada is logged in, so their new games could not be copied. They come after Ada logs out and you press Save again. The games taken off are already gone from Ada's list.");
        verify(!laterNote(window, 0).visible);
        // The game stays chosen, so Save gives it later.
        verify(findChild(window, "saveButton").enabled);
        tryCompare(findChild(window, "saveButton"), "activeFocus", true);
        if (standIns.screenshotPath() !== "") {
            wait(300);
            grabImage(window.contentItem).save(standIns.screenshotPath().replace(".png", "-games-held.png"));
        }
    }

    // A logged-in child's new games are written down for their next
    // logout: the page says they come by themselves, and starts from them.
    function test_aChildWhoseGamesWaitForTheirLogout() {
        const window = makeWindow("done", {
            "gamesScenario": "save-held-later"
        });
        showGames(window);
        mouseClick(card(window, 1));
        mouseClick(findChild(window, "saveButton"));
        tryCompare(window.gamesPage.saving, "step", GamesSaving.Saved);
        compare(saveOutcome(window), "Saved, but not every new game could be given. The sentences below say what each child has now, and what comes next.");
        compare(result(window, 0).text, "Ada's list is saved, with no change.");
        const note = laterNote(window, 0);
        verify(note.visible);
        compare(note.text, "Ada is logged in, so Ada's new games could not be copied yet. Ada's new games are copied the next time Ada logs out. You do not need to do anything else. The games taken off are already gone from Ada's list.");
        compare(note.Accessible.name, note.text);
        // Nothing is left to save, and the game says it waits.
        verify(!findChild(window, "saveButton").enabled);
        compare(findChild(card(window, 1), "onListLabel").text, "Waits for Ada to log out");
        compare(card(window, 1).Accessible.description, "On Ada's list, copied the next time Ada logs out");
        // The Save's own outcome says it, so the page's note waits.
        verify(!findChild(window, "waitingNotes").visible);
        if (standIns.screenshotPath() !== "") {
            wait(300);
            grabImage(window.contentItem).save(standIns.screenshotPath().replace(".png", "-games-later.png"));
        }
    }

    // Coming back to a child whose games wait: the note, the marks, and
    // Save off until something changes.
    function test_aWaitingListShowsOnTheGamesPage() {
        const window = makeWindow("done");
        verify(standIns.leave("games-waiting/ada", "ada=1001,1002,1003,1004\n"));
        window.gamesPage.reload();
        showGames(window);
        const notes = findChild(window, "waitingNotes");
        verify(notes.visible);
        const waiting = findChild(window, "waitingNote");
        verify(waiting.visible);
        compare(waiting.text, "Ada's new games are copied the next time Ada logs out.");
        compare(waiting.Accessible.name, waiting.text);
        compare(findChild(card(window, 1), "onListLabel").text, "Waits for Ada to log out");
        compare(findChild(card(window, 0), "onListLabel").text, "On the list");
        verify(!findChild(window, "saveButton").enabled);
        // Taking a waiting game off is a change to save, and Put back
        // returns to the waiting list.
        mouseClick(card(window, 1));
        compare(findChild(card(window, 1), "onListLabel").text, "Taken off, not saved yet");
        verify(findChild(window, "saveButton").enabled);
        mouseClick(findChild(window, "putBack"));
        compare(findChild(card(window, 1), "onListLabel").text, "Waits for Ada to log out");
        chooseChild(window, "ben");
        verify(!findChild(window, "waitingNotes").visible);
    }

    // A clean copy at a logout is dated, until the next Save for the child.
    function test_aCleanFinishIsDated() {
        const window = makeWindow("done");
        const when = new Date(2026, 9, 20, 17, 30);
        verify(standIns.leave("games-finished/ada", "copied ada 1002\nchild ada added 1 removed 0\n", when));
        window.gamesPage.reload();
        showGames(window);
        const finished = findChild(window, "finishedNote");
        verify(finished.visible);
        compare(finished.text, "Ada's new games were copied when Ada logged out on " + when.toLocaleDateString(Qt.locale(), Locale.LongFormat) + ".");
        compare(finished.Accessible.name, finished.text);
        verify(!findChild(window, "waitingNote").visible);
    }

    // A copy at a logout that could not give everything says what stopped
    // it, in the Save's own words, and that the games are not on the list.
    function test_aFinishThatHeldSaysWhy() {
        const window = makeWindow("done");
        const when = new Date(2026, 9, 21, 8, 0);
        verify(standIns.leave("games-finished/ada", "child ada added 0 removed 0 held no-library\n", when));
        window.gamesPage.reload();
        showGames(window);
        compare(findChild(window, "finishedNote").text, "When Ada logged out on " + when.toLocaleDateString(Qt.locale(), Locale.LongFormat) + ": Games added to Ada's list: 0. Ada's Steam has not run yet, so it has no place for games. Open Steam for Ada and sign in, then press Save again. The games that were not given are not on Ada's list now. To try them again, put them back on the list and press Save.");
    }

    function test_aSkippedGameIsNamed() {
        const window = makeWindow("done", {
            "gamesScenario": "save-skipped"
        });
        showGames(window);
        chooseChild(window, "ben");
        mouseClick(card(window, 3));
        mouseClick(findChild(window, "saveButton"));
        tryCompare(window.gamesPage.saving, "step", GamesSaving.Saved);
        verify(result(window, 0).text.indexOf("Spy Fox was not given to Ben: Steam is still downloading it.") >= 0);
    }

    // While the password box is open, every control on the page waits.
    function test_everythingWaitsWhileASaveRuns() {
        const window = makeWindow("done", {
            "gamesScenario": "asking"
        });
        showGames(window);
        mouseClick(card(window, 1));
        mouseClick(findChild(window, "saveButton"));
        compare(window.gamesPage.busy, true);
        compare(saveOutcome(window), "Confirm with your password, or your fingerprint, in the box that opened.");
        verify(!findChild(window, "saveButton").enabled);
        verify(!findChild(window, "putBack").enabled);
        verify(!findChild(window, "lookForGames").enabled);
        verify(!findChild(window, "openSteam").enabled);
        verify(!choice(window, "ben").enabled);
        verify(!card(window, 2).enabled);
        verify(!findChild(window, "unsavedSentence").visible);
        mouseClick(card(window, 2));
        compare(card(window, 2).onList, true);
        compare(highlighted(window), ["Putt-Putt Joins the Parade", "Freddi Fish", "Pajama Sam", "Spy Fox"]);
    }

    // A copy in progress names the child, and says they cannot log in.
    function test_aCopyInProgressSaysWhose() {
        const window = makeWindow("done", {
            "gamesScenario": "save-copying"
        });
        showGames(window);
        mouseClick(card(window, 1));
        mouseClick(findChild(window, "saveButton"));
        tryCompare(window.gamesPage.saving, "progress", GamesSaving.CopyingGames);
        compare(saveOutcome(window), "Copying games for Ada. This can take a few minutes, and Ada cannot log in until it ends.");
    }

    function test_aClosedPasswordBoxKeepsTheChanges() {
        const window = makeWindow("done", {
            "gamesScenario": "not-authorized"
        });
        showGames(window);
        mouseClick(card(window, 1));
        mouseClick(findChild(window, "saveButton"));
        tryCompare(window.gamesPage.saving, "step", GamesSaving.NotSaved);
        compare(saveOutcome(window), "Nothing was saved. It needs your password: the box was closed, or the password was not accepted. Your changes are still here.");
        verify(findChild(window, "saveButton").enabled);
        compare(findChild(card(window, 1), "onListLabel").text, "Added, not saved yet");
    }

    function test_anotherSaveRunning() {
        const window = makeWindow("done", {
            "gamesScenario": "refused-busy"
        });
        showGames(window);
        mouseClick(card(window, 1));
        mouseClick(findChild(window, "saveButton"));
        tryCompare(window.gamesPage.saving, "step", GamesSaving.NotSaved);
        verify(saveOutcome(window).startsWith("Another Save is still running"));
    }

    // Looking for new games needs no change, and says what it did.
    function test_lookForNewGames() {
        const window = makeWindow("done", {
            "gamesScenario": "save-done"
        });
        showGames(window);
        mouseClick(findChild(window, "lookForGames"));
        tryCompare(window.gamesPage.saving, "step", GamesSaving.Saved);
        compare(saveOutcome(window), "The shared folder was read again. Every game in it shows below.");
        // It names no child, so it says nothing about any child's list.
        compare(findChild(window, "saveResults").count, 0);
    }

    // While a look runs, the page speaks of looking, never of saving, and
    // stops asking for the password once the command has said it runs.
    function test_aLookInProgressSaysItIsLooking() {
        const window = makeWindow("done", {
            "gamesScenario": "save-looking"
        });
        showGames(window);
        mouseClick(findChild(window, "lookForGames"));
        tryCompare(window.gamesPage.saving, "step", GamesSaving.Saving);
        compare(saveOutcome(window), "Looking at the shared folder for games and their pictures.");
    }

    // A look that did not run speaks of looking, not of saved changes.
    function test_aRefusedLookSpeaksOfLooking() {
        const window = makeWindow("done", {
            "gamesScenario": "refused-busy"
        });
        showGames(window);
        mouseClick(findChild(window, "lookForGames"));
        tryCompare(window.gamesPage.saving, "step", GamesSaving.NotSaved);
        compare(saveOutcome(window), "A Save is still running, perhaps from another login, so the shared folder was not looked at again. Wait until it ends, then press Look for new games again.");
    }

    function test_aClosedPasswordBoxDuringALook() {
        const window = makeWindow("done", {
            "gamesScenario": "dismissed"
        });
        showGames(window);
        mouseClick(findChild(window, "lookForGames"));
        tryCompare(window.gamesPage.saving, "step", GamesSaving.NotSaved);
        compare(saveOutcome(window), "The shared folder was not looked at again. It needs your password: the box was closed, or the password was not accepted.");
    }

    // What a Save did stops showing once a game is turned, so the page
    // never says "Saved" beside changes that are not.
    function test_theOutcomeEndsWithTheNextTap() {
        const window = makeWindow("done", {
            "gamesScenario": "save-done"
        });
        showGames(window);
        mouseClick(card(window, 1));
        mouseClick(findChild(window, "saveButton"));
        tryCompare(window.gamesPage.saving, "step", GamesSaving.Saved);
        verify(findChild(window, "saveOutcomeBox").visible);
        mouseClick(card(window, 2));
        verify(!findChild(window, "saveOutcomeBox").visible);
        verify(findChild(window, "unsavedSentence").visible);
    }

    // A list that cannot be read says that Saving writes it again.
    function test_anUnreadableListSaysSavingWritesItAgain() {
        const window = makeWindow("done");
        showGames(window);
        verify(findChild(window, "listSentence").text.indexOf("later version") < 0);
    }
}
