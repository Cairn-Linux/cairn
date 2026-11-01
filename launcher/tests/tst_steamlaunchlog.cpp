// SPDX-License-Identifier: Apache-2.0
#include "SteamLaunchLog.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

namespace {

using Step = SteamLaunchLog::Step;

// A line as Steam writes it for the game 294650.
QByteArray line(const QByteArray& says, const QByteArray& appId = "294650") {
    return "[2026-10-20 15:30:43] GameAction [AppID " + appId + ", ActionID 1] : LaunchApp " +
           says + '\n';
}

// The command line Steam says it was given, as it writes it.
QByteArray commandLine(const QByteArray& arguments) {
    return "[2026-10-20 21:32:29] ExecCommandLine: \"'/var/home/child/.local/share/Steam/"
           "ubuntu12_32/steam' " +
           arguments + "\"\n";
}

void append(const QString& path, const QByteArray& text) {
    QFile log(path);
    QVERIFY(log.open(QIODevice::WriteOnly | QIODevice::Append));
    log.write(text);
}

// What Steam's own log says about a launch (ADR-0036).
class SteamLaunchLogTest : public QObject {
    Q_OBJECT

private slots:
    void theAppIdComesFromApplaunch_data() {
        QTest::addColumn<QStringList>("exec");
        QTest::addColumn<QString>("appId");
        QTest::newRow("steam") << QStringList{QStringLiteral("steam"), QStringLiteral("-applaunch"),
                                              QStringLiteral("294650")}
                               << QStringLiteral("294650");
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
        QCOMPARE(SteamLaunchLog::appIdOf(exec), appId);
    }

    // Each line is one Steam wrote on the kids' laptop.
    void eachLineSaysAStep_data() {
        QTest::addColumn<QByteArray>("line");
        QTest::addColumn<Step>("step");
        QTest::newRow("downloading")
            << line("changed task to DownloadingDepots with \"\"") << Step::Working;
        QTest::newRow("install script")
            << line("changed task to ProcessingInstallScript with \"\"") << Step::Working;
        QTest::newRow("answered at once")
            << line("continues with user response \"CreatingProcess\"") << Step::Working;
        QTest::newRow("cloud sync failed")
            << line("waiting for user response to SynchronizingCloud \"syncfailed\"")
            << Step::WaitingForAnswer;
        QTest::newRow("refused") << line("failed with AppError_33 with \"\"") << Step::Refused;
        QTest::newRow("failed") << line("changed task to Failed with \"\"") << Step::Refused;
        QTest::newRow("waiting for its window")
            << line("changed task to WaitingGameWindow with \"\"") << Step::GameStarted;
        QTest::newRow("completed")
            << line("changed task to Completed with \"\"") << Step::GameStarted;
        QTest::newRow("another game")
            << line("changed task to DownloadingDepots with \"\"", "70") << Step::Nothing;
        QTest::newRow("a longer app id")
            << line("changed task to DownloadingDepots with \"\"", "2946501") << Step::Nothing;
        QTest::newRow("not a launch step")
            << QByteArray("[2026-10-20 15:31:23] Running install script evaluator for AppID "
                          "294650, 1 step(s)\n")
            << Step::Nothing;
        QTest::newRow("something new") << line("thinks about it") << Step::Nothing;
    }

    void eachLineSaysAStep() {
        QFETCH(const QByteArray, line);
        QFETCH(const Step, step);
        QCOMPARE(SteamLaunchLog::stepOf(line, QStringLiteral("294650")), step);
    }

    // A launch from before the tap, even of the same game, says nothing
    // about this one.
    void onlyLinesAfterTheTapCount() {
        const QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("console_log.txt"));
        append(path, line("changed task to DownloadingDepots with \"\""));
        SteamLaunchLog log(path);
        log.startLaunch(QStringLiteral("294650"));
        QCOMPARE(log.read(), Step::Nothing);
        append(path, line("changed task to SynchronizingCloud with \"\""));
        QCOMPARE(log.read(), Step::Working);
    }

