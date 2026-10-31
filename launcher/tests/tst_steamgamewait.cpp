// SPDX-License-Identifier: Apache-2.0
#include "SteamGameWait.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

namespace {

// A process directory as /proc would show it, in a folder of the test's own.
void addProcess(const QTemporaryDir& root, const QString& pid, const QByteArray& name,
                const QList<QByteArray>& arguments) {
    QVERIFY(QDir(root.path()).mkpath(pid));
    QFile comm(root.filePath(pid + QStringLiteral("/comm")));
    QVERIFY(comm.open(QIODevice::WriteOnly));
    comm.write(name + '\n');
    QFile cmdline(root.filePath(pid + QStringLiteral("/cmdline")));
    QVERIFY(cmdline.open(QIODevice::WriteOnly));
    for (const QByteArray& argument : arguments) {
        cmdline.write(argument + '\0');
    }
}

QStringList steamGame() {
    return {QStringLiteral("steam"), QStringLiteral("-applaunch"), QStringLiteral("294650")};
}

// A launch step as Steam writes it in its log for the game 294650.
void steamSays(const QString& path, const QByteArray& says) {
    QFile log(path);
    QVERIFY(log.open(QIODevice::WriteOnly | QIODevice::Append));
    log.write("[2026-10-20 15:30:43] GameAction [AppID 294650, ActionID 1] : LaunchApp " + says +
              '\n');
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

// The client's own start command, as the login's `steam -silent` runs it.
QByteArray ownStart() {
    return "ExecCommandLine: \"'/var/home/child/.local/share/Steam/ubuntu12_32/"
           "steam' '-srt-logger-opened' '-silent'\"";
}

QStringList pajamaSam2() {
    return {QStringLiteral("steam"), QStringLiteral("-applaunch"), QStringLiteral("292780")};
}

// Steam taking a tap on Pajama Sam 2.
QByteArray pajamaSam2Taken() {
    return "ExecCommandLine: \"'~/.steam/root/ubuntu12_32/steam' '-applaunch' '292780'\"";
}

// A launch step for Pajama Sam 2, as Steam writes it now.
void pajamaSam2Says(const QString& path, const QByteArray& says) {
    steamLine(path, 0, "GameAction [AppID 292780, ActionID 1] : LaunchApp " + says);
}

// The test VM's stuck run (#206): Steam took the tap, fetched scout, and
// took the game off its schedule `secondsAgo`, with the launch still at
// DownloadingDepots.
void stuckRun(const QString& console, const QString& content, qint64 secondsAgo) {
    steamLine(console, secondsAgo + 22, pajamaSam2Taken());
    steamLine(console, secondsAgo + 20,
              "GameAction [AppID 292780, ActionID 1] : LaunchApp changed task to "
              "DownloadingDepots with \"\"");
    steamLine(content, secondsAgo + 20,
              "AppID 292780 scheduler update : Priority First, not played for 7421 seconds, "
              "update disabled for 0 seconds");
    steamLine(content, secondsAgo + 20,
              "AppID 1070560 scheduler update : Priority First, not played for 1 seconds, "
              "update disabled for 0 seconds");
    steamLine(content, secondsAgo + 20,
              "AppID 1070560 update started : download 0/75258128, store 0/0, reuse 0/0, delta "
              "0/0, stage 0/223012461 ");
    steamLine(content, secondsAgo,
              "AppID 1070560 finished update, 1 mounted depots (BuildID 26115262) : 1070561 "
              "(8475493586714137016),");
    steamLine(content, secondsAgo,
              "AppID 1070560 scheduler finished : removed from schedule (result No Error, state "
              "0xc) ");
    steamLine(content, secondsAgo,
              "AppID 292780 scheduler finished : removed from schedule (result Suspended, state "
              "0xc) ");
}

// The stuck run seen and Steam started again for it, as AppLauncher does
// when restartSteam() says so.
void restartedAfterAStall(SteamGameWait& wait, const QString& console, const QString& content) {
    wait.startLaunch(pajamaSam2());
    stuckRun(console, content, 90);
    QVERIFY(wait.stillComing());
    QVERIFY(wait.restartSteam());
    wait.steamRestarted(true);
}

// The cases without a log of their own read one in a folder of the test's,
// never the Steam log of whoever runs the tests.
const QTemporaryDir& dataHome() {
    static const QTemporaryDir home;
    return home;
}

// Whether a Steam game is still on its way (ADR-0029, #132, ADR-0036).
class SteamGameWaitTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        QVERIFY(dataHome().isValid());
        qputenv("XDG_DATA_HOME", dataHome().path().toUtf8());
    }

    void theAppIdComesFromApplaunch_data() {
        QTest::addColumn<QStringList>("exec");
        QTest::addColumn<QString>("appId");
        QTest::newRow("steam") << steamGame() << QStringLiteral("294650");
        QTest::newRow("full path")
            << QStringList{QStringLiteral("/usr/bin/steam"), QStringLiteral("-applaunch"),
                           QStringLiteral("70")}
            << QStringLiteral("70");
        QTest::newRow("not steam")
            << QStringList{QStringLiteral("scummvm"), QStringLiteral("-applaunch"),
                           QStringLiteral("70")}
            << QString();
        QTest::newRow("no app id")
            << QStringList{QStringLiteral("steam"), QStringLiteral("-applaunch")} << QString();
        QTest::newRow("not a number")
            << QStringList{QStringLiteral("steam"), QStringLiteral("-applaunch"),
                           QStringLiteral("x")}
            << QString();
        QTest::newRow("nothing") << QStringList{} << QString();
    }

