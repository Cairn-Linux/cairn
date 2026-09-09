// SPDX-License-Identifier: Apache-2.0
#include "AppLauncher.h"

#include <QSignalSpy>
#include <QTest>

namespace {

// Every case uses /bin/sh so the test needs nothing beyond a POSIX shell.
QString shell() {
    return QStringLiteral("/bin/sh");
}

class AppLauncherTest : public QObject {
    Q_OBJECT

private slots:
    void startsIdle() {
        const AppLauncher launcher(this);
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
        QVERIFY(launcher.title().isEmpty());
    }

    void emptyExecFailsAtOnce() {
        AppLauncher launcher(this);
        const QSignalSpy states(&launcher, &AppLauncher::stateChanged);
        launcher.launch(QStringLiteral("Draw"), {});
        QCOMPARE(launcher.state(), AppLauncher::State::Failed);
        QCOMPARE(launcher.title(), QStringLiteral("Draw"));
        QCOMPARE(states.count(), 1);
    }

    void missingProgramFails() {
        AppLauncher launcher(this);
        launcher.launch(QStringLiteral("Draw"), {QStringLiteral("/nonexistent/cairn-app")});
        QTRY_COMPARE(launcher.state(), AppLauncher::State::Failed);
    }

    void earlyBadExitFails() {
        AppLauncher launcher(this);
        launcher.setSettleMilliseconds(2000);
        launcher.launch(QStringLiteral("Music"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("exit 3")});
        QTRY_COMPARE(launcher.state(), AppLauncher::State::Failed);
        QCOMPARE(launcher.title(), QStringLiteral("Music"));
    }

    void launchThatOpensNoWindowReturnsToTilesAfterTheGrace() {
        AppLauncher launcher(this);
        launcher.setLaunchGraceMilliseconds(150);
        launcher.launch(QStringLiteral("Story"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("exit 0")});
        // No window ever opens, so the grace timer, not the exit, ends the
        // wait, and quietly: nothing failed.
        QTRY_COMPARE_WITH_TIMEOUT(launcher.state(), AppLauncher::State::Idle, 5000);
        QVERIFY(!launcher.needsGrownUp());
    }

    void lateBadExitIsNotAFailedLaunch() {
        AppLauncher launcher(this);
        launcher.setSettleMilliseconds(100);
        launcher.setLaunchGraceMilliseconds(400);
        launcher.launch(QStringLiteral("Build"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("sleep 0.2; exit 3")});
        // Past the settle window, an error exit is not a failed launch; with
        // no window it just returns to the tiles when the grace ends.
        QTRY_COMPARE_WITH_TIMEOUT(launcher.state(), AppLauncher::State::Idle, 5000);
        QVERIFY(!launcher.needsGrownUp());
    }

    void secondLaunchWhileRunningIsIgnored() {
        AppLauncher launcher(this);
        launcher.launch(QStringLiteral("Practice"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("sleep 0.2")});
        launcher.windowOpened(QStringLiteral("w1"), QStringLiteral("gcompris"),
                              QStringLiteral("GCompris"));
        QCOMPARE(launcher.state(), AppLauncher::State::Running);
        launcher.launch(QStringLiteral("Terminal"), {});
        QCOMPARE(launcher.state(), AppLauncher::State::Running);
        QCOMPARE(launcher.title(), QStringLiteral("Practice"));
        launcher.windowClosed(QStringLiteral("w1"));
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
        // Let the launching process exit before the launcher goes out of scope.
        QTest::qWait(400);
    }

    void dismissLeavesFailed() {
        AppLauncher launcher(this);
        launcher.launch(QStringLiteral("Draw"), {});
        QCOMPARE(launcher.state(), AppLauncher::State::Failed);
        launcher.dismiss();
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
        launcher.dismiss();
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
    }

    // A window nobody launched, while the tiles are up, is a grown-up's job.
    void windowOnItsOwnInterrupts() {
        AppLauncher launcher(this);
        const QSignalSpy states(&launcher, &AppLauncher::stateChanged);
        launcher.windowOpened(QStringLiteral("w1"), QStringLiteral("steam"),
                              QStringLiteral("Steam"));
        QCOMPARE(launcher.state(), AppLauncher::State::Interrupted);
        QVERIFY(launcher.needsGrownUp());
        QCOMPARE(launcher.title(), QStringLiteral("Steam"));
        QCOMPARE(states.count(), 1);
    }

    void windowIsNamedByTitleThenAppIdThenAPlainPhrase_data() {
        QTest::addColumn<QString>("appId");
        QTest::addColumn<QString>("title");
        QTest::addColumn<QString>("expected");
        QTest::newRow("title") << "steam" << "Steam - Update" << "Steam - Update";
        QTest::newRow("app id") << "steam" << "" << "steam";
        QTest::newRow("nothing") << "" << "" << "Another program";
    }

    void windowIsNamedByTitleThenAppIdThenAPlainPhrase() {
        QFETCH(const QString, appId);
        QFETCH(const QString, title);
        QFETCH(const QString, expected);
        AppLauncher launcher(this);
        launcher.windowOpened(QStringLiteral("w1"), appId, title);
        QCOMPARE(launcher.title(), expected);
    }

