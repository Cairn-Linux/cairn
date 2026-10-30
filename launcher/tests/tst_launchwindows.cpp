// SPDX-License-Identifier: Apache-2.0
#include "LaunchWindows.h"

#include <QTest>

namespace {

// Whose each window is (ADR-0026, #99): the compositor's facts decide, never a
// window's name.
class LaunchWindowsTest : public QObject {
    Q_OBJECT

private slots:
    void aWindowAtTheTilesOpenedOnItsOwn() {
        LaunchWindows windows;
        windows.opened(QStringLiteral("w1"), QStringLiteral("Steam"), false, {});
        QVERIFY(windows.unexpectedOnScreen());
        QCOMPARE(windows.unexpectedName(), QStringLiteral("Steam"));
        QVERIFY(!windows.appOnScreen());
    }

    // The child cannot see it, so it neither interrupts nor holds a launch.
    void aHiddenWindowNeverCounts() {
        LaunchWindows windows;
        windows.opened(QStringLiteral("h1"), QStringLiteral("Steam"), true, {});
        QVERIFY(!windows.unexpectedOnScreen());
        windows.startLaunch();
        windows.opened(QStringLiteral("h2"), QStringLiteral("Steam"), true, {});
        QVERIFY(!windows.appOnScreen());
        QVERIFY(!windows.appWasOnScreen());
    }

    void theFirstWindowOnTheScreenDuringALaunchIsTheApp() {
        LaunchWindows windows;
        windows.startLaunch();
        windows.opened(QStringLiteral("h1"), QStringLiteral("Steam"), true, {});
        windows.opened(QStringLiteral("g1"), QStringLiteral("Putt-Putt"), false, {});
        QVERIFY(windows.appOnScreen());
        QVERIFY(windows.appWasOnScreen());
        QVERIFY(!windows.unexpectedOnScreen());
    }

    void aWindowThatBelongsToTheAppIsTheApp() {
        LaunchWindows windows;
        windows.startLaunch();
        windows.opened(QStringLiteral("g1"), QStringLiteral("Tux Paint"), false, {});
        windows.opened(QStringLiteral("d1"), QStringLiteral("Open"), false, QStringLiteral("g1"));
        QVERIFY(!windows.unexpectedOnScreen());
        windows.closed(QStringLiteral("g1"));
        QVERIFY(windows.appOnScreen());
    }

    // Until the app is open, a game's splash and its own window both are the
    // app's, and the launch outlasts the splash closing (ADR-0028).
    void everyWindowWhileOpeningIsTheApp() {
        LaunchWindows windows;
        QVERIFY(!windows.opening());
        windows.startLaunch();
        QVERIFY(windows.opening());
        windows.opened(QStringLiteral("s1"), QStringLiteral("Splash"), false, {});
        windows.opened(QStringLiteral("g1"), QStringLiteral("Putt-Putt"), false, {});
        QVERIFY(!windows.unexpectedOnScreen());
        windows.closed(QStringLiteral("s1"));
        windows.closed(QStringLiteral("g1"));
        QVERIFY(windows.launching());
        windows.opened(QStringLiteral("g2"), QStringLiteral("Putt-Putt"), false, {});
        QVERIFY(windows.appOnScreen());
        windows.appIsOpen();
        QVERIFY(!windows.opening());
        QVERIFY(windows.launching());
    }

    void theLaunchEndingOrAGiveUpEndsTheOpening() {
        LaunchWindows windows;
        windows.startLaunch();
        windows.endLaunch();
        QVERIFY(!windows.opening());
        windows.startLaunch();
        windows.forgetAll();
        QVERIFY(!windows.opening());
    }