    void theAppIdComesFromApplaunch() {
        QFETCH(const QStringList, exec);
        QFETCH(const QString, appId);
        QCOMPARE(SteamGameWait::appIdOf(exec), appId);
    }

    // With nothing to say Steam is working on it, a Steam game is not waited
    // for past the usual grace.
    void aSteamGameAloneIsNotWaitedFor() {
        const QTemporaryDir root;
        SteamGameWait wait(root.path());
        wait.startLaunch(steamGame());
        QVERIFY(!wait.stillComing());
    }

    void steamStartingAtLoginIsWaitedFor() {
        const QTemporaryDir root;
        SteamGameWait wait(root.path());
        wait.setStartupMilliseconds(200);
        wait.steamStartedAtLogin();
        wait.startLaunch(steamGame());
        QVERIFY(wait.stillComing());
        QTRY_VERIFY(!wait.stillComing());
    }

    // Steam starting says nothing about a tile that is not a Steam game.
    void onlyASteamGameIsWaitedFor() {
        const QTemporaryDir root;
        SteamGameWait wait(root.path());
        wait.steamStartedAtLogin();
        wait.startLaunch({QStringLiteral("tuxpaint")});
        QVERIFY(!wait.stillComing());
    }

    void theGamesReaperIsWaitedFor() {
        const QTemporaryDir root;
        SteamGameWait wait(root.path());
        wait.startLaunch(steamGame());
        QVERIFY(wait.steamGame());
        QVERIFY(!wait.stillComing());
        addProcess(root, QStringLiteral("8447"), "reaper",
                   {"/home/ada/.local/share/Steam/ubuntu12_32/reaper", "SteamLaunch",
                    "AppId=294650", "--", "steam-launch-wrapper"});
        QVERIFY(wait.stillComing());
    }

    // The same game left running from an earlier start brings no new window;
    // its reaper does not keep a new press waiting.
    void aReaperFromBeforeThePressDoesNotCount() {
        const QTemporaryDir root;
        addProcess(root, QStringLiteral("8447"), "reaper",
                   {"reaper", "SteamLaunch", "AppId=294650"});
        SteamGameWait wait(root.path());
        wait.startLaunch(steamGame());
        QVERIFY(!wait.stillComing());
        addProcess(root, QStringLiteral("9001"), "reaper",
                   {"reaper", "SteamLaunch", "AppId=294650"});
        QVERIFY(wait.stillComing());
    }

    // Only Steam's reaper for this game counts, never a command line that
    // only mentions it.
    void anotherProcessIsNotTheReaper_data() {
        QTest::addColumn<QByteArray>("name");
        QTest::addColumn<QList<QByteArray>>("arguments");
        QTest::newRow("another game")
            << QByteArray("reaper") << QList<QByteArray>{"reaper", "SteamLaunch", "AppId=70"};
        QTest::newRow("not a launch")
            << QByteArray("reaper") << QList<QByteArray>{"reaper", "AppId=294650"};
        QTest::newRow("named something else")
            << QByteArray("bash")
            << QList<QByteArray>{"bash", "-c", "reaper SteamLaunch AppId=294650"};
        QTest::newRow("a longer app id")
            << QByteArray("reaper") << QList<QByteArray>{"reaper", "SteamLaunch", "AppId=2946501"};
    }

    void anotherProcessIsNotTheReaper() {
        QFETCH(const QByteArray, name);
        QFETCH(const QList<QByteArray>, arguments);
        const QTemporaryDir root;
        SteamGameWait wait(root.path());
        wait.startLaunch(steamGame());
        addProcess(root, QStringLiteral("100"), name, arguments);
        QVERIFY(!wait.stillComing());
    }

    // A reaper that never brings a window does not keep the child waiting
    // for ever.
    void theOuterLimitEndsTheWait() {
        const QTemporaryDir root;
        SteamGameWait wait(root.path());
        wait.setLimitMilliseconds(200);
        wait.steamStartedAtLogin();
        wait.startLaunch(steamGame());
        addProcess(root, QStringLiteral("8447"), "reaper",
                   {"reaper", "SteamLaunch", "AppId=294650"});
        QVERIFY(wait.stillComing());
        QTRY_VERIFY(!wait.stillComing());
    }

    // A new press starts the limit again and forgets the last game.
    void eachLaunchStartsAfresh() {
        const QTemporaryDir root;
        SteamGameWait wait(root.path());
        wait.startLaunch(steamGame());
        addProcess(root, QStringLiteral("8447"), "reaper",
                   {"reaper", "SteamLaunch", "AppId=294650"});
        QVERIFY(wait.stillComing());
        wait.startLaunch({QStringLiteral("tuxpaint")});
        QVERIFY(!wait.steamGame());
        QVERIFY(!wait.stillComing());
    }

    void nothingLaunchedIsNotWaitedFor() {
        SteamGameWait wait;
        QVERIFY(!wait.stillComing());
    }

