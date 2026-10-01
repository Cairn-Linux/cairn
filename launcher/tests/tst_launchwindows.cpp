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

    // The same name as the app's window does not make a window the app's.
    void anyOtherWindowDuringTheAppOpenedOnItsOwn() {
        LaunchWindows windows;
        windows.startLaunch();
        windows.opened(QStringLiteral("g1"), QStringLiteral("Putt-Putt"), false, {});
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

    void unknownWindowsChangeNothing() {
        LaunchWindows windows;
        windows.changed(QStringLiteral("never"), false, {});
        windows.closed(QStringLiteral("never"));
        QVERIFY(!windows.unexpectedOnScreen());
        QVERIFY(windows.unexpectedName().isEmpty());
    }
};

} // namespace

QTEST_APPLESS_MAIN(LaunchWindowsTest)
#include "tst_launchwindows.moc"
