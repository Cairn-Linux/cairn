// SPDX-License-Identifier: Apache-2.0
#include "AppLauncher.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

namespace {

// Every case uses /bin/sh so the test needs nothing beyond a POSIX shell.
QString shell() {
    return QStringLiteral("/bin/sh");
}

// Cases about whose a window is set the steady wait to zero, so the app is
// open the moment its window is up; the cases about the wait set it short.
class AppLauncherTest : public QObject {
    Q_OBJECT

private slots:
    void startsIdle() {
        const AppLauncher launcher(this);
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
        QVERIFY(launcher.title().isEmpty());
    }

    // A tile with nothing set up yet says so and needs no grown-up.
    void emptyExecIsComingSoonAtOnce() {
        AppLauncher launcher(this);
        const QSignalSpy states(&launcher, &AppLauncher::stateChanged);
        launcher.launch(QStringLiteral("Music"), {});
        QCOMPARE(launcher.state(), AppLauncher::State::ComingSoon);
        QVERIFY(!launcher.needsGrownUp());
        QCOMPARE(launcher.title(), QStringLiteral("Music"));
        QCOMPARE(states.count(), 1);
    }

    void missingProgramFails() {
        AppLauncher launcher(this);
        launcher.launch(QStringLiteral("Draw"), {QStringLiteral("/nonexistent/cairn-app")});
        QTRY_COMPARE(launcher.state(), AppLauncher::State::Failed);
    }