    // As on the kids' laptop (2026-10-20): Steam up, no reaper yet, while
    // Steam downloads what the game needs. Its log says it is working.
    void steamWorkingOnTheGameIsWaitedFor() {
        const QTemporaryDir root;
        const QString log = root.filePath(QStringLiteral("console_log.txt"));
        SteamGameWait wait(root.path(), log);
        wait.startLaunch(steamGame());
        QVERIFY(!wait.stillComing());
        QVERIFY(!wait.steamWorking());
        steamSays(log, "changed task to DownloadingDepots with \"\"");
        QVERIFY(wait.stillComing());
        QVERIFY(wait.steamWorking());
        QCOMPARE(wait.lastStep(), SteamLaunchLog::Step::Working);
    }

    // A question on a window the kiosk hides, or a refusal, is not work:
    // nothing will come, so the launch is not waited for.
    void aQuestionOrARefusalIsNotWaitedFor_data() {
        QTest::addColumn<QByteArray>("says");
        QTest::newRow("question") << QByteArray(
            "waiting for user response to SynchronizingCloud \"syncfailed\"");
        QTest::newRow("refusal") << QByteArray("failed with AppError_33 with \"\"");
    }

    void aQuestionOrARefusalIsNotWaitedFor() {
        QFETCH(const QByteArray, says);
        const QTemporaryDir root;
        const QString log = root.filePath(QStringLiteral("console_log.txt"));
        SteamGameWait wait(root.path(), log);
        wait.startLaunch(steamGame());
        steamSays(log, "changed task to SynchronizingCloud with \"\"");
        steamSays(log, says);
        QVERIFY(!wait.stillComing());
        QVERIFY(!wait.steamWorking());
    }

    // Work outlasts the reaper's limit, but not the longer one for work.
    void theWorkLimitEndsTheWait() {
        const QTemporaryDir root;
        const QString log = root.filePath(QStringLiteral("console_log.txt"));
        SteamGameWait wait(root.path(), log);
        wait.setLimitMilliseconds(100);
        wait.setWorkLimitMilliseconds(400);
        wait.startLaunch(steamGame());
        steamSays(log, "changed task to DownloadingDepots with \"\"");
        QTest::qWait(200);
        QVERIFY(wait.stillComing());
        QTRY_VERIFY(!wait.stillComing());
    }

    // Once the download is done the game itself starts, and a Proton game can
    // take a minute more for its first window. The reaper then gets the usual
    // limit counted from the last work, not from the tap.
    void afterTheWorkTheReaperGetsItsLimitAgain() {
        const QTemporaryDir root;
        const QString log = root.filePath(QStringLiteral("console_log.txt"));
        SteamGameWait wait(root.path(), log);
        wait.setLimitMilliseconds(300);
        wait.startLaunch(steamGame());
        steamSays(log, "changed task to DownloadingDepots with \"\"");
        QTest::qWait(350);
        QVERIFY(wait.stillComing());
        steamSays(log, "changed task to Completed with \"\"");
        addProcess(root, QStringLiteral("8447"), "reaper",
                   {"reaper", "SteamLaunch", "AppId=294650"});
        QVERIFY(wait.stillComing());
        QVERIFY(!wait.steamWorking());
        QTRY_VERIFY(!wait.stillComing());
    }

    // Work that ends just before the work limit still leaves the game's
    // reaper its own limit, counted from that work, past the work limit.
    void workEndingNearTheWorkLimitLeavesTheReaperItsLimit() {
        const QTemporaryDir root;
        const QString log = root.filePath(QStringLiteral("console_log.txt"));
        SteamGameWait wait(root.path(), log);
        wait.setLimitMilliseconds(400);
        wait.setWorkLimitMilliseconds(300);
        wait.startLaunch(steamGame());
        steamSays(log, "changed task to DownloadingDepots with \"\"");
        QTest::qWait(250);
        QVERIFY(wait.stillComing());
        steamSays(log, "changed task to Completed with \"\"");
        addProcess(root, QStringLiteral("8447"), "reaper",
                   {"reaper", "SteamLaunch", "AppId=294650"});
        QTest::qWait(150); // past the work limit, inside the reaper's.
        QVERIFY(wait.stillComing());
        QTRY_VERIFY(!wait.stillComing());
    }

    // Steam installing an update of itself is work: the launch waits for it
    // and the starting screen says the game is getting ready (ADR-0037).
    void steamUpdatingItselfIsWaitedFor() {
        const QTemporaryDir root;
        const QString log = root.filePath(QStringLiteral("console_log.txt"));
        const QString bootstrap = root.filePath(QStringLiteral("bootstrap_log.txt"));
        SteamGameWait wait(root.path(), log, bootstrap);
        wait.setLimitMilliseconds(100);
        steamLine(bootstrap, 20, "Startup - updater built Sep  3 2026 01:31:48");
        steamLine(bootstrap, 20, "Found pending update");
        steamLine(bootstrap, 19, "Installing update...");
        wait.startLaunch(steamGame());
        QTest::qWait(150);
        QVERIFY(wait.stillComing());
        QVERIFY(wait.steamWorking());
        steamLine(bootstrap, 0, "Update complete, launching Steam...");
        steamLine(bootstrap, 0, "Shutdown");
        steamLine(bootstrap, 0, "Startup - updater built Oct 13 2026 18:44:25");
        QVERIFY(wait.stillComing());
        QVERIFY(wait.steamWorking());
        // Still starting between its startup time and its own start command.
        steamLine(log, 0, "System startup time: 16.79 seconds");
        QVERIFY(wait.stillComing());
        QVERIFY(wait.steamWorking());
        // Started without the tap: still getting ready, until it takes the
        // tap that is sent again.
        steamLine(log, 0, ownStart());
        QVERIFY(wait.stillComing());
        QVERIFY(wait.steamWorking());
        steamLine(log, 0,
                  "ExecCommandLine: \"'~/.steam/root/ubuntu12_32/steam' '-applaunch' '294650'\"");
        QVERIFY(!wait.stillComing());
        QVERIFY(!wait.steamWorking());
    }

