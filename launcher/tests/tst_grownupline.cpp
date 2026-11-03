// SPDX-License-Identifier: Apache-2.0
#include "GrownUpLine.h"

#include <QDateTime>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

namespace {

using State = AppLauncher::State;
using Step = AppLauncher::GrownUpStep;

QString consoleIn(const QTemporaryDir& root) {
    return root.filePath(QStringLiteral("console_log.txt"));
}

QString contentIn(const QTemporaryDir& root) {
    return root.filePath(QStringLiteral("content_log.txt"));
}

// A wait that reads Steam's logs and processes in a folder of the test's,
// never those of whoever runs the tests.
SteamGameWait waitIn(const QTemporaryDir& root) {
    return SteamGameWait(root.path(), consoleIn(root),
                         root.filePath(QStringLiteral("bootstrap_log.txt")), contentIn(root));
}

// A line as Steam writes it, `secondsAgo` before now.
void steamLine(const QString& path, qint64 secondsAgo, const QByteArray& says) {
    QFile log(path);
    QVERIFY(log.open(QIODevice::WriteOnly | QIODevice::Append));
    const QString time = QDateTime::currentDateTime()
                             .addSecs(-secondsAgo)
                             .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    log.write('[' + time.toLatin1() + "] " + says + '\n');
}

QStringList pajamaSam2() {
    return {QStringLiteral("steam"), QStringLiteral("-applaunch"), QStringLiteral("292780")};
}

// Steam taking a tap on Pajama Sam 2.
QByteArray pajamaSam2Taken() {
    return "ExecCommandLine: \"'~/.steam/root/ubuntu12_32/steam' '-applaunch' '292780'\"";
}

// A launch step for Pajama Sam 2, as Steam writes it now.
void pajamaSam2Says(const QString& console, const QByteArray& says) {
    steamLine(console, 0, "GameAction [AppID 292780, ActionID 1] : LaunchApp " + says);
}

// Steam taking a tap on Pajama Sam 2 and asking about the game's cloud saves
// on a window the kiosk hides, as on the test VM while it was offline (#211).
void cloudQuestionForPajamaSam2(const QString& console) {
    steamLine(console, 0, pajamaSam2Taken());
    pajamaSam2Says(console, "changed task to SynchronizingCloud with \"\"");
    pajamaSam2Says(console, "waiting for user response to SynchronizingCloud \"syncfailed\"");
}

// The test VM's stuck run (#206): Steam took the tap, fetched scout, and took
// the game off its schedule `secondsAgo`, with the launch still at
// DownloadingDepots.
void stuckRun(const QString& console, const QString& content, qint64 secondsAgo) {
    steamLine(console, secondsAgo + 22, pajamaSam2Taken());
    steamLine(console, secondsAgo + 20,
              "GameAction [AppID 292780, ActionID 1] : LaunchApp changed task to "
              "DownloadingDepots with \"\"");
    steamLine(content, secondsAgo + 20,
              "AppID 1070560 update started : download 0/75258128, store 0/0, reuse 0/0, delta "
              "0/0, stage 0/223012461 ");
    steamLine(content, secondsAgo,
              "AppID 1070560 scheduler finished : removed from schedule (result No Error, state "
              "0xc) ");
    steamLine(content, secondsAgo,
              "AppID 292780 scheduler finished : removed from schedule (result Suspended, state "
              "0xc) ");
}

// A tap on Pajama Sam 2 while Steam's servers turn its saved sign-in down, as
// on the kids' laptop on 2026-10-27 (#182). It ends at its first look.
void refusedTap(SteamGameWait& wait, const QString& console) {
    wait.startLaunch(pajamaSam2());
    steamLine(console, 0, "LogonFailure Access Denied");
    QVERIFY(!wait.stillComing());
}

// Which line the grown-up screen adds after a failed launch (ADR-0045,
// ADR-0049).
class GrownUpLineTest : public QObject {
    Q_OBJECT

private slots:
    // Only a failed launch gets a line. A window that opened on its own, a
    // tile that is coming soon, and a launch that is starting or open get
    // none, even right after a tap whose sign-in was turned down: the
    // launcher keeps what Steam's logs said until the next tap.
    void onlyAFailedLaunchGetsALine_data() {
        QTest::addColumn<State>("state");
        QTest::addColumn<Step>("step");
        QTest::newRow("failed") << State::Failed << Step::LogOutForAGrownUp;
        QTest::newRow("a window that opened on its own") << State::Interrupted << Step::None;
        QTest::newRow("a coming-soon tile") << State::ComingSoon << Step::None;
        QTest::newRow("starting") << State::Starting << Step::None;
        QTest::newRow("open") << State::Running << Step::None;
        QTest::newRow("at the tiles") << State::Idle << Step::None;
    }

