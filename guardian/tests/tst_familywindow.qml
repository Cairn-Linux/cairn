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
            "gamesPage": standIns.gamesPage(given.withChildren !== false, given.withGames !== false, given.steamStarts !== false, given.gamesScenario === undefined ? "" : given.gamesScenario, given.loggedIn === undefined ? [] : given.loggedIn, given.withOwnSteamGames === true),
            "accountPage": standIns.accountPage(),
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

    function showAccount(window) {
        mouseClick(findChild(window, "accountTab"));
        compare(window.showing, "account");
        waitForPolish(window);
    }

    function typeInto(field, text) {
        findChild(field, "input").forceActiveFocus();
        for (let i = 0; i < text.length; ++i)
            keyClick(text[i]);
    }

    function passwordOutcome(window) {
        return findChild(window, "passwordOutcomeText").text;
    }

    // The third page: the grown-up's own name, Plasma's pictures and the
    // password form, and nothing said yet. With no picture, the frame says
    // so on screen as well as to a screen reader, so it never looks like a
    // picture that did not load.
    function test_yourAccountPage() {
        const window = makeWindow("done");
        compare(findChild(window, "accountTab").text, "Your account");
        compare(findChild(window, "accountTab").Accessible.role, Accessible.PageTab);
        showAccount(window);
        verify(findChild(window, "accountView").visible);
        verify(!findChild(window, "childrenView").visible);
        verify(!findChild(window, "gamesView").visible);
        compare(findChild(window, "accountTab").Accessible.checked, true);
        compare(findChild(window, "accountHeading").text, "Your account");
        compare(findChild(window, "ownName").text, "Guardian");
        compare(findChild(window, "ownPicture").Accessible.name, "No picture yet");
        const noPicture = findChild(window, "noPicture");
        verify(noPicture.visible);
        compare(noPicture.text, "No picture yet");
        verify(noPicture.contentWidth <= noPicture.width);
        verify(noPicture.contentHeight <= noPicture.height);
        const tiles = findChild(window, "pictureTiles");
        compare(tiles.count, 3);
        compare(tiles.itemAt(0).Accessible.name, "Use the picture Artist Konqi");
        verify(!findChild(window, "pictureOutcome").visible);
        verify(!findChild(window, "passwordOutcome").visible);
        compare(findChild(findChild(window, "newPasswordField"), "input").echoMode, TextInput.Password);
        compare(findChild(findChild(window, "againPasswordField"), "input").echoMode, TextInput.Password);
        compare(findChild(findChild(window, "newPasswordField"), "input").Accessible.passwordEdit, true);
        if (standIns.screenshotPath() !== "") {
            wait(200);
            grabImage(window.contentItem).save(standIns.screenshotPath().replace(".png", "-account.png"));
        }
    }

    // Type it twice, Enter, and the password is changed; the fields are
    // emptied, so the new password is not left on screen.
    function test_changeYourPassword() {
        const window = makeWindow("done");
        showAccount(window);
        typeInto(findChild(window, "newPasswordField"), "new pass");
        keyClick(Qt.Key_Return);
        tryCompare(findChild(findChild(window, "againPasswordField"), "input"), "activeFocus", true);
        typeInto(findChild(window, "againPasswordField"), "new pass");
        keyClick(Qt.Key_Return);
        tryCompare(window.accountPage, "step", AccountPage.PasswordChanged);
        compare(passwordOutcome(window), "Your password is changed. Use the new one from now on: to log in, to unlock the screen and in every password box.");
        compare(findChild(window, "newPasswordField").text, "");
        compare(findChild(window, "againPasswordField").text, "");
        tryCompare(findChild(findChild(window, "newPasswordField"), "input"), "activeFocus", true);
        if (standIns.screenshotPath() !== "") {
            wait(200);
            grabImage(window.contentItem).save(standIns.screenshotPath().replace(".png", "-account-password.png"));
        }
    }

    // Two different passwords: nothing is asked, the fields stay, and
    // focus goes to the second.
    function test_twoDifferentPasswords() {
        const window = makeWindow("done");
        showAccount(window);
        findChild(window, "newPasswordField").text = "one";
        findChild(window, "againPasswordField").text = "two";
        mouseClick(findChild(window, "changePasswordButton"));
        compare(window.accountPage.step, AccountPage.PasswordNotChanged);
        compare(passwordOutcome(window), "The two new passwords are not the same. Type the new password again in the second box.");
        compare(findChild(window, "newPasswordField").text, "one");
        tryCompare(findChild(findChild(window, "againPasswordField"), "input"), "activeFocus", true);
    }

    function test_aPasswordNotAccepted() {
        const window = makeWindow("not-authorized");
        showAccount(window);
        findChild(window, "newPasswordField").text = "new";
        findChild(window, "againPasswordField").text = "new";
        mouseClick(findChild(window, "changePasswordButton"));
        tryCompare(window.accountPage, "step", AccountPage.PasswordNotChanged);
        compare(passwordOutcome(window), "Your password did not change. It needs the password you use now: the box was closed, or the password was not accepted.");
        // Kept, so pressing the button again is enough.
        compare(findChild(window, "newPasswordField").text, "new");
    }

    // While the password box is open, everything on the page waits.
    function test_everythingWaitsWhileThePasswordChanges() {
        const window = makeWindow("asking");
        showAccount(window);
        findChild(window, "newPasswordField").text = "new";
        findChild(window, "againPasswordField").text = "new";
        mouseClick(findChild(window, "changePasswordButton"));
        compare(window.accountPage.step, AccountPage.Asking);
        compare(passwordOutcome(window), "Type the password you use now in the box that opened.");
        verify(!findChild(window, "passwordForm").enabled);
        verify(!findChild(window, "pictureChoices").enabled);
        verify(!findChild(window, "ownPictureButton").enabled);
    }

    function test_theCommandsOwnPasswordRefusals_data() {
        return [
            {
                tag: "failed",
                scenario: "refused-failed",
                words: "Your password did not change. Trying again is safe. If it fails again, the reason is in the system log. In a terminal: journalctl -t cairn-family -t cairn-guardian"
            },
            {
                tag: "not-a-grown-up",
                scenario: "refused-not-a-grown-up",
                words: "Your password did not change. Only a grown-up's account can change its password here."
            },
            {
                tag: "unknown",
                scenario: "garbage",
                words: "Cairn's family command did not answer as expected, so it is not known whether your password changed. Try the new one the next time a box asks; if it is not accepted, the old one still works. The reason is in the system log. In a terminal: journalctl -t cairn-family -t cairn-guardian"
            }
        ];
    }

    function test_theCommandsOwnPasswordRefusals(data) {
        const window = makeWindow(data.scenario);
        showAccount(window);
        findChild(window, "newPasswordField").text = "new";
        findChild(window, "againPasswordField").text = "new";
        mouseClick(findChild(window, "changePasswordButton"));
        tryCompare(window.accountPage, "step", AccountPage.PasswordNotChanged);
        compare(passwordOutcome(window), data.words);
    }

    // With no account service to take it, a picture chosen says so, under
    // the pictures.
    function test_aPictureNotTaken() {
        const window = makeWindow("done");
        showAccount(window);
        mouseClick(findChild(window, "pictureTiles").itemAt(1));
        compare(window.accountPage.step, AccountPage.PictureNotChanged);
        verify(findChild(window, "pictureOutcome").visible);
        compare(findChild(window, "pictureOutcomeText").text, "Your picture stays as it was: the computer's account service did not take it. Trying again is safe. The reason is in the system log. In a terminal: journalctl -t cairn-guardian");
        verify(!findChild(window, "passwordOutcome").visible);
    }

    // In a short window, what a change did scrolls into sight.
    function test_theAccountOutcomeComesIntoSight() {
        const window = makeWindow("done", {
            "smallest": true
        });
        showAccount(window);
        findChild(window, "pictureTiles").itemAt(0).chosen();
        compare(window.accountPage.step, AccountPage.PictureNotChanged);
        tryVerify(() => inSight(window, findChild(window, "pictureOutcome")));
        findChild(window, "scroller").contentY = 0;
        findChild(window, "newPasswordField").text = "one";
        findChild(window, "againPasswordField").text = "two";
        findChild(window, "changePasswordButton").activated();
        tryVerify(() => inSight(window, findChild(window, "passwordOutcome")));
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
        compare(findChild(window, "listSentence").text, "Games on Ada's list: 3. Each game on the list is highlighted.");
        const first = findChild(window, "gameCards").itemAt(0);
        compare(findChild(first, "onListLabel").text, "On the list");
        compare(first.Accessible.description, "On Ada's list");
        compare(findChild(findChild(window, "gameCards").itemAt(1), "onListLabel").text, "Not on the list");
        verify(!findChild(window, "noGames").visible);
        verify(!findChild(window, "noChildrenForGames").visible);
        verify(!findChild(window, "ownSteamGames").visible);
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
        compare(findChild(window, "listSentence").text, "Games on Cy's list: 2. Each game on the list is highlighted. The ones no longer in the shared folder come last. Cy's own copy may still play them.");
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

    // The keyboard alone: Tab from the switch, past its last tab, to the
    // children, Space picks.
    function test_pickAChildWithTheKeyboard() {
        const window = makeWindow("done");
        findChild(window, "childrenTab").forceActiveFocus();
        keyClick(Qt.Key_Right);
        tryCompare(findChild(window, "gamesTab"), "activeFocus", true);
        keyClick(Qt.Key_Space);
        compare(window.showing, "games");
        keyClick(Qt.Key_Tab);
        tryCompare(findChild(window, "accountTab"), "activeFocus", true);
        keyClick(Qt.Key_Tab);
        tryCompare(choice(window, "ada"), "activeFocus", true);
        keyClick(Qt.Key_Tab);
        tryCompare(choice(window, "ben"), "activeFocus", true);
        keyClick(Qt.Key_Space);
        compare(window.gamesPage.chosen, "ben");
        // Past Cy and Look for new games; Save and Put back are off.
        keyClick(Qt.Key_Tab);
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

    // Games in the Guardian's own Steam are named, with how to move them;
    // Steam's runtime is not.
    function test_gamesInTheGuardiansOwnSteam() {
        const window = makeWindow("done", {
            "withOwnSteamGames": true
        });
        showGames(window);
        const note = findChild(window, "ownSteamGames");
        verify(note.visible);
        verify(note.text.startsWith("In your own Steam library, not the shared folder: Celeste and Portal 2."));
        verify(note.text.indexOf("Runtime") < 0);
        verify(note.text.indexOf("Storage") >= 0);
        verify(note.text.indexOf("/var/lib/cairn/steam-library") >= 0);
        compare(note.Accessible.name, note.text);
        if (standIns.screenshotPath() !== "") {
            wait(300);
            grabImage(window.contentItem).save(standIns.screenshotPath().replace(".png", "-games-own-steam.png"));
        }
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
        findChild(window, "accountTab").forceActiveFocus();
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
        return findChild(window, "saveResults").itemAt(index);
    }

    function progressWindow(window) {
        return findChild(window, "saveProgress");
    }

    // Close in the Save's window, once the Save has ended.
    function closeProgress(window) {
        const close = findChild(window, "closeSaveProgress");
        tryVerify(() => close.visible);
        waitForPolish(window);
        mouseClick(close);
        verify(!progressWindow(window).visible);
    }

    function saveOutcome(window) {
        return findChild(window, "saveOutcome").text;
    }

    // The page no longer says that choosing comes later.
    function test_noWordOfALaterVersion() {
        const window = makeWindow("done");
        showGames(window);
        compare(findChild(window, "notYet"), null);
        verify(!progressWindow(window).visible);
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

    // One game is a common count: a new child's list starts empty, so the
    // first game a parent picks for them gives this sentence. It said
    // "They are highlighted" for that one game too.
    function test_oneGameOnTheList() {
        const window = makeWindow("done");
        showGames(window);
        chooseChild(window, "ben");
        mouseClick(card(window, 0));
        compare(highlighted(window), ["Putt-Putt Joins the Parade"]);
        compare(findChild(window, "listSentence").text, "Games on Ben's list: 1. Each game on the list is highlighted.");
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

    // A grown-up holding Enter or Space on a game card turns it once. Each
    // of the keyboard's repeats turned it the other way, about 25 times a
    // second, and the grid jumped up and down until the key came up.
    function test_aHeldKeyTurnsAGameOnce_data() {
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

    function test_aHeldKeyTurnsAGameOnce(data) {
        const window = makeWindow("done");
        showGames(window);
        const second = card(window, 1);
        let turns = 0;
        second.turned.connect(() => ++turns);
        second.forceActiveFocus();
        keyPress(data.key);
        for (let i = 0; i < 5; ++i)
            heldKey.repeat(window, data.key);
        keyRelease(data.key);
        verify(second.activeFocus);
        compare(turns, 1);
        compare(second.onList, true);
        compare(findChild(second, "onListLabel").text, "Added, not saved yet");
    }

    // The window's other controls take a held key once too, so they all
    // work alike. `index` picks one control a Repeater made.
    function test_aHeldKeyPressesOnce_data() {
        return [
            {
                tag: "button",
                page: "games",
                control: "openSteam",
                signal: "activated"
            },
            {
                tag: "child",
                page: "games",
                control: "childChoiceRepeater",
                index: 1,
                signal: "chosen"
            },
            {
                tag: "level",
                page: "children",
                control: "level2",
                signal: "chosen"
            },
            {
                tag: "picture",
                page: "account",
                control: "pictureTiles",
                index: 0,
                signal: "chosen"
            }
        ];
    }

    function test_aHeldKeyPressesOnce(data) {
        const window = makeWindow("done");
        if (data.page === "games")
            showGames(window);
        else if (data.page === "account")
            showAccount(window);
        const found = findChild(window, data.control);
        const control = data.index === undefined ? found : found.itemAt(data.index);
        let presses = 0;
        control[data.signal].connect(() => ++presses);
        control.forceActiveFocus();
        keyPress(Qt.Key_Space);
        for (let i = 0; i < 5; ++i)
            heldKey.repeat(window, Qt.Key_Space);
        keyRelease(Qt.Key_Space);
        verify(control.activeFocus);
        compare(presses, 1);
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
        verify(!findChild(window, "lidWarning").visible);
        tryCompare(findChild(window, "closeSaveProgress"), "activeFocus", true);
        if (standIns.screenshotPath() !== "") {
            wait(300);
            grabImage(window.contentItem).save(standIns.screenshotPath().replace(".png", "-games-saved.png"));
        }
        closeProgress(window);
        verify(!findChild(window, "saveButton").enabled);
        verify(!findChild(choice(window, "ada"), "unsavedLabel").visible);
        compare(findChild(card(window, 1), "onListLabel").text, "On the list");
        // Focus goes back to the child picked, as Save has nothing to do.
        tryCompare(choice(window, "ada"), "activeFocus", true);
    }

    // A child who logged in after the page last looked: the copy is held,
    // and the result says what to do.
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
        // Nothing was taken off, so the sentence does not say so.
        compare(result(window, 0).text, "Ada's list is saved, with no change. Ada is logged in, so their new games could not be copied. They come after Ada logs out and you press Save again.");
        closeProgress(window);
        // The game stays chosen, so Save can give it once Ada logs out.
        verify(findChild(window, "saveButton").enabled);
        tryCompare(findChild(window, "saveButton"), "activeFocus", true);
    }

    // The same for a logged-in child held without a wait.
    function test_aHeldSentenceSaysWhatWasTakenOff() {
        const window = makeWindow("done", {
            "gamesScenario": "save-held-logged-in"
        });
        showGames(window);
        mouseClick(card(window, 0));
        mouseClick(card(window, 1));
        mouseClick(findChild(window, "saveButton"));
        tryCompare(window.gamesPage.saving, "step", GamesSaving.Saved);
        compare(result(window, 0).text, "Ada's list is saved. Added: 0. Taken off: 1. Ada is logged in, so their new games could not be copied. They come after Ada logs out and you press Save again. The games taken off are already gone from Ada's list.");
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

    // While the password box is open, every control on the page waits,
    // behind the Save's own window.
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

    // A copy in progress names the child, says they cannot log in, and
    // warns not to log out or close the lid.
    function test_aCopyInProgressSaysWhose() {
        const window = makeWindow("done", {
            "gamesScenario": "save-copying"
        });
        showGames(window);
        mouseClick(card(window, 1));
        mouseClick(findChild(window, "saveButton"));
        tryCompare(window.gamesPage.saving, "progress", GamesSaving.CopyingGames);
        compare(saveOutcome(window), "Copying games for Ada. This can take a few minutes, and Ada cannot log in until it ends.");
        verify(progressWindow(window).visible);
        compare(findChild(window, "saveProgressHeading").text, "Saving games");
        verify(findChild(window, "lidWarning").visible);
        compare(findChild(window, "lidWarning").text, "Do not log out or close the lid until this is finished.");
        verify(!findChild(window, "closeSaveProgress").visible);
        if (standIns.screenshotPath() !== "") {
            wait(300);
            grabImage(window.contentItem).save(standIns.screenshotPath().replace(".png", "-games-copying.png"));
        }
        // Escape does not put it away while the copy runs, and nothing on
        // the page behind it takes a click.
        keyClick(Qt.Key_Escape);
        verify(progressWindow(window).visible);
        mouseClick(findChild(window, "childrenTab"));
        compare(window.showing, "games");
    }

    // ---- Steam's tools (ADR-0046) ----

    // While Steam's tools are taken from the parent's own Steam, a Save
    // says so, and so does a look for new games.
    function test_takingTheToolsSaysSo() {
        const window = makeWindow("done", {
            "gamesScenario": "save-sharing-tools"
        });
        showGames(window);
        mouseClick(card(window, 1));
        mouseClick(findChild(window, "saveButton"));
        tryCompare(window.gamesPage.saving, "progress", GamesSaving.SharingTools);
        compare(saveOutcome(window), "Getting the tools Steam runs games with from your own Steam. This usually takes a few seconds.");
        verify(findChild(window, "lidWarning").visible);
    }

    function test_aLookTakingTheToolsSaysSo() {
        const window = makeWindow("done", {
            "gamesScenario": "save-sharing-tools"
        });
        showGames(window);
        mouseClick(findChild(window, "lookForGames"));
        tryCompare(window.gamesPage.saving, "progress", GamesSaving.SharingTools);
        compare(saveOutcome(window), "Getting the tools Steam runs games with from your own Steam. This usually takes a few seconds.");
        verify(!findChild(window, "lidWarning").visible);
    }

    // Copying the tools into a child's Steam names the child, who cannot
    // log in until it ends.
    function test_copyingAChildsToolsSaysWhose() {
        const window = makeWindow("done", {
            "gamesScenario": "save-tools-copying"
        });
        showGames(window);
        mouseClick(card(window, 1));
        mouseClick(findChild(window, "saveButton"));
        tryCompare(window.gamesPage.saving, "progress", GamesSaving.CopyingTools);
        compare(saveOutcome(window), "Copying the tools Steam runs games with into Ada's Steam. This usually takes a few seconds, and Ada cannot log in until it ends.");
        // The games copied so far still show, and no tool among them.
        tryCompare(findChild(window, "copiedGames"), "count", 1);
        compare(findChild(window, "copiedGames").itemAt(0).text, "Copied Putt-Putt Joins the Parade for Ada.");
    }

    function test_eachChildsToolsSentence_data() {
        return [
            {
                "tag": "copied",
                "scenario": "save-tools",
                "words": "Copied the tools Steam runs games with into Ada's Steam, so Ada's first game does not have to download them. A tool Cairn has no copy of, or a newer version Valve has made since, still downloads the first time a game needs it, which can make that game wait several minutes."
            },
            {
                "tag": "missing",
                "scenario": "save-tools-missing",
                "words": "Ada's Steam may not have every tool Steam runs games with yet. The first game that needs a missing one waits while Ada's Steam downloads it, which can take several minutes. If that game does not start, log Ada out and back in, then try it again. The record is in the system log. In a terminal: journalctl -t cairn-steam-visit"
            },
            {
                "tag": "none to copy",
                "scenario": "save-no-tools",
                "words": "Cairn had no copies of the tools Steam runs games with to give Ada's Steam. If Ada's Steam does not have them yet, Ada's first game waits while it downloads them, which can take several minutes. If that game does not start, log Ada out and back in, then try it again."
            }
        ];
    }

    // What each child's copy did with the tools comes last in their
    // result. A tool never makes the Save say a game was not given.
    function test_eachChildsToolsSentence(data) {
        const window = makeWindow("done", {
            "gamesScenario": data.scenario
        });
        showGames(window);
        mouseClick(card(window, 1));
        mouseClick(findChild(window, "saveButton"));
        tryCompare(window.gamesPage.saving, "step", GamesSaving.Saved);
        compare(saveOutcome(window), "Saved. Each child sees their games the next time they log in.");
        compare(result(window, 0).text, "Ada's list is saved. Added: 4. Taken off: 0. " + data.words);
    }

    // A tool the parent's Steam could not give is said once, for the whole
    // Save, after what it saved.
    function test_aToolLeftOutIsSaidForTheSave() {
        const window = makeWindow("done", {
            "gamesScenario": "save-tools-left-out"
        });
        showGames(window);
        mouseClick(card(window, 1));
        mouseClick(findChild(window, "saveButton"));
        tryCompare(window.gamesPage.saving, "step", GamesSaving.Saved);
        compare(saveOutcome(window), "Saved. Each child sees their games the next time they log in. Not every tool Steam runs games with could be taken from your own Steam, so a child who got new games may not have every tool yet. Their Steam downloads what is missing the first time a game needs it, which can take several minutes. If that game does not start, log the child out and back in, then try it again. The reason is in the system log. In a terminal: journalctl -t cairn-family");
        verify(result(window, 0).text.startsWith("Ada's list is saved. Added: 4. Taken off: 0. Copied the tools Steam runs games with into Ada's Steam,"));
        if (standIns.screenshotPath() !== "") {
            wait(300);
            grabImage(window.contentItem).save(standIns.screenshotPath().replace(".png", "-games-tools.png"));
        }
    }

    // A look gives no child anything, so it says nothing about the tools.
    function test_aLookSaysNothingAboutToolsLeftOut() {
        const window = makeWindow("done", {
            "gamesScenario": "save-tools-left-out"
        });
        showGames(window);
        mouseClick(findChild(window, "lookForGames"));
        tryCompare(window.gamesPage.saving, "step", GamesSaving.Saved);
        compare(saveOutcome(window), "The shared folder was read again. Every game in it shows on the page.");
    }

    // Each game copied is named as it comes.
    function test_theProgressWindowNamesEachGameCopied() {
        const window = makeWindow("done", {
            "gamesScenario": "save-copied-then-copying"
        });
        showGames(window);
        mouseClick(card(window, 1));
        mouseClick(findChild(window, "saveButton"));
        tryCompare(findChild(window, "copiedGames"), "count", 2);
        compare(findChild(window, "copiedGames").itemAt(0).text, "Copied Putt-Putt Joins the Parade for Ada.");
        compare(findChild(window, "copiedGames").itemAt(1).text, "Ada already had Freddi Fish.");
    }

    // Escape puts the window away once the Save has ended, and focus goes
    // back to the page.
    function test_escapeClosesTheEndedSave() {
        const window = makeWindow("done", {
            "gamesScenario": "not-authorized"
        });
        showGames(window);
        mouseClick(card(window, 1));
        mouseClick(findChild(window, "saveButton"));
        tryCompare(findChild(window, "closeSaveProgress"), "activeFocus", true);
        keyClick(Qt.Key_Tab);
        verify(findChild(window, "closeSaveProgress").activeFocus);
        keyClick(Qt.Key_Escape);
        verify(!progressWindow(window).visible);
        compare(window.gamesPage.saving.step, GamesSaving.Ready);
        tryCompare(findChild(window, "saveButton"), "activeFocus", true);
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
        compare(saveOutcome(window), "The shared folder was read again. Every game in it shows on the page.");
        compare(findChild(window, "saveProgressHeading").text, "Looking for new games");
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
        // A look copies nothing, so it has no warning about the lid.
        verify(!findChild(window, "lidWarning").visible);
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

    // What a Save did shows until Close; the page waits behind it.
    function test_theOutcomeStaysUntilClose() {
        const window = makeWindow("done", {
            "gamesScenario": "save-done"
        });
        showGames(window);
        mouseClick(card(window, 1));
        mouseClick(findChild(window, "saveButton"));
        tryCompare(window.gamesPage.saving, "step", GamesSaving.Saved);
        mouseClick(card(window, 2));
        compare(card(window, 2).onList, true);
        verify(progressWindow(window).visible);
        closeProgress(window);
        mouseClick(card(window, 2));
        compare(card(window, 2).onList, false);
        verify(findChild(window, "unsavedSentence").visible);
    }

    // ---- a child who is logged in ----

    // The child's games are greyed under a card that says why; the other
    // children and Look for new games stay.
    function test_aLoggedInChildsGamesAreCovered() {
        const window = makeWindow("done", {
            "loggedIn": ["ada"]
        });
        showGames(window);
        verify(window.gamesPage.chosenLoggedIn);
        verify(findChild(window, "loggedInCover").visible);
        compare(findChild(window, "loggedInSentence").text, "Ada is still logged in. Log Ada out, then choose their games.");
        compare(findChild(window, "logOutChild").text, "Log Ada out");
        compare(findChild(window, "logOutChild").Accessible.name, "Log Ada out");
        verify(!findChild(window, "childGames").enabled);
        compare(findChild(window, "childGames").opacity, Tokens.coveredOpacity);
        mouseClick(card(window, 1));
        compare(card(window, 1).onList, false);
        verify(findChild(window, "lookForGames").enabled);
        if (standIns.screenshotPath() !== "") {
            wait(300);
            grabImage(window.contentItem).save(standIns.screenshotPath().replace(".png", "-games-logged-in.png"));
        }
        // Ben is not logged in: his games show.
        chooseChild(window, "ben");
        verify(!findChild(window, "loggedInCover").visible);
        verify(findChild(window, "childGames").enabled);
        compare(findChild(window, "childGames").opacity, 1);
    }

    // Log out warns first, with focus on Not now; then the password and the
    // logout, and the games show again.
    function test_logAChildOut() {
        const window = makeWindow("done", {
            "loggedIn": ["ada"],
            "gamesScenario": "log-out-done"
        });
        showGames(window);
        mouseClick(findChild(window, "logOutChild"));
        compare(findChild(window, "loggedInSentence").text, "Log Ada out? Anything Ada has not saved in a game or program is lost. Steam is asked to save first.");
        tryCompare(findChild(window, "notNowLogOut"), "activeFocus", true);
        compare(findChild(window, "confirmLogOut").text, "Log Ada out");
        if (standIns.screenshotPath() !== "") {
            wait(300);
            grabImage(window.contentItem).save(standIns.screenshotPath().replace(".png", "-games-log-out.png"));
        }
        // The buttons appear together; until the row is laid out, Log Ada
        // out sits where Not now is.
        waitForPolish(window);
        mouseClick(findChild(window, "confirmLogOut"));
        tryVerify(() => !window.gamesPage.chosenLoggedIn);
        verify(!findChild(window, "loggedInCover").visible);
        verify(findChild(window, "childGames").enabled);
        tryCompare(choice(window, "ada"), "activeFocus", true);
        mouseClick(card(window, 1));
        compare(card(window, 1).onList, true);
    }

    function test_notNowLogsNobodyOut() {
        const window = makeWindow("done", {
            "loggedIn": ["ada"],
            "gamesScenario": "log-out-done"
        });
        showGames(window);
        mouseClick(findChild(window, "logOutChild"));
        tryCompare(findChild(window, "notNowLogOut"), "activeFocus", true);
        keyClick(Qt.Key_Space);
        compare(findChild(window, "loggedInSentence").text, "Ada is still logged in. Log Ada out, then choose their games.");
        tryCompare(findChild(window, "logOutChild"), "activeFocus", true);
        verify(window.gamesPage.chosenLoggedIn);
    }

    // A program that will not close: the parent is told to restart.
    function test_aStuckLogoutSaysRestart() {
        const window = makeWindow("done", {
            "loggedIn": ["ada"],
            "gamesScenario": "log-out-held-stuck"
        });
        showGames(window);
        mouseClick(findChild(window, "logOutChild"));
        tryCompare(findChild(window, "notNowLogOut"), "activeFocus", true);
        waitForPolish(window);
        mouseClick(findChild(window, "confirmLogOut"));
        tryCompare(window.gamesPage.loggingOut, "step", ChildLogOut.NotLoggedOut);
        compare(findChild(window, "loggedInSentence").text, "One of Ada's programs would not close, so Ada is not fully logged out. Restart the computer, then choose Ada's games.");
        tryCompare(findChild(window, "closeLogOut"), "activeFocus", true);
        verify(!findChild(window, "childGames").enabled);
        mouseClick(findChild(window, "closeLogOut"));
        compare(findChild(window, "loggedInSentence").text, "Ada is still logged in. Log Ada out, then choose their games.");
    }

    function test_aClosedPasswordBoxLogsNobodyOut() {
        const window = makeWindow("done", {
            "loggedIn": ["ada"],
            "gamesScenario": "dismissed"
        });
        showGames(window);
        mouseClick(findChild(window, "logOutChild"));
        tryCompare(findChild(window, "notNowLogOut"), "activeFocus", true);
        waitForPolish(window);
        mouseClick(findChild(window, "confirmLogOut"));
        tryCompare(window.gamesPage.loggingOut, "step", ChildLogOut.NotLoggedOut);
        compare(findChild(window, "loggedInSentence").text, "Ada was not logged out. It needs your password: the box was closed, or the password was not accepted.");
    }

    // A child who logs in while their changes wait keeps the changes, but
    // Save waits, and the page says why.
    function test_saveWaitsForALoggedInChildsChanges() {
        const window = makeWindow("done");
        showGames(window);
        mouseClick(card(window, 1));
        verify(findChild(window, "saveButton").enabled);
        standIns.setLoggedIn(["ada"]);
        window.gamesPage.reload();
        verify(!findChild(window, "saveButton").enabled);
        verify(!findChild(window, "putBack").enabled);
        compare(findChild(window, "loggedInUnsavedSentence").text, "Save waits: changes for a child who is logged in cannot be saved yet. Logged in: Ada. Log them out first.");
        compare(card(window, 1).onList, true);
    }

    // A list that cannot be read says that Saving writes it again.
    function test_anUnreadableListSaysSavingWritesItAgain() {
        const window = makeWindow("done");
        showGames(window);
        verify(findChild(window, "listSentence").text.indexOf("later version") < 0);
    }
}