    // The client restarted after its update without taking the tap: the tap
    // is sent again, once.
    void aTapLostInAnUpdateIsSentAgainOnce() {
        const QTemporaryDir root;
        const QString log = root.filePath(QStringLiteral("console_log.txt"));
        SteamGameWait wait(root.path(), log, root.filePath(QStringLiteral("bootstrap_log.txt")));
        steamLine(log, 3600, ownStart());
        wait.startLaunch(steamGame());
        wait.stillComing();
        QVERIFY(!wait.sendAgain());
        steamLine(log, 5, ownStart());
        wait.stillComing();
        QVERIFY(wait.sendAgain());
        wait.stillComing();
        QVERIFY(!wait.sendAgain());
        wait.startLaunch(steamGame());
        wait.stillComing();
        QVERIFY(!wait.sendAgain());
    }

    // Steam takes a tap it held in the same second as its own start command,
    // so the new client gets a moment before the tap counts as lost.
    void aClientJustStartedHasAMomentToTakeTheTap() {
        const QTemporaryDir root;
        const QString log = root.filePath(QStringLiteral("console_log.txt"));
        SteamGameWait wait(root.path(), log, root.filePath(QStringLiteral("bootstrap_log.txt")));
        wait.startLaunch(steamGame());
        steamLine(log, 0, ownStart());
        wait.stillComing();
        QVERIFY(!wait.sendAgain());
    }

    void aTapSteamTookIsNotSentAgain() {
        const QTemporaryDir root;
        const QString log = root.filePath(QStringLiteral("console_log.txt"));
        SteamGameWait wait(root.path(), log, root.filePath(QStringLiteral("bootstrap_log.txt")));
        wait.startLaunch(steamGame());
        steamLine(log, 5, ownStart());
        steamLine(log, 5,
                  "ExecCommandLine: \"'~/.steam/root/ubuntu12_32/steam' '-applaunch' '294650'\"");
        wait.stillComing();
        QVERIFY(!wait.sendAgain());
    }

    // A new reaper for the game is Steam working on it, whatever its log says.
    void aGameWithANewReaperIsNotSentAgain() {
        const QTemporaryDir root;
        const QString log = root.filePath(QStringLiteral("console_log.txt"));
        SteamGameWait wait(root.path(), log, root.filePath(QStringLiteral("bootstrap_log.txt")));
        wait.startLaunch(steamGame());
        steamLine(log, 5, ownStart());
        addProcess(root, QStringLiteral("8447"), "reaper",
                   {"reaper", "SteamLaunch", "AppId=294650"});
        wait.stillComing();
        QVERIFY(!wait.sendAgain());
    }

    // #182: Steam starting at login while its saved sign-in is turned down.
    // It never finishes starting, but that is not work: the wait ends at the
    // next look, with no "Getting it ready".
    void aRefusedSignInEndsTheWait() {
        const QTemporaryDir root;
        const QString log = root.filePath(QStringLiteral("console_log.txt"));
        const QString bootstrap = root.filePath(QStringLiteral("bootstrap_log.txt"));
        SteamGameWait wait(root.path(), log, bootstrap);
        steamLine(bootstrap, 30, "Startup - updater built Oct 13 2026 18:44:25");
        steamLine(log, 28, "Client version: 1792618285");
        wait.startLaunch(steamGame());
        QVERIFY(wait.stillComing());
        QVERIFY(wait.steamWorking());
        steamLine(log, 0, "LogonFailure Access Denied");
        QVERIFY(!wait.stillComing());
        QVERIFY(!wait.steamWorking());
        QCOMPARE(wait.signIn(), SteamSignIn::Now::Refused);
        // The tries after it fail with No Connection; still refused.
        steamLine(log, 0, "LogonFailure No Connection");
        QVERIFY(!wait.stillComing());
        QCOMPARE(wait.signIn(), SteamSignIn::Now::Refused);
        QVERIFY(!wait.sendAgain());
    }

    // A Steam that cannot reach its servers gets a short wait to come back,
    // then the wait ends, even while it is still starting.
    void noConnectionGetsAShortWait() {
        const QTemporaryDir root;
        const QString log = root.filePath(QStringLiteral("console_log.txt"));
        const QString bootstrap = root.filePath(QStringLiteral("bootstrap_log.txt"));
        SteamGameWait wait(root.path(), log, bootstrap);
        wait.setOfflineLimitMilliseconds(200);
        steamLine(bootstrap, 30, "Startup - updater built Oct 13 2026 18:44:25");
        steamLine(log, 28, "Client version: 1792618285");
        steamLine(log, 10, "LogonFailure No Connection");
        wait.startLaunch(steamGame());
        QVERIFY(wait.stillComing());
        QVERIFY(!wait.steamWorking());
        QCOMPARE(wait.signIn(), SteamSignIn::Now::NoConnection);
        QTRY_VERIFY(!wait.stillComing());
        QCOMPARE(wait.signIn(), SteamSignIn::Now::NoConnection);
    }

