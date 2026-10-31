// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Window
import QtTest

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
}