    // Once the app is open, the same name as the app's window does not make a
    // window the app's.
    void anyOtherWindowDuringTheAppOpenedOnItsOwn() {
        LaunchWindows windows;
        windows.startLaunch();
        windows.opened(QStringLiteral("g1"), QStringLiteral("Putt-Putt"), false, {});
        windows.appIsOpen();
        windows.opened(QStringLiteral("g2"), QStringLiteral("Putt-Putt"), false, {});
        QVERIFY(windows.unexpectedOnScreen());
        windows.opened(QStringLiteral("d1"), QStringLiteral("Help"), false, QStringLiteral("g2"));
        QCOMPARE(windows.unexpectedName(), QStringLiteral("Putt-Putt"));
        windows.closed(QStringLiteral("g2"));
        // A window that belongs to an unexpected one is not the app's either.
        QCOMPARE(windows.unexpectedName(), QStringLiteral("Help"));
    }

    // A hidden window is decided when it is first shown, by the rules then.
    void aHiddenWindowShownLaterIsDecidedThen() {
        LaunchWindows windows;
        windows.startLaunch();
        windows.opened(QStringLiteral("h1"), QStringLiteral("Steam"), true, {});
        windows.opened(QStringLiteral("g1"), QStringLiteral("Putt-Putt"), false, {});
        windows.appIsOpen();
        windows.changed(QStringLiteral("h1"), false, {});
        QVERIFY(windows.unexpectedOnScreen());
        QCOMPARE(windows.unexpectedName(), QStringLiteral("Steam"));
        windows.changed(QStringLiteral("h1"), true, {});
        QVERIFY(!windows.unexpectedOnScreen());
    }

    void anAppWindowThatHidesLeavesTheScreen() {
        LaunchWindows windows;
        windows.startLaunch();
        windows.opened(QStringLiteral("g1"), QStringLiteral("Putt-Putt"), false, {});
        windows.changed(QStringLiteral("g1"), true, {});
        QVERIFY(!windows.appOnScreen());
        QVERIFY(windows.appWasOnScreen());
    }

    void whenTheLaunchEndsTheAppsWindowsAreNoLongerTheApp() {
        LaunchWindows windows;
        windows.startLaunch();
        windows.opened(QStringLiteral("g1"), QStringLiteral("Putt-Putt"), false, {});
        windows.opened(QStringLiteral("g2"), QStringLiteral("Putt-Putt"), true,
                       QStringLiteral("g1"));
        windows.endLaunch();
        QVERIFY(!windows.launching());
        QVERIFY(!windows.appOnScreen());
        QVERIFY(windows.unexpectedOnScreen());
        // The hidden one is decided again if it is ever shown; nothing is
        // launching now, so it opened on its own.
        windows.closed(QStringLiteral("g1"));
        QVERIFY(!windows.unexpectedOnScreen());
        windows.changed(QStringLiteral("g2"), false, {});
        QVERIFY(windows.unexpectedOnScreen());
    }

    void giveUpForgetsEveryWindowOpenNow() {
        LaunchWindows windows;
        windows.opened(QStringLiteral("w1"), QStringLiteral("Steam"), false, {});
        windows.startLaunch();
        windows.forgetAll();
        QVERIFY(!windows.launching());
        QVERIFY(!windows.unexpectedOnScreen());
        windows.changed(QStringLiteral("w1"), false, {});
        QVERIFY(!windows.unexpectedOnScreen());
        // A window that opens after it counts as usual.
        windows.opened(QStringLiteral("w2"), QStringLiteral("Steam"), false, {});
        QVERIFY(windows.unexpectedOnScreen());
    }