    // Steam back online in the short wait: starting is work again.
    void signingInInTheShortWaitIsWorkAgain() {
        const QTemporaryDir root;
        const QString log = root.filePath(QStringLiteral("console_log.txt"));
        const QString bootstrap = root.filePath(QStringLiteral("bootstrap_log.txt"));
        SteamGameWait wait(root.path(), log, bootstrap);
        wait.setOfflineLimitMilliseconds(100);
        steamLine(bootstrap, 30, "Startup - updater built Oct 13 2026 18:44:25");
        steamLine(log, 28, "Client version: 1792618285");
        steamLine(log, 10, "LogonFailure No Connection");
        wait.startLaunch(steamGame());
        QVERIFY(wait.stillComing());
        steamLine(log, 0, "Waiting for compat in post-logon took: 0.098334s");
        QTest::qWait(150);
        QVERIFY(wait.stillComing());
        QVERIFY(wait.steamWorking());
        QCOMPARE(wait.signIn(), SteamSignIn::Now::Fine);
    }

    // Steam starts games while it cannot reach its servers (2026-10-28): once
    // its log is about the game, the sign-in no longer decides.
    void aGameSteamHasInHandIsWaitedForOffline() {
        const QTemporaryDir root;
        const QString log = root.filePath(QStringLiteral("console_log.txt"));
        SteamGameWait wait(root.path(), log, root.filePath(QStringLiteral("bootstrap_log.txt")));
        wait.setOfflineLimitMilliseconds(0);
        steamLine(log, 10, "LogonFailure No Connection");
        wait.startLaunch(steamGame());
        steamSays(log, "changed task to RunningInstallScript with \"\"");
        QVERIFY(wait.stillComing());
        QVERIFY(wait.steamWorking());
        QCOMPARE(wait.signIn(), SteamSignIn::Now::Fine);
    }

    // The journal says what Steam said last, sign-in first, for the grown-up.
    void journalNoteSaysWhatSteamSaid_data() {
        QTest::addColumn<QByteArray>("signInLine");
        QTest::addColumn<QByteArray>("gameLine");
        QTest::addColumn<QString>("note");
        QTest::newRow("refused sign-in")
            << QByteArray("LogonFailure Access Denied") << QByteArray()
            << QStringLiteral("Steam could not sign in, so Ada's game did not start. Steam needs "
                              "to be signed in again.");
        QTest::newRow("no connection")
            << QByteArray("LogonFailure No Connection") << QByteArray()
            << QStringLiteral("Steam could not reach the internet, so Ada's game did not start.");
        QTest::newRow("question")
            << QByteArray()
            << QByteArray("waiting for user response to SynchronizingCloud \"syncfailed\"")
            << QStringLiteral(
                   "Steam is waiting for an answer about Ada's game on a window the kiosk hides.");
        QTest::newRow("refusal") << QByteArray() << QByteArray("failed with AppError_33 with \"\"")
                                 << QStringLiteral("Steam refused to start Ada's game.");
        QTest::newRow("nothing said") << QByteArray() << QByteArray() << QString();
    }

    void journalNoteSaysWhatSteamSaid() {
        QFETCH(const QByteArray, signInLine);
        QFETCH(const QByteArray, gameLine);
        QFETCH(const QString, note);
        const QTemporaryDir root;
        const QString log = root.filePath(QStringLiteral("console_log.txt"));
        const QString bootstrap = root.filePath(QStringLiteral("bootstrap_log.txt"));
        SteamGameWait wait(root.path(), log, bootstrap);
        steamLine(bootstrap, 30, "Startup - updater built Oct 13 2026 18:44:25");
        steamLine(log, 28, "Client version: 1792618285");
        wait.startLaunch(steamGame());
        if (!signInLine.isEmpty()) {
            steamLine(log, 0, signInLine);
        }
        if (!gameLine.isEmpty()) {
            steamSays(log, gameLine);
        }
        wait.stillComing();
        QCOMPARE(wait.journalNote(QStringLiteral("Ada's game")), note);
    }

    // #206 on the test VM: Steam took the game off its schedule a minute
    // and a half ago and the launch still says it is downloading. The launch
    // waits, getting ready, and Steam is started again once.
    void aStalledLaunchIsWaitedForAndRestartedOnce() {
        const QTemporaryDir root;
        const QString log = root.filePath(QStringLiteral("console_log.txt"));
        const QString content = root.filePath(QStringLiteral("content_log.txt"));
        SteamGameWait wait(root.path(), log, root.filePath(QStringLiteral("bootstrap_log.txt")),
                           content);
        wait.startLaunch(pajamaSam2());
        QVERIFY(!wait.restartSteam());
        stuckRun(log, content, 90);
        QVERIFY(wait.stillComing());
        QVERIFY(wait.steamWorking());
        QVERIFY(wait.logOutWouldHelp());
        QVERIFY(wait.restartSteam());
        QVERIFY(!wait.restartSteam());
        QVERIFY(wait.stillComing());
        QVERIFY(wait.steamWorking());
        QVERIFY(!wait.restartSteam());
        QVERIFY(!wait.sendAgain());
    }