    // In the kiosk every program starts in a scope of its own, which is what
    // cairn-give-up ends (ADR-0025, #98). A stand-in systemd-run on PATH
    // writes down what it was asked and runs the program after the --.
    void aScopedLaunchStartsTheProgramInsideSystemdRun() {
        const QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString log = dir.filePath(QStringLiteral("asked"));
        QFile fake(dir.filePath(QStringLiteral("systemd-run")));
        QVERIFY(fake.open(QIODevice::WriteOnly));
        fake.write(QStringLiteral("#!/bin/sh\n"
                                  "printf '%s\\n' \"$@\" > '%1'\n"
                                  "while [ \"$1\" != -- ]; do shift; done\n"
                                  "shift\n"
                                  "exec \"$@\"\n")
                       .arg(log)
                       .toUtf8());
        fake.close();
        QVERIFY(fake.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        const QByteArray path = qgetenv("PATH");
        qputenv("PATH", dir.path().toUtf8() + ':' + path);

        AppLauncher launcher(this);
        launcher.setProperty("scoped", true);
        launcher.launch(QStringLiteral("Putt-Putt"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("exit 0")});
        // The program was found when it started, so PATH can go back now.
        qputenv("PATH", path);

        QTRY_VERIFY(QFileInfo(log).size() > 0);
        QFile asked(log);
        QVERIFY(asked.open(QIODevice::ReadOnly));
        const QString unit =
            QStringLiteral("--unit=cairn-app-%1-1").arg(QCoreApplication::applicationPid());
        QCOMPARE(QString::fromUtf8(asked.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts),
                 (QStringList{QStringLiteral("--user"), QStringLiteral("--scope"),
                              QStringLiteral("--quiet"), QStringLiteral("--collect"), unit,
                              QStringLiteral("--"), shell(), QStringLiteral("-c"),
                              QStringLiteral("exit 0")}));
        QVERIFY(launcher.state() != AppLauncher::State::Failed);
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
        launcher.setSteadyMilliseconds(0);
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

    void dismissLeavesComingSoonAndFailed() {
        AppLauncher launcher(this);
        launcher.launch(QStringLiteral("Music"), {});
        QCOMPARE(launcher.state(), AppLauncher::State::ComingSoon);
        launcher.dismiss();
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
        launcher.launch(QStringLiteral("Draw"), {QStringLiteral("/nonexistent/cairn-app")});
        QTRY_COMPARE(launcher.state(), AppLauncher::State::Failed);
        launcher.dismiss();
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
        launcher.dismiss();
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
    }

    // A hidden window changing says nothing about the coming-soon screen.
    void comingSoonStaysUntilDismissed() {
        AppLauncher launcher(this);
        launcher.launch(QStringLiteral("Music"), {});
        launcher.windowOpened(QStringLiteral("h1"), QStringLiteral("steam"),
                              QStringLiteral("Steam"), true);
        launcher.windowClosed(QStringLiteral("h1"));
        QCOMPARE(launcher.state(), AppLauncher::State::ComingSoon);
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
        launcher.setSteadyMilliseconds(0);
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
        launcher.setSteadyMilliseconds(0);
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

    void windowAfterComingSoonTakesOver() {
        AppLauncher launcher(this);
        launcher.launch(QStringLiteral("Draw"), {});
        QCOMPARE(launcher.state(), AppLauncher::State::ComingSoon);
        launcher.windowOpened(QStringLiteral("w1"), QStringLiteral("steam"),
                              QStringLiteral("Steam"));
        QCOMPARE(launcher.state(), AppLauncher::State::Interrupted);
        QCOMPARE(launcher.title(), QStringLiteral("Steam"));
        launcher.windowClosed(QStringLiteral("w1"));
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
    }

    // #99, as it happened in the VM: Steam opened its main window while a game
    // ran, labwc hid it, and the launcher took it for part of the game, so once
    // the game was over the tiles did nothing. A hidden window never counts.
    void aHiddenHelperLeftAfterTheGameDoesNotHoldTheTiles() {
        AppLauncher launcher(this);
        launcher.setSteadyMilliseconds(0);
        launcher.launch(QStringLiteral("Putt-Putt"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("exit 0")});
        launcher.windowOpened(QStringLiteral("g1"), QStringLiteral("scummvm"),
                              QStringLiteral("Putt-Putt Joins the Parade"));
        launcher.windowOpened(QStringLiteral("h1"), QStringLiteral("steam"),
                              QStringLiteral("Steam"), true);
        QCOMPARE(launcher.state(), AppLauncher::State::Running);
        launcher.windowClosed(QStringLiteral("g1"));
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
        launcher.launch(QStringLiteral("Draw"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("exit 0")});
        QCOMPARE(launcher.state(), AppLauncher::State::Starting);
    }

    // The other half of #99: the same hidden window at the tiles put up a
    // grown-up screen that nothing could clear.
    void aHiddenWindowAtTheTilesDoesNotInterrupt() {
        AppLauncher launcher(this);
        const QSignalSpy states(&launcher, &AppLauncher::stateChanged);
        launcher.windowOpened(QStringLiteral("h1"), QStringLiteral("steam"),
                              QStringLiteral("Steam"), true);
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
        QCOMPARE(states.count(), 0);
        // Shown, it is on the screen, and it opened on its own.
        launcher.windowChanged(QStringLiteral("h1"), false);
        QCOMPARE(launcher.state(), AppLauncher::State::Interrupted);
        QCOMPARE(launcher.title(), QStringLiteral("Steam"));
        launcher.windowChanged(QStringLiteral("h1"), true);
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
    }

    // A window's name is whatever its program chose, so a second window with
    // the game's own app id and title is still not the game's.
    void aSecondWindowIsAGrownUpsJobWhateverItIsCalled() {
        AppLauncher launcher(this);
        launcher.setSteadyMilliseconds(0);
        launcher.launch(QStringLiteral("Putt-Putt"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("sleep 0.2")});
        launcher.windowOpened(QStringLiteral("g1"), QStringLiteral("scummvm"),
                              QStringLiteral("Putt-Putt Joins the Parade"));
        launcher.windowOpened(QStringLiteral("g2"), QStringLiteral("scummvm"),
                              QStringLiteral("Putt-Putt Joins the Parade"));
        QCOMPARE(launcher.state(), AppLauncher::State::Interrupted);
        launcher.windowClosed(QStringLiteral("g2"));
        QCOMPARE(launcher.state(), AppLauncher::State::Running);
        QCOMPARE(launcher.title(), QStringLiteral("Putt-Putt"));
        launcher.windowClosed(QStringLiteral("g1"));
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
        QTest::qWait(300); // reap the process before the launcher is destroyed.
    }

    // The compositor says which window a dialog belongs to; that one is the app.
    void aDialogOfTheAppIsTheApp() {
        AppLauncher launcher(this);
        launcher.setSteadyMilliseconds(0);
        launcher.launch(QStringLiteral("Draw"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("exit 0")});
        launcher.windowOpened(QStringLiteral("g1"), QStringLiteral("tuxpaint"),
                              QStringLiteral("Tux Paint"));
        launcher.windowOpened(QStringLiteral("d1"), QStringLiteral("tuxpaint"),
                              QStringLiteral("Open"), false, QStringLiteral("g1"));
        QCOMPARE(launcher.state(), AppLauncher::State::Running);
        launcher.windowClosed(QStringLiteral("g1"));
        QCOMPARE(launcher.state(), AppLauncher::State::Running);
        launcher.windowClosed(QStringLiteral("d1"));
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
    }

    // When the program the launcher started has run and then exits, the app
    // is over; a window of it still up after a moment is a grown-up's job.
    void aWindowLeftAfterTheProgramEndsIsAGrownUpsJob() {
        AppLauncher launcher(this);
        launcher.setSteadyMilliseconds(0);
        launcher.setSettleMilliseconds(50);
        launcher.launch(QStringLiteral("Story"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("sleep 0.3")});
        launcher.windowOpened(QStringLiteral("g1"), QStringLiteral("story"),
                              QStringLiteral("Story"));
        QCOMPARE(launcher.state(), AppLauncher::State::Running);
        QTRY_COMPARE_WITH_TIMEOUT(launcher.state(), AppLauncher::State::Interrupted, 5000);
    }

    // The usual end: the window goes with the program, and the moment between
    // the two never flashes the grown-up screen.
    void aWindowThatGoesWithItsProgramNeedsNoGrownUp() {
        AppLauncher launcher(this);
        launcher.setSteadyMilliseconds(0);
        launcher.setSettleMilliseconds(50);
        launcher.launch(QStringLiteral("Story"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("sleep 0.3")});
        launcher.windowOpened(QStringLiteral("g1"), QStringLiteral("story"),
                              QStringLiteral("Story"));
        QTest::qWait(500); // the program has exited; its window is a moment behind.
        launcher.windowClosed(QStringLiteral("g1"));
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
        QTest::qWait(2500);
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
        QVERIFY(!launcher.needsGrownUp());
    }

    // A program from an earlier launch that hid its window and runs on must
    // not stop the next one starting.
    void aRelaunchWhileAnOldProgramRunsStartsANewOne() {
        const QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString started = dir.filePath(QStringLiteral("started"));
        AppLauncher launcher(this);
        launcher.setSteadyMilliseconds(0);
        launcher.launch(QStringLiteral("Putt-Putt"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("sleep 30")});
        launcher.windowOpened(QStringLiteral("g1"), QStringLiteral("scummvm"),
                              QStringLiteral("Putt-Putt Joins the Parade"));
        launcher.windowChanged(QStringLiteral("g1"), true);
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
        launcher.launch(QStringLiteral("Draw"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("touch '%1'").arg(started)});
        QCOMPARE(launcher.state(), AppLauncher::State::Starting);
        QTRY_VERIFY(QFile::exists(started));
    }

    // A launch stays Starting until the app's window has stayed on the screen
    // for the steady wait, so the starting screen does not go with a window
    // that is only on its way (ADR-0028).
    void theAppIsOpenOnceItsWindowStaysUp() {
        AppLauncher launcher(this);
        launcher.setSteadyMilliseconds(300);
        launcher.launch(QStringLiteral("Putt-Putt"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("exit 0")});
        launcher.windowOpened(QStringLiteral("g1"), QStringLiteral("scummvm"),
                              QStringLiteral("Putt-Putt Joins the Parade"));
        QCOMPARE(launcher.state(), AppLauncher::State::Starting);
        QTRY_COMPARE(launcher.state(), AppLauncher::State::Running);
        launcher.windowClosed(QStringLiteral("g1"));
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
    }

    // As on the first laptop: a game shows a window for a moment, closes it,
    // and opens its own. That is one start, not a game that ended and a
    // window that opened on its own.
    void aSplashBeforeTheGameIsPartOfTheStart() {
        AppLauncher launcher(this);
        launcher.setSteadyMilliseconds(300);
        launcher.setLaunchGraceMilliseconds(5000);
        const QSignalSpy states(&launcher, &AppLauncher::stateChanged);
        launcher.launch(QStringLiteral("Pajama Sam"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("exit 0")});
        QTest::qWait(300); // `steam -applaunch` hands off before any window.
        launcher.windowOpened(QStringLiteral("s1"), QStringLiteral("steam_app_1"), {});
        QTest::qWait(100);
        launcher.windowClosed(QStringLiteral("s1"));
        QCOMPARE(launcher.state(), AppLauncher::State::Starting);
        launcher.windowOpened(QStringLiteral("g1"), QStringLiteral("steam_app_1"),
                              QStringLiteral("Pajama Sam"));
        QCOMPARE(launcher.state(), AppLauncher::State::Starting);
        QTRY_COMPARE(launcher.state(), AppLauncher::State::Running);
        QCOMPARE(launcher.title(), QStringLiteral("Pajama Sam"));
        // Starting, then Running: nothing in between reached the screen.
        QCOMPARE(states.count(), 2);
    }

    // While the app is opening, a second window beside the first is the
    // app's too; a game may open its own before its splash has gone.
    void everyWindowWhileOpeningIsTheApp() {
        AppLauncher launcher(this);
        launcher.setSteadyMilliseconds(300);
        launcher.launch(QStringLiteral("Spy Fox"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("exit 0")});
        launcher.windowOpened(QStringLiteral("s1"), QStringLiteral("steam_app_2"), {});
        launcher.windowOpened(QStringLiteral("g1"), QStringLiteral("steam_app_2"),
                              QStringLiteral("Spy Fox"));
        QVERIFY(!launcher.needsGrownUp());
        launcher.windowClosed(QStringLiteral("s1"));
        QTRY_COMPARE(launcher.state(), AppLauncher::State::Running);
        launcher.windowClosed(QStringLiteral("g1"));
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
    }

    // A window that comes and goes with nothing after it is a launch that did
    // not open: the tiles come back when the grace ends, with no grown-up.
    void aWindowThatComesAndGoesAloneReturnsToTheTiles() {
        AppLauncher launcher(this);
        launcher.setSteadyMilliseconds(300);
        launcher.setLaunchGraceMilliseconds(500);
        launcher.launch(QStringLiteral("Pep"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("exit 0")});
        launcher.windowOpened(QStringLiteral("g1"), QStringLiteral("steam_app_3"), {});
        launcher.windowClosed(QStringLiteral("g1"));
        QCOMPARE(launcher.state(), AppLauncher::State::Starting);
        QTRY_COMPARE(launcher.state(), AppLauncher::State::Idle);
        QVERIFY(!launcher.needsGrownUp());
    }

    // A window that is up when the grace ends gets the rest of its steady
    // wait; if it then stays, the app is open.
    void aWindowUpAtTheEndOfTheGraceGetsItsWait() {
        AppLauncher launcher(this);
        launcher.setSteadyMilliseconds(400);
        launcher.setLaunchGraceMilliseconds(200);
        launcher.launch(QStringLiteral("Freddi Fish"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("exit 0")});
        QTest::qWait(100);
        launcher.windowOpened(QStringLiteral("g1"), QStringLiteral("scummvm"),
                              QStringLiteral("Freddi Fish"));
        QTest::qWait(200);
        QCOMPARE(launcher.state(), AppLauncher::State::Starting);
        QTRY_COMPARE(launcher.state(), AppLauncher::State::Running);
    }

    // Past the grace, a window that goes before its wait is over ends the
    // launch: nothing is left to wait for.
    void aWindowThatGoesAfterTheGraceEndsTheLaunch() {
        AppLauncher launcher(this);
        launcher.setSteadyMilliseconds(5000);
        launcher.setLaunchGraceMilliseconds(100);
        launcher.launch(QStringLiteral("Freddi Fish"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("exit 0")});
        launcher.windowOpened(QStringLiteral("g1"), QStringLiteral("scummvm"),
                              QStringLiteral("Freddi Fish"));
        QTest::qWait(200);
        QCOMPARE(launcher.state(), AppLauncher::State::Starting);
        launcher.windowClosed(QStringLiteral("g1"));
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
        QVERIFY(!launcher.needsGrownUp());
    }

    // A window that comes up while the app is opening gets the whole steady
    // wait, even if an earlier window has been up nearly as long.
    void eachNewWindowGetsTheWholeWait() {
        AppLauncher launcher(this);
        launcher.setSteadyMilliseconds(400);
        launcher.setLaunchGraceMilliseconds(5000);
        launcher.launch(QStringLiteral("Spy Fox"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("exit 0")});
        launcher.windowOpened(QStringLiteral("s1"), QStringLiteral("steam_app_2"), {});
        QTest::qWait(300);
        launcher.windowOpened(QStringLiteral("g1"), QStringLiteral("steam_app_2"),
                              QStringLiteral("Spy Fox"));
        launcher.windowClosed(QStringLiteral("s1"));
        QTest::qWait(200);
        QCOMPARE(launcher.state(), AppLauncher::State::Starting);
        QTRY_COMPARE(launcher.state(), AppLauncher::State::Running);
    }

    // The same for a hidden window that is shown.
    void aWindowShownWhileOpeningGetsTheWholeWait() {
        AppLauncher launcher(this);
        launcher.setSteadyMilliseconds(400);
        launcher.setLaunchGraceMilliseconds(5000);
        launcher.launch(QStringLiteral("Spy Fox"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("exit 0")});
        launcher.windowOpened(QStringLiteral("s1"), QStringLiteral("steam_app_2"), {});
        launcher.windowOpened(QStringLiteral("g1"), QStringLiteral("steam_app_2"),
                              QStringLiteral("Spy Fox"), true);
        QTest::qWait(300);
        launcher.windowChanged(QStringLiteral("g1"), false);
        launcher.windowClosed(QStringLiteral("s1"));
        QTest::qWait(200);
        QCOMPARE(launcher.state(), AppLauncher::State::Starting);
        QTRY_COMPARE(launcher.state(), AppLauncher::State::Running);
    }

    // A program that quits after its own window came up is the app ending, as
    // when a child closes it at once: when the window goes, the tiles come
    // back without waiting out the grace.
    void anAppThatQuitsWhileOpeningEndsWithItsWindow() {
        AppLauncher launcher(this);
        launcher.setSteadyMilliseconds(5000);
        launcher.setLaunchGraceMilliseconds(15000);
        launcher.launch(QStringLiteral("Draw"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("sleep 0.2; exit 3")});
        launcher.windowOpened(QStringLiteral("w1"), QStringLiteral("tuxpaint"),
                              QStringLiteral("Tux Paint"));
        QTest::qWait(600); // the program has quit; its window is a moment behind.
        QCOMPARE(launcher.state(), AppLauncher::State::Starting);
        launcher.windowClosed(QStringLiteral("w1"));
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
        QVERIFY(!launcher.needsGrownUp());
    }

    // The grown-up's key always brings the tiles back, whatever is still open.
    void giveUpAlwaysBringsTheTilesBack() {
        AppLauncher launcher(this);
        launcher.setSteadyMilliseconds(0);
        launcher.windowOpened(QStringLiteral("w1"), QStringLiteral("steam"),
                              QStringLiteral("Steam"));
        QCOMPARE(launcher.state(), AppLauncher::State::Interrupted);
        launcher.giveUp();
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
        QVERIFY(!launcher.needsGrownUp());
        launcher.windowClosed(QStringLiteral("w1"));
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);

        launcher.launch(QStringLiteral("Practice"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("exit 0")});
        launcher.windowOpened(QStringLiteral("g1"), QStringLiteral("gcompris"),
                              QStringLiteral("GCompris"));
        QCOMPARE(launcher.state(), AppLauncher::State::Running);
        launcher.giveUp();
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
        launcher.launch(QStringLiteral("Draw"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("exit 0")});
        QCOMPARE(launcher.state(), AppLauncher::State::Starting);
    }
};

} // namespace

QTEST_GUILESS_MAIN(AppLauncherTest)
#include "tst_applauncher.moc"