    void onlyAFailedLaunchGetsALine() {
        QFETCH(const State, state);
        QFETCH(const Step, step);
        const QTemporaryDir root;
        SteamGameWait wait = waitIn(root);
        refusedTap(wait, consoleIn(root));
        QCOMPARE(GrownUpLine::step(state, wait), step);
    }

    // #182: Steam's servers turned the saved sign-in down. Nothing helps
    // until a grown-up signs Steam in again, so the line says a grown-up can
    // fix it, also once Steam's later tries fail with No Connection.
    void aRefusedSignInSaysAGrownUpCanFixIt() {
        const QTemporaryDir root;
        const QString console = consoleIn(root);
        SteamGameWait wait = waitIn(root);
        refusedTap(wait, console);
        QCOMPARE(GrownUpLine::step(State::Failed, wait), Step::LogOutForAGrownUp);
        steamLine(console, 0, "LogonFailure No Connection");
        wait.startLaunch(pajamaSam2());
        QVERIFY(!wait.stillComing());
        QCOMPARE(GrownUpLine::step(State::Failed, wait), Step::LogOutForAGrownUp);
    }

    // A stall seen while Steam's sign-in is turned down ends the wait on the
    // sign-in, and the line is the sign-in's, not the stall's.
    void aRefusedSignInSeenWithAStallSaysAGrownUpCanFixIt() {
        const QTemporaryDir root;
        const QString console = consoleIn(root);
        SteamGameWait wait = waitIn(root);
        wait.startLaunch(pajamaSam2());
        stuckRun(console, contentIn(root), 90);
        steamLine(console, 0, "LogonFailure Access Denied");
        QVERIFY(!wait.stillComing());
        QCOMPARE(GrownUpLine::step(State::Failed, wait), Step::LogOutForAGrownUp);
    }

    // Started again after a stall, Steam's saved sign-in is turned down.
    void aRefusedSignInAfterTheRestartSaysAGrownUpCanFixIt() {
        const QTemporaryDir root;
        const QString console = consoleIn(root);
        SteamGameWait wait = waitIn(root);
        wait.startLaunch(pajamaSam2());
        stuckRun(console, contentIn(root), 90);
        QVERIFY(wait.stillComing());
        QVERIFY(wait.restartSteam());
        wait.steamRestarted(true);
        steamLine(console, 0, "LogonFailure Access Denied");
        QVERIFY(!wait.stillComing());
        QCOMPARE(GrownUpLine::step(State::Failed, wait), Step::LogOutForAGrownUp);
    }

    // #211, then #182: offline, Steam asked about the game's cloud saves and
    // kept the question. Back online, its saved sign-in is turned down. A
    // later tap on the held game ends on the question, but the sign-in comes
    // first: logged back in, the next tap would meet it again.
    void aGameHeldByAQuestionWithItsSignInTurnedDownSaysAGrownUpCanFixIt() {
        const QTemporaryDir root;
        const QString console = consoleIn(root);
        SteamGameWait wait = waitIn(root);
        steamLine(console, 0, "LogonFailure No Connection");
        wait.startLaunch(pajamaSam2());
        cloudQuestionForPajamaSam2(console);
        QVERIFY(!wait.stillComing());
        QCOMPARE(GrownUpLine::step(State::Failed, wait), Step::None);
        steamLine(console, 0, "LogonFailure Access Denied");
        wait.startLaunch(pajamaSam2());
        steamLine(console, 0, pajamaSam2Taken());
        QVERIFY(!wait.stillComing());
        QCOMPARE(wait.lastStep(), SteamLaunchLog::Step::WaitingForAnswer);
        QCOMPARE(GrownUpLine::step(State::Failed, wait), Step::LogOutForAGrownUp);
    }

    // Started again, Steam took the tap and stopped the same way, which says
    // to log out and back in (ADR-0045). Its sign-in was turned down
    // meanwhile, and that line wins: logging out and back in would only meet
    // the sign-in.
    void aRefusedSignInWinsOverLoggingOutAndBackIn() {
        const QTemporaryDir root;
        const QString console = consoleIn(root);
        const QString content = contentIn(root);
        SteamGameWait wait = waitIn(root);
        wait.startLaunch(pajamaSam2());
        stuckRun(console, content, 90);
        QVERIFY(wait.stillComing());
        QVERIFY(wait.restartSteam());
        wait.steamRestarted(true);
        steamLine(console, 0, pajamaSam2Taken());
        pajamaSam2Says(console, "changed task to DownloadingDepots with \"\"");
        QVERIFY(wait.stillComing());
        steamLine(console, 0, "LogonFailure Access Denied");
        steamLine(content, 65,
                  "AppID 292780 scheduler finished : removed from schedule (result Suspended, "
                  "state 0xc) ");
        QVERIFY(!wait.stillComing());
        QVERIFY(wait.logOutWouldHelp());
        QCOMPARE(GrownUpLine::step(State::Failed, wait), Step::LogOutForAGrownUp);
    }

