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
