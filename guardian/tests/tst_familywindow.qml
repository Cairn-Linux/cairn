// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtTest
import Cairn.Family

TestCase {
    id: testCase

    name: "FamilyWindow"
    when: windowShown

    Component {
        id: windowComponent

        FamilyWindow {}
    }

    function makeWindow(scenario, withChildren) {
        // Tall enough for the whole page, so every control can be clicked.
        const window = createTemporaryObject(windowComponent, testCase, {
            "page": standIns.page(scenario, withChildren === undefined ? true : withChildren),
            "height": 1400
        });
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
        const window = makeWindow("done", false);
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
}