    // With the network down Steam cannot reach its servers and tries again
    // by itself. Logging out does not bring the network back, so there is no
    // line: not at a plain tap, not at the tap that meets the cloud
    // question, and not at a later tap on the game it holds.
    void noLineWhileSteamCannotReachItsServers() {
        const QTemporaryDir root;
        const QString console = consoleIn(root);
        SteamGameWait wait = waitIn(root);
        wait.setOfflineLimitMilliseconds(0);
        steamLine(console, 0, "LogonFailure No Connection");
        wait.startLaunch(pajamaSam2());
        QVERIFY(!wait.stillComing());
        QCOMPARE(wait.signIn(), SteamSignIn::Now::NoConnection);
        QCOMPARE(GrownUpLine::step(State::Failed, wait), Step::None);
        wait.startLaunch(pajamaSam2());
        cloudQuestionForPajamaSam2(console);
        QVERIFY(!wait.stillComing());
        QCOMPARE(GrownUpLine::step(State::Failed, wait), Step::None);
        wait.startLaunch(pajamaSam2());
        steamLine(console, 0, pajamaSam2Taken());
        QVERIFY(!wait.stillComing());
        QCOMPARE(wait.lastStep(), SteamLaunchLog::Step::WaitingForAnswer);
        QCOMPARE(GrownUpLine::step(State::Failed, wait), Step::None);
    }

    // A tile that is not a Steam game gets no line, even while Steam's
    // sign-in is turned down and right after a Steam tap that got one.
    void noLineForATileThatIsNotASteamGame() {
        const QTemporaryDir root;
        SteamGameWait wait = waitIn(root);
        refusedTap(wait, consoleIn(root));
        wait.startLaunch({QStringLiteral("tuxpaint")});
        QVERIFY(!wait.stillComing());
        QCOMPARE(GrownUpLine::step(State::Failed, wait), Step::None);
    }

    // Once Steam has signed in again, a launch that fails for no reason
    // Steam's logs give gets no line.
    void theLineGoesOnceSteamHasSignedInAgain() {
        const QTemporaryDir root;
        const QString console = consoleIn(root);
        SteamGameWait wait = waitIn(root);
        refusedTap(wait, console);
        steamLine(console, 0, "Waiting for compat in post-logon took: 0.098334s");
        wait.startLaunch(pajamaSam2());
        steamLine(console, 0, pajamaSam2Taken());
        QVERIFY(!wait.stillComing());
        QCOMPARE(GrownUpLine::step(State::Failed, wait), Step::None);
    }

    // A new tap forgets the last one's sign-in: a Steam tile whose program
    // fails before the launcher's first look gets no line.
    void aNewTapHasNoLineBeforeItsFirstLook() {
        const QTemporaryDir root;
        SteamGameWait wait = waitIn(root);
        refusedTap(wait, consoleIn(root));
        wait.startLaunch(pajamaSam2());
        QCOMPARE(GrownUpLine::step(State::Failed, wait), Step::None);
    }

    // ADR-0045's line still comes in its own cases: a stall that starting
    // Steam again could not mend (#206) ...
    void aStallSteamCouldNotMendStillSaysToLogOutAndBackIn() {
        const QTemporaryDir root;
        SteamGameWait wait = waitIn(root);
        wait.startLaunch(pajamaSam2());
        stuckRun(consoleIn(root), contentIn(root), 90);
        QVERIFY(wait.stillComing());
        QVERIFY(wait.restartSteam());
        wait.steamRestarted(false);
        QVERIFY(!wait.stillComing());
        QCOMPARE(GrownUpLine::step(State::Failed, wait), Step::LogOutAndBackIn);
    }

    // ... and a later tap on a game the cloud question holds, once Steam has
    // signed in again (#211).
    void aGameTheCloudQuestionHoldsStillSaysToLogOutAndBackIn() {
        const QTemporaryDir root;
        const QString console = consoleIn(root);
        SteamGameWait wait = waitIn(root);
        steamLine(console, 0, "LogonFailure No Connection");
        wait.startLaunch(pajamaSam2());
        cloudQuestionForPajamaSam2(console);
        QVERIFY(!wait.stillComing());
        steamLine(console, 0, "Waiting for compat in post-logon took: 0.098334s");
        wait.startLaunch(pajamaSam2());
        steamLine(console, 0, pajamaSam2Taken());
        QVERIFY(!wait.stillComing());
        QCOMPARE(GrownUpLine::step(State::Failed, wait), Step::LogOutAndBackIn);
    }
};

} // namespace

QTEST_GUILESS_MAIN(GrownUpLineTest)
#include "tst_grownupline.moc"