    // The last line about the game decides; lines about others change nothing.
    void theLastLineAboutTheGameDecides() {
        const QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("console_log.txt"));
        SteamLaunchLog log(path);
        log.startLaunch(QStringLiteral("294650"));
        append(path,
               line("changed task to DownloadingDepots with \"\"") +
                   line("changed task to Completed with \"\"", "70") +
                   "[2026-10-20 15:30:44] CAPIJobRequestUserStats - Server response failed 2\n");
        QCOMPARE(log.read(), Step::Working);
        QCOMPARE(log.read(), Step::Working);
        append(path, line("changed task to WaitingGameWindow with \"\"") +
                         line("changed task to Completed with \"\""));
        QCOMPARE(log.read(), Step::GameStarted);
    }

    // A launch that stops while Steam says it is downloading is the stall of
    // #206; the last step about the game says which it is.
    void theLastStepSaysWhetherSteamIsDownloading() {
        const QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("console_log.txt"));
        SteamLaunchLog log(path);
        log.startLaunch(QStringLiteral("294650"));
        QVERIFY(!log.downloading());
        QCOMPARE(log.step(), Step::Nothing);
        append(path, line("changed task to CheckShaderDepotManifest with \"\"") +
                         line("changed task to DownloadingDepots with \"\""));
        log.read();
        QVERIFY(log.downloading());
        QCOMPARE(log.step(), Step::Working);
        // Another game's download says nothing about this one.
        append(path, line("changed task to RunningInstallScript with \"\"") +
                         line("changed task to DownloadingDepots with \"\"", "70"));
        log.read();
        QVERIFY(!log.downloading());
        QCOMPARE(log.step(), Step::Working);
        append(path, line("changed task to DownloadingDepots with \"\""));
        log.read();
        QVERIFY(log.downloading());
        log.startLaunch(QStringLiteral("294650"));
        QVERIFY(!log.downloading());
        QCOMPARE(log.step(), Step::Nothing);
        // A new, smaller log forgets it too.
        append(path, line("changed task to DownloadingDepots with \"\""));
        log.read();
        QVERIFY(log.downloading());
        QVERIFY(QFile::remove(path));
        append(path, "[2026-10-20 15:31:00] System startup time: 16.79 seconds\n");
        log.read();
        QVERIFY(!log.downloading());
        QCOMPARE(log.step(), Step::Nothing);
    }

    // The question Steam asks when it could not check a game's Steam Cloud
    // saves is told apart from its other questions, such as the one before
    // a game's interstitials, as written on the test VM (#211). The last
    // step about the game decides; lines about anything else change nothing.
    void theLastStepSaysWhetherSteamAsksAboutCloudSaves_data() {
        QTest::addColumn<QByteArray>("lines");
        QTest::addColumn<Step>("step");
        QTest::addColumn<bool>("cloud");
        const QByteArray question =
            line("waiting for user response to SynchronizingCloud \"syncfailed\"");
        QTest::newRow("cloud sync failed")
            << line("changed task to SynchronizingCloud with \"\"") + question
            << Step::WaitingForAnswer << true;
        QTest::newRow("then a line about something else")
            << question +
                   "[2026-10-20 15:30:44] CAPIJobRequestUserStats - Server response failed 2\n"
            << Step::WaitingForAnswer << true;
        QTest::newRow("syncing the cloud")
            << line("changed task to SynchronizingCloud with \"\"") << Step::Working << false;
        QTest::newRow("another question")
            << line("changed task to ShowInterstitials with \"\"") +
                   line("waiting for user response to ShowInterstitials \"\"")
            << Step::WaitingForAnswer << false;
        QTest::newRow("answered") << question + line("changed task to SynchronizingStats with \"\"")
                                  << Step::Working << false;
        QTest::newRow("another game's")
            << line("waiting for user response to SynchronizingCloud \"syncfailed\"", "70")
            << Step::Nothing << false;
    }

    void theLastStepSaysWhetherSteamAsksAboutCloudSaves() {
        QFETCH(const QByteArray, lines);
        QFETCH(const Step, step);
        QFETCH(const bool, cloud);
        const QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("console_log.txt"));
        SteamLaunchLog log(path);
        log.startLaunch(QStringLiteral("294650"));
        QVERIFY(!log.cloudSyncFailed());
        append(path, lines);
        QCOMPARE(log.read(), step);
        QCOMPARE(log.cloudSyncFailed(), cloud);
        QCOMPARE(log.read(), step);
        QCOMPARE(log.cloudSyncFailed(), cloud);
    }

    // A new tap forgets the cloud question, and so does a new log.
    void aNewTapOrANewLogForgetsTheCloudQuestion() {
        const QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("console_log.txt"));
        const QByteArray question =
            line("waiting for user response to SynchronizingCloud \"syncfailed\"");
        SteamLaunchLog log(path);
        log.startLaunch(QStringLiteral("294650"));
        append(path, question);
        log.read();
        QVERIFY(log.cloudSyncFailed());
        log.startLaunch(QStringLiteral("294650"));
        QVERIFY(!log.cloudSyncFailed());
        append(path, question);
        log.read();
        QVERIFY(log.cloudSyncFailed());
        QVERIFY(QFile::remove(path));
        append(path, "[2026-10-20 15:31:00] System startup time: 16.79 seconds\n");
        log.read();
        QVERIFY(!log.cloudSyncFailed());
        QCOMPARE(log.step(), Step::Nothing);
    }

    // Steam may be part-way through a line; it is read once it is whole.
    void aHalfWrittenLineIsReadWhenWhole() {
        const QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("console_log.txt"));
        SteamLaunchLog log(path);
        log.startLaunch(QStringLiteral("294650"));
        const QByteArray whole = line("changed task to DownloadingDepots with \"\"");
        append(path, whole.first(40));
        QCOMPARE(log.read(), Step::Nothing);
        append(path, whole.mid(40));
        QCOMPARE(log.read(), Step::Working);
    }

    // Steam began a new log after the tap: all of it is new.
    void aNewLogIsReadFromTheStart() {
        const QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("console_log.txt"));
        append(path, QByteArray(500, 'x') + '\n');
        SteamLaunchLog log(path);
        log.startLaunch(QStringLiteral("294650"));
        QVERIFY(QFile::remove(path));
        append(path, line("changed task to DownloadingDepots with \"\""));
        QCOMPARE(log.read(), Step::Working);
    }

    // No log at the tap, as before Steam's first start: the log that comes
    // later is read from its start.
    void aLogThatComesLaterIsRead() {
        const QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("console_log.txt"));
        SteamLaunchLog log(path);
        log.startLaunch(QStringLiteral("294650"));
        QCOMPARE(log.read(), Step::Nothing);
        append(path, line("changed task to DownloadingDepots with \"\""));
        QCOMPARE(log.read(), Step::Working);
    }

    // A new tap forgets what the last game's launch had got to.
    void eachLaunchStartsAfresh() {
        const QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("console_log.txt"));
        SteamLaunchLog log(path);
        log.startLaunch(QStringLiteral("294650"));
        append(path, line("failed with AppError_33 with \"\""));
        QCOMPARE(log.read(), Step::Refused);
        log.startLaunch(QStringLiteral("294650"));
        QCOMPARE(log.read(), Step::Nothing);
    }

    void noGameSaysNothing() {
        const QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("console_log.txt"));
        append(path, line("changed task to DownloadingDepots with \"\""));
        SteamLaunchLog log(path);
        QCOMPARE(log.read(), Step::Nothing);
        QCOMPARE(SteamLaunchLog::stepOf(line("changed task to DownloadingDepots with \"\""), {}),
                 Step::Nothing);
    }

    // Steam names the tap the moment it takes it (ADR-0037).
    void theTapReachingSteamIsSeen() {
        const QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("console_log.txt"));
        SteamLaunchLog log(path);
        log.startLaunch(QStringLiteral("294650"));
        append(path, commandLine("'-applaunch' '2946501'"));
        log.read();
        QVERIFY(!log.reachedSteam());
        append(path, "[2026-10-20 21:40:13] ExecCommandLine: \"'~/.steam/root/ubuntu12_32/steam' "
                     "'-applaunch' '294650'\"\n");
        log.read();
        QVERIFY(log.reachedSteam());
        QVERIFY(!log.steamStartedAt());
    }

    // A client started after the tap runs its own start command; one that
    // takes no tap with it leaves the tap where it was.
    void aClientStartedAfterTheTapIsSeen() {
        const QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("console_log.txt"));
        append(path, commandLine("'-srt-logger-opened' '-silent'"));
        SteamLaunchLog log(path);
        log.startLaunch(QStringLiteral("294650"));
        log.read();
        QVERIFY(!log.steamStartedAt());
        append(path, commandLine("'-srt-logger-opened' '-silent'"));
        log.read();
        QCOMPARE(log.steamStartedAt(),
                 QDateTime::fromString(QStringLiteral("2026-10-20 21:32:29"),
                                       QStringLiteral("yyyy-MM-dd HH:mm:ss")));
        QVERIFY(!log.reachedSteam());
        log.startLaunch(QStringLiteral("294650"));
        QVERIFY(!log.steamStartedAt());
    }

    // A tap made before any Steam ran starts the client with the game.
    void aClientStartedWithTheGameTookTheTap() {
        const QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("console_log.txt"));
        SteamLaunchLog log(path);
        log.startLaunch(QStringLiteral("294650"));
        append(path, commandLine("'-srt-logger-opened' '-applaunch' '294650'"));
        log.read();
        QVERIFY(log.reachedSteam());
        QVERIFY(!log.steamStartedAt());
    }

    // A client that starts after the tap is a new one, whether it runs its
    // own start command alone, starts with the tap, or begins a new log
    // (#211). The tap's own line, a step, or a start from before the tap is
    // not.
    void aNewClientIsSeen_data() {
        QTest::addColumn<QByteArray>("lines");
        QTest::addColumn<bool>("newLog");
        QTest::addColumn<bool>("started");
        QTest::newRow("its own start")
            << commandLine("'-srt-logger-opened' '-silent'") << false << true;
        QTest::newRow("started with the tap")
            << commandLine("'-srt-logger-opened' '-applaunch' '294650'") << false << true;
        QTest::newRow("a new log")
            << line("changed task to DownloadingDepots with \"\"") << true << true;
        QTest::newRow("the tap alone") << commandLine("'-applaunch' '294650'") << false << false;
        QTest::newRow("a step") << line("changed task to DownloadingDepots with \"\"") << false
                                << false;
    }

    void aNewClientIsSeen() {
        QFETCH(const QByteArray, lines);
        QFETCH(const bool, newLog);
        QFETCH(const bool, started);
        const QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("console_log.txt"));
        append(path, commandLine("'-srt-logger-opened' '-silent'") + QByteArray(500, 'x') + '\n');
        SteamLaunchLog log(path);
        log.startLaunch(QStringLiteral("294650"));
        log.read();
        QVERIFY(!log.clientStarted());
        if (newLog) {
            QVERIFY(QFile::remove(path));
        }
        append(path, lines);
        log.read();
        QCOMPARE(log.clientStarted(), started);
        log.read();
        QCOMPARE(log.clientStarted(), started);
        log.startLaunch(QStringLiteral("294650"));
        QVERIFY(!log.clientStarted());
    }

    void theTimeOfALine_data() {
        QTest::addColumn<QByteArray>("line");
        QTest::addColumn<bool>("valid");
        QTest::newRow("steam") << QByteArray("[2026-10-20 21:32:28] System startup time: 16.79 "
                                             "seconds")
                               << true;
        QTest::newRow("no time") << QByteArray("Running install script evaluator") << false;
        QTest::newRow("not a time") << QByteArray("[2026-13-45 99:00:00] Startup - ") << false;
        QTest::newRow("cut short") << QByteArray("[2026-10-20 21:3") << false;
    }

    void theTimeOfALine() {
        QFETCH(const QByteArray, line);
        QFETCH(const bool, valid);
        QCOMPARE(SteamLaunchLog::timeOf(line).has_value(), valid);
    }

    void theLogIsInTheChildsSteamFolder() {
        QVERIFY(
            SteamLaunchLog::defaultPath().endsWith(QStringLiteral("/Steam/logs/console_log.txt")));
    }
};

} // namespace

QTEST_GUILESS_MAIN(SteamLaunchLogTest)
#include "tst_steamlaunchlog.moc"