    // While Steam stops and starts again the child still sees "Getting it
    // ready"; once the new client has started without the game, the tap is
    // sent again, once.
    void theWaitGoesOnWhileSteamStartsAgain() {
        const QTemporaryDir root;
        const QString log = root.filePath(QStringLiteral("console_log.txt"));
        const QString content = root.filePath(QStringLiteral("content_log.txt"));
        SteamGameWait wait(root.path(), log, root.filePath(QStringLiteral("bootstrap_log.txt")),
                           content);
        wait.startLaunch(pajamaSam2());
        stuckRun(log, content, 90);
        QVERIFY(wait.stillComing());
        QVERIFY(wait.restartSteam());
        // The old client is told to shut down and fails the launch.
        steamLine(log, 0, "ExecCommandLine: \"'~/.steam/root/ubuntu12_32/steam' '-shutdown'\"");
        pajamaSam2Says(log, "changed task to Failed with \"\"");
        QVERIFY(wait.stillComing());
        QVERIFY(wait.steamWorking());
        QVERIFY(!wait.sendAgain());
        wait.steamRestarted(true);
        QVERIFY(wait.stillComing());
        QVERIFY(wait.steamWorking());
        QVERIFY(!wait.sendAgain());
        steamLine(log, 5, ownStart());
        QVERIFY(wait.stillComing());
        QVERIFY(wait.steamWorking());
        QVERIFY(wait.sendAgain());
        QVERIFY(wait.stillComing());
        QVERIFY(!wait.sendAgain());
        steamLine(log, 0, pajamaSam2Taken());
        pajamaSam2Says(log, "changed task to DownloadingDepots with \"\"");
        QVERIFY(wait.stillComing());
        QVERIFY(wait.steamWorking());
        QCOMPARE(wait.lastStep(), SteamLaunchLog::Step::Working);
        QVERIFY(!wait.restartSteam());
    }

    // The restarted Steam took the tap and stopped the same way: the wait
    // ends at once, with no second restart, and logging out is what helps.
    void aStallAfterTheRestartEndsTheWait() {
        const QTemporaryDir root;
        const QString log = root.filePath(QStringLiteral("console_log.txt"));
        const QString content = root.filePath(QStringLiteral("content_log.txt"));
        SteamGameWait wait(root.path(), log, root.filePath(QStringLiteral("bootstrap_log.txt")),
                           content);
        restartedAfterAStall(wait, log, content);
        steamLine(log, 5, ownStart());
        QVERIFY(wait.stillComing());
        QVERIFY(wait.sendAgain());
        steamLine(log, 0, pajamaSam2Taken());
        pajamaSam2Says(log, "changed task to DownloadingDepots with \"\"");
        QVERIFY(wait.stillComing());
        steamLine(content, 65,
                  "AppID 292780 scheduler finished : removed from schedule (result Suspended, "
                  "state 0xc) ");
        QVERIFY(!wait.stillComing());
        QVERIFY(!wait.restartSteam());
        QVERIFY(!wait.sendAgain());
        QVERIFY(wait.logOutWouldHelp());
        QCOMPARE(wait.journalNote(QStringLiteral("Pajama Sam 2")),
                 QStringLiteral("Steam stopped getting Pajama Sam 2 ready, so Pajama Sam 2 did not "
                                "start. Log the child out and back in, then try it again."));
    }

    // Started again, Steam took the tap, fetched soldier and started the
    // game, but no window came and no reaper runs: logging out is still what
    // helps, and the journal says starting Steam again did not bring it.
    void aRestartThatBringsNoGameStillSaysToLogOut() {
        const QTemporaryDir root;
        const QString log = root.filePath(QStringLiteral("console_log.txt"));
        const QString content = root.filePath(QStringLiteral("content_log.txt"));
        SteamGameWait wait(root.path(), log, root.filePath(QStringLiteral("bootstrap_log.txt")),
                           content);
        restartedAfterAStall(wait, log, content);
        steamLine(log, 5, ownStart());
        QVERIFY(wait.stillComing());
        QVERIFY(wait.sendAgain());
        steamLine(log, 0, pajamaSam2Taken());
        pajamaSam2Says(log, "changed task to DownloadingDepots with \"\"");
        QVERIFY(wait.stillComing());
        steamLine(content, 0,
                  "AppID 1391110 scheduler update : Priority First, not played for 1 seconds, "
                  "update disabled for 0 seconds");
        steamLine(content, 0,
                  "AppID 1391110 update started : download 0/192183760, store 0/0, reuse 0/0, "
                  "delta 0/0, stage 0/677401998 ");
        steamLine(content, 0, "AppID 1391110 finished update, 1 mounted depots : 1391111,");
        steamLine(content, 0,
                  "AppID 1391110 scheduler finished : removed from schedule (result No Error, "
                  "state 0xc) ");
        steamLine(content, 0,
                  "AppID 292780 scheduler finished : removed from schedule (result Suspended, "
                  "state 0xc) ");
        pajamaSam2Says(log, "changed task to RunningInstallScript with \"\"");
        QVERIFY(wait.stillComing());
        pajamaSam2Says(log, "changed task to WaitingGameWindow with \"\"");
        QVERIFY(!wait.stillComing());
        QVERIFY(!wait.restartSteam());
        QVERIFY(wait.logOutWouldHelp());
        QCOMPARE(wait.journalNote(QStringLiteral("Pajama Sam 2")),
                 QStringLiteral("Starting Steam again did not bring Pajama Sam 2. Log the child "
                                "out and back in, then try it again."));
    }