    void ownWindowIsIgnored() {
        AppLauncher launcher(this);
        launcher.setOwnAppId(QStringLiteral("cairn-launcher"));
        launcher.windowOpened(QStringLiteral("w1"), QStringLiteral("cairn-launcher"),
                              QStringLiteral("Cairn"));
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
        launcher.windowClosed(QStringLiteral("w1"));
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
    }

    void closingTheWindowReturnsToIdle() {
        AppLauncher launcher(this);
        launcher.windowOpened(QStringLiteral("w1"), QStringLiteral("steam"),
                              QStringLiteral("Steam"));
        launcher.windowClosed(QStringLiteral("w1"));
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
        QVERIFY(!launcher.needsGrownUp());
    }

    void everyUnexpectedWindowMustClose() {
        AppLauncher launcher(this);
        launcher.windowOpened(QStringLiteral("w1"), QStringLiteral("steam"),
                              QStringLiteral("Steam"));
        launcher.windowOpened(QStringLiteral("w2"), QStringLiteral("steam"),
                              QStringLiteral("Sign in"));
        // The first window keeps the name; the second does not rename the screen.
        QCOMPARE(launcher.title(), QStringLiteral("Steam"));
        launcher.windowClosed(QStringLiteral("w1"));
        QCOMPARE(launcher.state(), AppLauncher::State::Interrupted);
        launcher.windowClosed(QStringLiteral("w2"));
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
    }

    void unknownWindowClosingChangesNothing() {
        AppLauncher launcher(this);
        const QSignalSpy states(&launcher, &AppLauncher::stateChanged);
        launcher.windowClosed(QStringLiteral("never-opened"));
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
        QCOMPARE(states.count(), 0);
    }

    // The window a launched program opens is the app, not an interruption,
    // and the launcher tracks the app by that window: it is Running while the
    // window is up and returns to the tiles when it closes (issue #42).
    void aLaunchedWindowIsTrackedUntilItCloses() {
        AppLauncher launcher(this);
        launcher.launch(QStringLiteral("Draw"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("sleep 0.2")});
        QCOMPARE(launcher.state(), AppLauncher::State::Starting);
        launcher.windowOpened(QStringLiteral("w1"), QStringLiteral("tuxpaint"),
                              QStringLiteral("Tux Paint"));
        QCOMPARE(launcher.state(), AppLauncher::State::Running);
        QVERIFY(!launcher.needsGrownUp());
        // The tile keeps its own name; the window does not rename the frame.
        QCOMPARE(launcher.title(), QStringLiteral("Draw"));
        launcher.windowClosed(QStringLiteral("w1"));
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
        QTest::qWait(300); // reap the process before the launcher is destroyed.
    }

    // steam -applaunch and flatpak run return within a second while the app
    // runs on; a window that opens after that exit is still the app (issue
    // #42), so the launcher must not return to the tiles when the process
    // ends, only when the window does.
    void aWindowAfterTheLaunchProcessExitsIsStillTheApp() {
        AppLauncher launcher(this);
        launcher.launch(QStringLiteral("Putt-Putt"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("exit 0")});
        QTest::qWait(50); // let the launching process exit, as steam does.
        QCOMPARE(launcher.state(), AppLauncher::State::Starting);
        launcher.windowOpened(QStringLiteral("g1"), QStringLiteral("scummvm"),
                              QStringLiteral("Putt-Putt Joins the Parade"));
        QCOMPARE(launcher.state(), AppLauncher::State::Running);
        QVERIFY(!launcher.needsGrownUp());
        launcher.windowClosed(QStringLiteral("g1"));
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
    }

    void dismissDoesNotEndAnInterruption() {
        AppLauncher launcher(this);
        launcher.windowOpened(QStringLiteral("w1"), QStringLiteral("steam"),
                              QStringLiteral("Steam"));
        launcher.dismiss();
        QCOMPARE(launcher.state(), AppLauncher::State::Interrupted);
    }

    void launchWhileInterruptedIsIgnored() {
        AppLauncher launcher(this);
        launcher.windowOpened(QStringLiteral("w1"), QStringLiteral("steam"),
                              QStringLiteral("Steam"));
        const QSignalSpy states(&launcher, &AppLauncher::stateChanged);
        launcher.launch(QStringLiteral("Draw"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("exit 0")});
        QCOMPARE(states.count(), 0);
        QCOMPARE(launcher.title(), QStringLiteral("Steam"));
    }

    void windowAfterAFailedLaunchTakesOver() {
        AppLauncher launcher(this);
        launcher.launch(QStringLiteral("Draw"), {});
        QCOMPARE(launcher.state(), AppLauncher::State::Failed);
        launcher.windowOpened(QStringLiteral("w1"), QStringLiteral("steam"),
                              QStringLiteral("Steam"));
        QCOMPARE(launcher.state(), AppLauncher::State::Interrupted);
        QCOMPARE(launcher.title(), QStringLiteral("Steam"));
        launcher.windowClosed(QStringLiteral("w1"));
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
    }
};

} // namespace

QTEST_GUILESS_MAIN(AppLauncherTest)
#include "tst_applauncher.moc"
