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
            wait(100);
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
        compare(outcome(window), "Cy can log in now, at level 2. This window does not copy Steam games to Cy yet; the family set-up script does.");
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
}