    void aRestartThatFailedEndsTheWait() {
        const QTemporaryDir root;
        const QString log = root.filePath(QStringLiteral("console_log.txt"));
        const QString content = root.filePath(QStringLiteral("content_log.txt"));
        SteamGameWait wait(root.path(), log, root.filePath(QStringLiteral("bootstrap_log.txt")),
                           content);
        wait.startLaunch(pajamaSam2());
        stuckRun(log, content, 90);
        QVERIFY(wait.stillComing());
        QVERIFY(wait.restartSteam());
        // What the old client wrote as it stopped is not Steam's reason.
        pajamaSam2Says(log, "changed task to Failed with \"\"");
        wait.steamRestarted(false);
        QVERIFY(!wait.stillComing());
        QVERIFY(wait.logOutWouldHelp());
        QCOMPARE(wait.journalNote(QStringLiteral("Pajama Sam 2")),
                 QStringLiteral("Steam stopped getting Pajama Sam 2 ready, so Pajama Sam 2 did not "
                                "start. Log the child out and back in, then try it again."));
    }

    // A Steam that is updating itself, cannot reach its servers, or has
    // started the game after all is not started again.
    void aStallIsNotRestartedWhenSteamCannotTakeATap_data() {
        QTest::addColumn<QString>("why");
        QTest::newRow("updating itself") << QStringLiteral("update");
        QTest::newRow("no connection") << QStringLiteral("offline");
        QTest::newRow("a new reaper") << QStringLiteral("reaper");
    }

    void aStallIsNotRestartedWhenSteamCannotTakeATap() {
        QFETCH(const QString, why);
        const QTemporaryDir root;
        const QString log = root.filePath(QStringLiteral("console_log.txt"));
        const QString bootstrap = root.filePath(QStringLiteral("bootstrap_log.txt"));
        const QString content = root.filePath(QStringLiteral("content_log.txt"));
        SteamGameWait wait(root.path(), log, bootstrap, content);
        wait.startLaunch(pajamaSam2());
        stuckRun(log, content, 90);
        if (why == QStringLiteral("update")) {
            steamLine(bootstrap, 10, "Startup - updater built Oct 21 2026 21:09:45");
            steamLine(bootstrap, 10, "Found pending update");
            steamLine(bootstrap, 10, "Installing update...");
        } else if (why == QStringLiteral("offline")) {
            steamLine(log, 0, "LogonFailure No Connection");
        } else {
            addProcess(root, QStringLiteral("8447"), "reaper",
                       {"reaper", "SteamLaunch", "AppId=292780"});
        }
        QVERIFY(wait.stillComing());
        QVERIFY(!wait.restartSteam());
    }

    // A stall seen while Steam cannot reach its servers: what Steam still
    // needs must come from them, so starting it again would not help. The
    // wait ends by #182's rules, and the journal names the connection, not
    // the stall, so the grown-up screen adds no line.
    void aStallWhileOfflineEndsOnTheConnection() {
        const QTemporaryDir root;
        const QString log = root.filePath(QStringLiteral("console_log.txt"));
        const QString content = root.filePath(QStringLiteral("content_log.txt"));
        SteamGameWait wait(root.path(), log, root.filePath(QStringLiteral("bootstrap_log.txt")),
                           content);
        wait.setOfflineLimitMilliseconds(200);
        wait.startLaunch(pajamaSam2());
        stuckRun(log, content, 90);
        steamLine(log, 0, "LogonFailure No Connection");
        QVERIFY(wait.stillComing());
        QVERIFY(!wait.steamWorking());
        QVERIFY(!wait.restartSteam());
        QTRY_VERIFY(!wait.stillComing());
        QVERIFY(!wait.restartSteam());
        QVERIFY(!wait.sendAgain());
        QCOMPARE(wait.signIn(), SteamSignIn::Now::NoConnection);
        QVERIFY(!wait.logOutWouldHelp());
        QCOMPARE(wait.journalNote(QStringLiteral("Pajama Sam 2")),
                 QStringLiteral("Steam could not reach the internet, so Pajama Sam 2 did not "
                                "start."));
    }

    // A stall seen while Steam's sign-in is turned down ends the wait at
    // that look, and the journal names the sign-in, not the stall.
    void aStallWithARefusedSignInEndsOnTheSignIn() {
        const QTemporaryDir root;
        const QString log = root.filePath(QStringLiteral("console_log.txt"));
        const QString content = root.filePath(QStringLiteral("content_log.txt"));
        SteamGameWait wait(root.path(), log, root.filePath(QStringLiteral("bootstrap_log.txt")),
                           content);
        wait.startLaunch(pajamaSam2());
        stuckRun(log, content, 90);
        steamLine(log, 0, "LogonFailure Access Denied");
        QVERIFY(!wait.stillComing());
        QVERIFY(!wait.restartSteam());
        QVERIFY(!wait.sendAgain());
        QCOMPARE(wait.signIn(), SteamSignIn::Now::Refused);
        QVERIFY(!wait.logOutWouldHelp());
        QCOMPARE(wait.journalNote(QStringLiteral("Pajama Sam 2")),
                 QStringLiteral("Steam could not sign in, so Pajama Sam 2 did not start. Steam "
                                "needs to be signed in again."));
    }

