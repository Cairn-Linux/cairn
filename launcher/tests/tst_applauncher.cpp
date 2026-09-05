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

    void cleanExitReturnsToIdle() {
        AppLauncher launcher(this);
        const QSignalSpy states(&launcher, &AppLauncher::stateChanged);
        launcher.launch(QStringLiteral("Story"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("exit 0")});
        QTRY_COMPARE(launcher.state(), AppLauncher::State::Idle);
        // Starting, Running, Idle: the app came up and closed normally.
        QCOMPARE(states.count(), 3);
    }

    void lateBadExitIsNotAFailedLaunch() {
        AppLauncher launcher(this);
        launcher.setSettleMilliseconds(100);
        launcher.launch(QStringLiteral("Build"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("sleep 0.5; exit 3")});
        QTRY_COMPARE(launcher.state(), AppLauncher::State::Running);
        QTRY_COMPARE_WITH_TIMEOUT(launcher.state(), AppLauncher::State::Idle, 5000);
    }

    void secondLaunchWhileRunningIsIgnored() {
        AppLauncher launcher(this);
        launcher.launch(QStringLiteral("Practice"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("sleep 1")});
        QTRY_COMPARE(launcher.state(), AppLauncher::State::Running);
        launcher.launch(QStringLiteral("Terminal"), {});
        QCOMPARE(launcher.state(), AppLauncher::State::Running);
        QCOMPARE(launcher.title(), QStringLiteral("Practice"));
        QTRY_COMPARE_WITH_TIMEOUT(launcher.state(), AppLauncher::State::Idle, 5000);
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

    // Qt Test finds data functions through the meta-object, so it is a slot.
    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
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

    // The window that a launched program opens is expected, not an interruption.
    void windowWhileRunningIsTheProgramsOwn() {
        AppLauncher launcher(this);
        launcher.setSettleMilliseconds(100);
        launcher.launch(QStringLiteral("Draw"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("sleep 0.5")});
        QTRY_COMPARE(launcher.state(), AppLauncher::State::Running);
        launcher.windowOpened(QStringLiteral("w1"), QStringLiteral("tuxpaint"),
                              QStringLiteral("Tux Paint"));
        QCOMPARE(launcher.state(), AppLauncher::State::Running);
        QTRY_COMPARE_WITH_TIMEOUT(launcher.state(), AppLauncher::State::Idle, 5000);
        // It was never counted, so its closing later changes nothing either.
        launcher.windowClosed(QStringLiteral("w1"));
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
