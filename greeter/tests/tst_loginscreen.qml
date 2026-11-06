// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Window
import QtTest
import Cairn.Brand
import Cairn.Greeter

// The real theme from greeter/theme with a stand-in for SDDM. Accounts: Ada
// (L1), Ben (L3) and Test Guardian (a Guardian, named as on the test VM),
// shown in that order.
TestCase {
    id: testCase

    name: "LoginScreen"
    when: windowShown

    property Item screen
    property ListView tiles
    property Item passwordRow
    property Item message
    // Some time on some day; the wrong tries are counted from here.
    readonly property real now: 1800000000

    // A TestCase is never visible itself, so the screen gets a window, the
    // size of the smallest laptop screen the design targets.
    Window {
        id: host

        width: 1280
        height: 800
        visible: true
    }

    SignalSpy {
        id: logins

        target: sddm
        signalName: "loggedIn"
    }

    function init() {
        const component = Qt.createComponent(Qt.resolvedUrl("../theme/Main.qml"));
        compare(component.status, Component.Ready, component.errorString());
        screen = createTemporaryObject(component, host.contentItem, {
            "family": testFamily,
            "width": host.width,
            "height": host.height
        });
        verify(screen !== null);
        // No one on a visit, whatever the machine running the tests has in
        // /run/cairn.
        screen.failedLogin.visitsFile = Qt.resolvedUrl("fixtures/no-visits");
        // No wrong tries, whatever the machine running the tests has in
        // /var/lib/faillock, and a clock that holds still.
        faillock.reset();
        faillock.setSettings("deny = 3\nunlock_time = 600\n");
        screen.accountLock.settingsFile = faillock.settingsFile;
        screen.accountLock.fixedTime = at(now);
        tiles = findChild(screen, "familyTiles");
        passwordRow = findChild(screen, "passwordRow");
        message = findChild(screen, "message");
        verify(tiles !== null && passwordRow !== null && message !== null);
        logins.clear();
        host.requestActivate();
        tryVerify(() => host.active);
        tiles.forceActiveFocus();
        waitForRendering(screen);
    }

    function at(seconds) {
        return new Date(seconds * 1000);
    }

    function wrongTries(loginName, count, time) {
        for (let i = 0; i < count; ++i)
            faillock.addFailure(loginName, time);
    }

    // Opens the Guardian's password row and sends a password.
    function tryAPassword() {
        mouseClick(tiles.itemAtIndex(2));
        keyClick(Qt.Key_X);
        keyClick(Qt.Key_Return);
    }

    function test_familyInOrder() {
        compare(tiles.count, 3);
        compare(tiles.itemAtIndex(0).displayName, "Ada");
        compare(tiles.itemAtIndex(1).displayName, "Ben");
        compare(tiles.itemAtIndex(2).displayName, "Test Guardian");
        compare(tiles.itemAtIndex(0).Accessible.name, "Ada");
    }

    function test_aYoungChildLogsInWithOnePress() {
        compare(tiles.currentIndex, 0);
        keyClick(Qt.Key_Return);
        compare(logins.count, 1);
        compare(sddm.lastUser, "ada");
        compare(sddm.lastPassword, "");
        verify(!passwordRow.visible);
    }

    function test_aClickWorksToo() {
        mouseClick(tiles.itemAtIndex(0));
        compare(logins.count, 1);
        compare(sddm.lastUser, "ada");
    }

    // On the test VM the grown-up's tile read "Test Gua…". A first and last
    // name wrap onto two lines, whole, and the tile still fits its row.
    function test_aLongNameWrapsToTwoLines() {
        const tile = tiles.itemAtIndex(2);
        const name = findChild(tile, "tileName");
        compare(name.text, "Test Guardian");
        compare(name.lineCount, 2);
        verify(!name.truncated);
        verify(name.contentWidth <= name.width);
        verify(tile.height <= tiles.height);
    }

    // A name breaks only between words. "Christopher" is wider than the
    // tile. Split at any letter, it read "Christophe" over "r", which is
    // hard for an early reader to recognise. Now it stays on one line and
    // ends in an ellipsis inside the tile.
    function test_aLongWordIsNeverSplit() {
        const tile = tiles.itemAtIndex(0);
        tile.displayName = "Christopher";
        const name = findChild(tile, "tileName");
        compare(name.lineCount, 1);
        verify(name.truncated);
        verify(name.contentWidth <= name.width);
    }

    // A name too long for the password box in any font ends in an ellipsis
    // inside the box, rather than being cut off mid-letter at its edge.
    function test_aLongNameStaysInsideThePasswordBox() {
        mouseClick(tiles.itemAtIndex(2));
        passwordRow.displayName = "Alexandra Montgomery-Smith";
        const field = findChild(passwordRow, "passwordField");
        const placeholder = findChild(passwordRow, "passwordPlaceholder");
        verify(placeholder.visible);
        verify(placeholder.truncated);
        verify(placeholder.contentWidth <= field.width);
    }

    // The Guardian's tile never logs in by itself: it opens the password row.
    function test_aGuardianIsAskedForAPassword() {
        keyClick(Qt.Key_Right);
        keyClick(Qt.Key_Right);
        compare(tiles.currentIndex, 2);
        keyClick(Qt.Key_Return);
        compare(logins.count, 0);
        verify(passwordRow.visible);
        const field = findChild(passwordRow, "passwordField");
        verify(field.activeFocus);
        keyClick(Qt.Key_S);
        keyClick(Qt.Key_E);
        keyClick(Qt.Key_Return);
        compare(logins.count, 1);
        compare(sddm.lastUser, "guardian");
        compare(sddm.lastPassword, "se");
    }

    // Enter on an empty field sends nothing, so it never counts towards the
    // lockout.
    function test_anEmptyPasswordIsNotATry() {
        mouseClick(tiles.itemAtIndex(2));
        keyClick(Qt.Key_Return);
        mouseClick(findChild(passwordRow, "logInButton"));
        compare(logins.count, 0);
        verify(passwordRow.visible);
    }

    function test_anOlderChildIsAskedForAPassword() {
        keyClick(Qt.Key_Right);
        keyClick(Qt.Key_Return);
        compare(logins.count, 0);
        verify(passwordRow.visible);
    }

    function test_escapeGoesBackToTheTiles() {
        keyClick(Qt.Key_Left);
        compare(tiles.currentIndex, 2);
        keyClick(Qt.Key_Return);
        verify(passwordRow.visible);
        keyClick(Qt.Key_Escape);
        verify(!passwordRow.visible);
        verify(tiles.activeFocus);
        compare(logins.count, 0);
    }

    function test_aWrongPasswordSaysSoAndClears() {
        mouseClick(tiles.itemAtIndex(2));
        keyClick(Qt.Key_X);
        keyClick(Qt.Key_Return);
        compare(logins.count, 1);
        sddm.fail();
        verify(passwordRow.visible);
        verify(message.visible);
        verify(message.text.indexOf("did not work") >= 0);
        compare(findChild(passwordRow, "passwordField").text, "");
        verify(findChild(passwordRow, "passwordField").activeFocus);
    }

    // A child's login that fails is not the child's fault, and says so the
    // same way the launcher does.
    function test_aChildsFailedLoginNeedsAGrownUp() {
        keyClick(Qt.Key_Return);
        sddm.fail();
        compare(message.text, "Something needs a grown-up.");
        verify(!passwordRow.visible);
    }

    // A grown-up has the child's Steam open from their own login, so PAM
    // says no, and the screen says why without blame (ADR-0032).
    function test_aChildOnASteamVisitIsToldToTryAgainSoon() {
        screen.failedLogin.visitsFile = Qt.resolvedUrl("fixtures/steam-visits-ada");
        keyClick(Qt.Key_Return);
        sddm.fail();
        compare(message.text, "A grown-up is setting something up for you. Try again soon.");
        verify(!passwordRow.visible);
        verify(tiles.activeFocus);
    }

    // An older child's password was right; the visit is still why.
    function test_anOlderChildOnAVisitIsNotToldThePasswordWasWrong() {
        screen.failedLogin.visitsFile = Qt.resolvedUrl("fixtures/steam-visits-ben");
        keyClick(Qt.Key_Right);
        keyClick(Qt.Key_Return);
        keyClick(Qt.Key_X);
        keyClick(Qt.Key_Return);
        compare(logins.count, 1);
        sddm.fail();
        compare(message.text, "A grown-up is setting something up for you. Try again soon.");
        verify(!passwordRow.visible);
        verify(tiles.activeFocus);
    }

    // Another child's visit changes nothing for this one.
    function test_anotherChildsVisitIsNotThisChildsReason() {
        screen.failedLogin.visitsFile = Qt.resolvedUrl("fixtures/steam-visits-ben");
        keyClick(Qt.Key_Return);
        sddm.fail();
        compare(message.text, "Something needs a grown-up.");
    }

    // While SDDM is answering, a second press does nothing.
    function test_oneLoginAtATime() {
        keyClick(Qt.Key_Return);
        mouseClick(tiles.itemAtIndex(0));
        keyClick(Qt.Key_Return);
        compare(logins.count, 1);
    }

    // The login screen is Ink, so every focus ring on it is the one for Ink.
    // The ring for Sand and Paper is Fjord, 1.97:1 on Ink (#216).
    function test_theRingsAreTheOnesForInk() {
        const ring = findChild(tiles.itemAtIndex(0), "focusRing");
        verify(ring.visible);
        compare(ring.border.color, Tokens.focusOnInk);
        mouseClick(tiles.itemAtIndex(2));
        const box = findChild(passwordRow, "passwordBox");
        verify(findChild(passwordRow, "passwordField").activeFocus);
        compare(box.border.color, Tokens.focusOnInk);
        const back = findChild(passwordRow, "backButton");
        back.forceActiveFocus();
        verify(back.border.width > 0);
        compare(back.border.color, Tokens.focusOnInk);
    }

    // With two or more tries left, no count: a number on every try is noise.
    function test_aWrongPasswordWithTriesLeftSaysTryAgain() {
        tryAPassword();
        wrongTries("guardian", 1, now);
        sddm.fail();
        compare(message.text, "That password did not work. Try again.");
    }

    // The issue's sentence, with the real numbers, drawn as written.
    function test_beforeTheLastTryItWarns() {
        wrongTries("guardian", 1, now - 30);
        tryAPassword();
        wrongTries("guardian", 1, now);
        sddm.fail();
        compare(message.text, "That password did not work. One more wrong try locks this account for 10 minutes.");
        compare(findChild(passwordRow, "passwordField").text, "");
        verify(findChild(passwordRow, "passwordField").activeFocus);
    }

    // The third wrong try says the lock at once, and SDDM's second
    // loginFailed gives the same words.
    function test_theTryThatLocksSaysSo() {
        wrongTries("guardian", 2, now - 30);
        tryAPassword();
        wrongTries("guardian", 1, now);
        sddm.fail();
        const locked = "This account is locked after too many wrong passwords. Try again in 10 minutes.";
        compare(message.text, locked);
        sddm.fail();
        compare(message.text, locked);
        verify(passwordRow.visible);
    }

    // The right password while locked is refused by PAM; the screen never
    // calls it wrong, and the row stays open, since PAM is the gate.
    function test_aTryWhileLockedIsNotCalledWrong() {
        wrongTries("guardian", 3, now - 120);
        tryAPassword();
        sddm.fail();
        compare(message.text, "This account is locked after too many wrong passwords. Try again in 8 minutes.");
        verify(message.text.indexOf("did not work") < 0);
        verify(passwordRow.visible);
        verify(findChild(passwordRow, "passwordField").activeFocus);
    }

    // A lock from sudo or the lock screen shows as soon as the tile is
    // chosen, before anyone types.
    function test_aLockedTileSaysSoBeforeTyping() {
        wrongTries("guardian", 3, now - 60);
        mouseClick(tiles.itemAtIndex(2));
        compare(logins.count, 0);
        compare(message.text, "This account is locked after too many wrong passwords. Try again in 9 minutes.");
        compare(message.Accessible.name, message.text);
        verify(passwordRow.visible);
        verify(findChild(passwordRow, "passwordField").activeFocus);
    }

    function test_itCountsDownByTheMinute() {
        wrongTries("guardian", 3, now);
        mouseClick(tiles.itemAtIndex(2));
        screen.accountLock.fixedTime = at(now + 59);
        compare(message.text, "This account is locked after too many wrong passwords. Try again in 10 minutes.");
        screen.accountLock.fixedTime = at(now + 539);
        compare(message.text, "This account is locked after too many wrong passwords. Try again in 2 minutes.");
        screen.accountLock.fixedTime = at(now + 540);
        compare(message.text, "This account is locked after too many wrong passwords. Try again in less than a minute.");
        // faillock still refuses at exactly ten minutes.
        screen.accountLock.fixedTime = at(now + 600);
        compare(message.text, "This account is locked after too many wrong passwords. Try again in less than a minute.");
        // Focus elsewhere, so the end of the lock has to bring it back.
        findChild(passwordRow, "backButton").forceActiveFocus();
        screen.accountLock.fixedTime = at(now + 601);
        compare(message.text, "You can try again now.");
        compare(message.Accessible.name, "You can try again now.");
        verify(findChild(passwordRow, "passwordField").activeFocus);
        // A wrong password now gets the short words. faillock forgets the
        // old tries once the lock has run out.
        keyClick(Qt.Key_X);
        keyClick(Qt.Key_Return);
        faillock.reset();
        wrongTries("guardian", 1, now + 601);
        sddm.fail();
        compare(message.text, "That password did not work. Try again.");
    }

    // A try sent in the last seconds of a lock is refused for the lock,
    // and PAM answers after the lock has run out. The password was never
    // checked, so it is not called wrong.
    function test_aTryAsTheLockEndsIsNotCalledWrong() {
        wrongTries("guardian", 3, now);
        screen.accountLock.fixedTime = at(now + 599);
        tryAPassword();
        screen.accountLock.fixedTime = at(now + 601);
        sddm.fail();
        compare(message.text, "You can try again now.");
        verify(passwordRow.visible);
    }

    function test_aLockUntilResetSaysAGrownUpCanUnlockIt() {
        faillock.setSettings("deny = 3\nunlock_time = never\n");
        wrongTries("guardian", 3, now - 86400);
        mouseClick(tiles.itemAtIndex(2));
        compare(message.text, "This account is locked after too many wrong passwords. A grown-up can unlock it.");
    }

    // The record's newest try is later than the clock.
    function test_aClockSetBackSaysTryAgainLater() {
        wrongTries("guardian", 3, now);
        screen.accountLock.fixedTime = at(now - 3600);
        mouseClick(tiles.itemAtIndex(2));
        compare(message.text, "This account is locked after too many wrong passwords. Try again later.");
    }

    function test_beforeTheLastTryOfALockUntilResetItWarns() {
        faillock.setSettings("deny = 3\nunlock_time = never\n");
        wrongTries("guardian", 1, now - 30);
        tryAPassword();
        wrongTries("guardian", 1, now);
        sddm.fail();
        compare(message.text, "That password did not work. One more wrong try locks this account.");
    }

    // A lock sentence that can no longer be worked out does not stay up.
    function test_aLockThatBecomesUnclearTakesItsWordsAway() {
        wrongTries("guardian", 3, now);
        mouseClick(tiles.itemAtIndex(2));
        verify(message.text.indexOf("locked") >= 0);
        faillock.setSettings("admin_group = wheel\n");
        screen.accountLock.fixedTime = at(now + 1);
        compare(screen.accountLock.state, AccountLock.Unknown);
        compare(message.text, "");
    }

    // A grown-up's faillock --reset shows without anyone touching the screen.
    function test_aResetShowsByItself() {
        wrongTries("guardian", 3, now);
        mouseClick(tiles.itemAtIndex(2));
        verify(message.text.indexOf("locked") >= 0);
        faillock.reset();
        tryCompare(message, "text", "You can try again now.");
    }

    function test_backLeavesTheLockWords() {
        wrongTries("guardian", 3, now);
        mouseClick(tiles.itemAtIndex(2));
        keyClick(Qt.Key_Escape);
        compare(message.text, "");
        compare(screen.accountLock.loginName, "");
        verify(tiles.activeFocus);
    }

    // When the lock cannot be worked out, the old words stay, and nothing
    // claims a lock.
    function test_aLockThatCannotBeWorkedOutKeepsThePlainWords() {
        faillock.setSettings("admin_group = wheel\n");
        wrongTries("guardian", 3, now);
        mouseClick(tiles.itemAtIndex(2));
        compare(message.text, "");
        keyClick(Qt.Key_X);
        keyClick(Qt.Key_Return);
        sddm.fail();
        compare(message.text, "That password did not work. After a few wrong tries, the account waits a few minutes before it lets anyone try again.");
    }

    // L1 and L2 children are let in before faillock, so a stray record
    // changes nothing for them.
    function test_aYoungChildWithAStrayRecordStillLogsInAtOnce() {
        wrongTries("ada", 3, now);
        keyClick(Qt.Key_Return);
        compare(logins.count, 1);
        compare(sddm.lastUser, "ada");
        compare(message.text, "");
    }
}