    // After a stall, the restarted Steam asked a question on a window the
    // kiosk hides. The journal says so, and logging out is not the answer.
    void theOtherReasonsComeFirstInTheJournal() {
        const QTemporaryDir root;
        const QString log = root.filePath(QStringLiteral("console_log.txt"));
        const QString content = root.filePath(QStringLiteral("content_log.txt"));
        SteamGameWait wait(root.path(), log, root.filePath(QStringLiteral("bootstrap_log.txt")),
                           content);
        restartedAfterAStall(wait, log, content);
        steamLine(log, 5, ownStart());
        QVERIFY(wait.stillComing());
        QVERIFY(wait.sendAgain());
        steamLine(log, 0, pajamaSam2Taken());
        pajamaSam2Says(log, "changed task to SynchronizingCloud with \"\"");
        pajamaSam2Says(log, "waiting for user response to SynchronizingCloud \"syncfailed\"");
        QVERIFY(!wait.stillComing());
        QCOMPARE(wait.journalNote(QStringLiteral("Pajama Sam 2")),
                 QStringLiteral("Steam is waiting for an answer about Pajama Sam 2 on a window "
                                "the kiosk hides."));
        QVERIFY(!wait.logOutWouldHelp());
    }

    // The restarted Steam cannot reach its servers for a moment: it gets the
    // offline wait of its own, not what is left of the tap's.
    void aRestartedSteamGetsAnOfflineWaitOfItsOwn() {
        const QTemporaryDir root;
        const QString log = root.filePath(QStringLiteral("console_log.txt"));
        const QString content = root.filePath(QStringLiteral("content_log.txt"));
        SteamGameWait wait(root.path(), log, root.filePath(QStringLiteral("bootstrap_log.txt")),
                           content);
        wait.setOfflineLimitMilliseconds(400);
        wait.startLaunch(pajamaSam2());
        stuckRun(log, content, 90);
        QVERIFY(wait.stillComing());
        QVERIFY(wait.restartSteam());
        QTest::qWait(500); // past the offline limit counted from the tap.
        wait.steamRestarted(true);
        steamLine(log, 0, "LogonFailure No Connection");
        QVERIFY(wait.stillComing());
        QCOMPARE(wait.signIn(), SteamSignIn::Now::NoConnection);
        QVERIFY(!wait.sendAgain());
        steamLine(log, 0, "Waiting for compat in post-logon took: 0.098334s");
        steamLine(log, 5, ownStart());
        QVERIFY(wait.stillComing());
        QCOMPARE(wait.signIn(), SteamSignIn::Now::Fine);
        QVERIFY(wait.sendAgain());
        QVERIFY(wait.stillComing());
        QVERIFY(!wait.sendAgain());
    }

    // A stall seen late in the tap's work limit, as after a game update: the
    // Steam started again gets a work limit of its own, takes the tap sent
    // again and fetches what the game needs. That limit still ends the wait.
    void aRestartedSteamGetsAWorkLimitOfItsOwn() {
        const QTemporaryDir root;
        const QString log = root.filePath(QStringLiteral("console_log.txt"));
        const QString content = root.filePath(QStringLiteral("content_log.txt"));
        SteamGameWait wait(root.path(), log, root.filePath(QStringLiteral("bootstrap_log.txt")),
                           content);
        wait.setWorkLimitMilliseconds(1000);
        wait.startLaunch(pajamaSam2());
        stuckRun(log, content, 90);
        QVERIFY(wait.stillComing());
        QVERIFY(wait.restartSteam());
        QTest::qWait(600);
        wait.steamRestarted(true);
        QTest::qWait(600); // past the work limit counted from the tap.
        QVERIFY(wait.stillComing());
        QVERIFY(wait.steamWorking());
        steamLine(log, 5, ownStart());
        QVERIFY(wait.stillComing());
        QVERIFY(wait.sendAgain());
        steamLine(log, 0, pajamaSam2Taken());
        pajamaSam2Says(log, "changed task to DownloadingDepots with \"\"");
        QVERIFY(wait.stillComing());
        QVERIFY(wait.steamWorking());
        QTRY_VERIFY(!wait.stillComing());
    }

    // A Steam started again may install an update of itself first. That is
    // work under the work limit, not the time a started client gets to take
    // the tap.
    void aSteamUpdatingItselfAfterTheRestartIsStillWaitedFor() {
        const QTemporaryDir root;
        const QString log = root.filePath(QStringLiteral("console_log.txt"));
        const QString bootstrap = root.filePath(QStringLiteral("bootstrap_log.txt"));
        const QString content = root.filePath(QStringLiteral("content_log.txt"));
        SteamGameWait wait(root.path(), log, bootstrap, content);
        wait.setRetakeLimitMilliseconds(200);
        restartedAfterAStall(wait, log, content);
        steamLine(bootstrap, 10, "Startup - updater built Oct 21 2026 21:09:45");
        steamLine(bootstrap, 10, "Found pending update");
        steamLine(bootstrap, 10, "Installing update...");
        QTest::qWait(300);
        QVERIFY(wait.stillComing());
        QVERIFY(wait.steamWorking());
        QVERIFY(!wait.sendAgain());
    }

    void notASteamGameIsNeverSentAgain() {
        const QTemporaryDir root;
        const QString log = root.filePath(QStringLiteral("console_log.txt"));
        SteamGameWait wait(root.path(), log, root.filePath(QStringLiteral("bootstrap_log.txt")));
        wait.startLaunch({QStringLiteral("tuxpaint")});
        steamLine(log, 5, ownStart());
        wait.stillComing();
        QVERIFY(!wait.sendAgain());
    }
};

} // namespace

QTEST_GUILESS_MAIN(SteamGameWaitTest)
#include "tst_steamgamewait.moc"