    // Steam updating itself at login, as on the kids' laptop on 2026-10-27:
    // its updater's windows came up with no tap (#183). They are Steam's, so
    // nothing interrupts, and each is one to hide.
    void steamsUpdaterWindowsAtTheTilesAreSteams() {
        LaunchWindows windows;
        windows.steamUpdating(true);
        windows.opened(QStringLiteral("u1"), QStringLiteral("Progress"), false, {});
        windows.opened(QStringLiteral("u2"), QStringLiteral("Steam setup"), false, {});
        windows.opened(QStringLiteral("u3"), QStringLiteral("Steam"), false, {});
        QVERIFY(!windows.unexpectedOnScreen());
        QVERIFY(windows.unexpectedName().isEmpty());
        QVERIFY(windows.steamUpdaterOnScreen(QStringLiteral("u1")));
        QVERIFY(windows.steamUpdaterOnScreen(QStringLiteral("u3")));
        QCOMPARE(windows.name(QStringLiteral("u2")), QStringLiteral("Steam setup"));
        // Hidden, it is no longer one to hide.
        windows.changed(QStringLiteral("u1"), true, {});
        QVERIFY(!windows.steamUpdaterOnScreen(QStringLiteral("u1")));
        // Steam's it stays, even if it comes back after the update.
        windows.steamUpdating(false);
        windows.changed(QStringLiteral("u1"), false, {});
        QVERIFY(windows.steamUpdaterOnScreen(QStringLiteral("u1")));
        QVERIFY(!windows.unexpectedOnScreen());
        // A window after the update opened on its own, as before.
        windows.opened(QStringLiteral("w1"), QStringLiteral("Steam"), false, {});
        QVERIFY(windows.unexpectedOnScreen());
        QVERIFY(!windows.steamUpdaterOnScreen(QStringLiteral("w1")));
    }

    // A tap while Steam updates itself: the updater's window is not the
    // game's, so it neither opens the app nor counts as it having come.
    void steamsUpdaterWindowDuringALaunchIsNotTheApp() {
        LaunchWindows windows;
        windows.startLaunch();
        windows.steamUpdating(true);
        windows.opened(QStringLiteral("u1"), QStringLiteral("Progress"), false, {});
        QVERIFY(!windows.appOnScreen());
        QVERIFY(!windows.appWasOnScreen());
        QVERIFY(!windows.unexpectedOnScreen());
        windows.steamUpdating(false);
        windows.opened(QStringLiteral("g1"), QStringLiteral("Arthur's Reading Race"), false, {});
        QVERIFY(windows.appOnScreen());
        windows.endLaunch();
        // Still Steam's after the launch, never on its own.
        QVERIFY(windows.steamUpdaterOnScreen(QStringLiteral("u1")));
        QCOMPARE(windows.unexpectedName(), QStringLiteral("Arthur's Reading Race"));
    }

    // A dialog of an open app's while Steam updates itself is the app's,
    // never one to hide: hidden, it would leave the app stuck behind it.
    void anOpenAppsDialogWhileSteamUpdatesItselfIsTheApps() {
        LaunchWindows windows;
        windows.startLaunch();
        windows.opened(QStringLiteral("g1"), QStringLiteral("Tux Paint"), false, {});
        windows.appIsOpen();
        windows.steamUpdating(true);
        windows.opened(QStringLiteral("g2"), QStringLiteral("Save changes?"), false,
                       QStringLiteral("g1"));
        QVERIFY(!windows.steamUpdaterOnScreen(QStringLiteral("g2")));
        QVERIFY(!windows.unexpectedOnScreen());
        windows.closed(QStringLiteral("g1"));
        QVERIFY(windows.appOnScreen());
    }

    // A window hidden while Steam updated itself is decided when it is
    // first shown, by what holds then.
    void aHiddenWindowIsDecidedWhenShown() {
        LaunchWindows windows;
        windows.steamUpdating(true);
        windows.opened(QStringLiteral("h1"), QStringLiteral("Sign in to Steam"), true, {});
        windows.steamUpdating(false);
        windows.changed(QStringLiteral("h1"), false, {});
        QVERIFY(windows.unexpectedOnScreen());
        QVERIFY(!windows.steamUpdaterOnScreen(QStringLiteral("h1")));
    }

    void unknownWindowsChangeNothing() {
        LaunchWindows windows;
        windows.changed(QStringLiteral("never"), false, {});
        windows.closed(QStringLiteral("never"));
        QVERIFY(!windows.unexpectedOnScreen());
        QVERIFY(windows.unexpectedName().isEmpty());
        QVERIFY(!windows.steamUpdaterOnScreen(QStringLiteral("never")));
        QVERIFY(windows.name(QStringLiteral("never")).isEmpty());
    }
};

} // namespace

QTEST_APPLESS_MAIN(LaunchWindowsTest)
#include "tst_launchwindows.moc"
