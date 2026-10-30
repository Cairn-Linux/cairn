// SPDX-License-Identifier: Apache-2.0
#include "SteamContentLog.h"

#include <QDateTime>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

namespace {

// How the stuck run below ends: the game taken off Steam's schedule.
QByteArray gameOffSchedule() {
    return "[2026-10-30 17:10:28] AppID 292780 scheduler finished : removed from schedule (result "
           "Suspended, state 0xc) \n";
}

// What a new child's Steam wrote on the test VM on 2026-10-30 after a tap on
// Pajama Sam 2 (292780): it fetched scout (1070560), took the game off its
// schedule, and wrote nothing more.
QByteArray stuckRun() {
    return "[2026-10-30 17:10:08] AppID 292780 scheduler update : Priority First, not played for "
           "7421 seconds, update disabled for 0 seconds\n"
           "[2026-10-30 17:10:08] AppID 1070560 scheduler update : Priority First, not played for "
           "1 seconds, update disabled for 0 seconds\n"
           "[2026-10-30 17:10:08] AppID 1070560 update started : download 0/75258128, store 0/0, "
           "reuse 0/0, delta 0/0, stage 0/223012461 \n"
           "[2026-10-30 17:10:28] AppID 1070560 finished update, 1 mounted depots (BuildID "
           "26115262) : 1070561 (8475493586714137016),\n"
           "[2026-10-30 17:10:28] AppID 1070560 scheduler finished : removed from schedule "
           "(result No Error, state 0xc) \n" +
           gameOffSchedule();
}

// One line of the content log, at a time after the stuck run.
QByteArray later(const QByteArray& says) {
    return "[2026-10-30 17:10:40] " + says + '\n';
}

QDateTime at(const char* time) {
    return QDateTime::fromString(QString::fromLatin1(time), QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

void append(const QString& path, const QByteArray& text) {
    QFile log(path);
    QVERIFY(log.open(QIODevice::WriteOnly | QIODevice::Append));
    log.write(text);
}

// What Steam's content log says about the downloads for a tapped game (#206).
class SteamContentLogTest : public QObject {
    Q_OBJECT

private slots:
    void theStuckRunLeavesTheGameOffTheSchedule() {
        const QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("content_log.txt"));
        SteamContentLog log(path);
        log.startLaunch(QStringLiteral("292780"));
        append(path, stuckRun());
        log.read();
        QCOMPARE(log.offScheduleAt(), at("2026-10-30 17:10:28"));
        // Read again with nothing new: still the same.
        log.read();
        QCOMPARE(log.offScheduleAt(), at("2026-10-30 17:10:28"));
    }

    // Anything more Steam does for the launch after the removal means it
    // went on, whether for the game or a tool it scheduled for it.
    void aLaterLineForTheLaunchMeansSteamWentOn_data() {
        QTest::addColumn<QByteArray>("says");
        QTest::newRow("another tool scheduled")
            << QByteArray("AppID 1391110 scheduler update : Priority First, not played for 1 "
                          "seconds, update disabled for 0 seconds");
        QTest::newRow("a tool's download started")
            << QByteArray("AppID 1070560 update started : download 0/75258128, store 0/0, reuse "
                          "0/0, delta 0/0, stage 0/223012461 ");
        QTest::newRow("a tool's update moved on")
            << QByteArray("AppID 1070560 App update changed : Running Update,");
        QTest::newRow("a tool's update finished")
            << QByteArray("AppID 1070560 finished update, 1 mounted depots (BuildID 26115262) : "
                          "1070561 (8475493586714137016),");
        QTest::newRow("a tool's update cancelled")
            << QByteArray("AppID 1070560 update canceled : Priority (Suspended)");
        QTest::newRow("the game stays on the schedule")
            << QByteArray("AppID 292780 scheduler finished : staying in schedule (result "
                          "Suspended, state 0x40a) ");
        QTest::newRow("the game scheduled again")
            << QByteArray("AppID 292780 scheduler update : Priority Auto Update, not played for 1 "
                          "seconds, update disabled for 0 seconds");
    }

    void aLaterLineForTheLaunchMeansSteamWentOn() {
        QFETCH(const QByteArray, says);
        const QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("content_log.txt"));
        SteamContentLog log(path);
        log.startLaunch(QStringLiteral("292780"));
        append(path, stuckRun() + later(says));
        log.read();
        QVERIFY(!log.offScheduleAt());
    }

    // Lines about other games, or about files, space or speed, say nothing
    // about whether Steam went on with the launch.
    void otherLinesChangeNothing_data() {
        QTest::addColumn<QByteArray>("says");
        QTest::newRow("another game's own update")
            << later("AppID 2471140 scheduler update : Priority Auto Update, not played for 7 "
                     "seconds, update disabled for 0 seconds") +
                   later("AppID 2471140 update started : download 0/1084455136, store 0/0, reuse "
                         "0/0, delta 0/0, stage 0/1084455136 ") +
                   later("AppID 2471140 App update changed : Running Update,Downloading,") +
                   later("AppID 2471140 finished update, 1 mounted depots (BuildID 26303620) : "
                         "2471141 (8612705756445978520),") +
                   later("AppID 2471140 scheduler finished : removed from schedule (result No "
                         "Error, state 0xc) ");
        QTest::newRow("Steam's own ticket")
            << later("AppID 7 failed to update ownership ticket (Access Denied)");
        QTest::newRow("the game's state") << later("AppID 292780 state changed : Fully Installed,");
        QTest::newRow("space set aside") << later("AppID 1070560 preallocated 0 files (0 MB) ");
        QTest::newRow("chunks") << later("Downloading 136 chunks for depot 1070561 "
                                         "(8475493586714137016)");
        QTest::newRow("download rate") << later("Current download rate: 76.510 Mbps");
    }

    void otherLinesChangeNothing() {
        QFETCH(const QByteArray, says);
        const QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("content_log.txt"));
        SteamContentLog log(path);
        log.startLaunch(QStringLiteral("292780"));
        append(path, stuckRun() + says);
        log.read();
        QCOMPARE(log.offScheduleAt(), at("2026-10-30 17:10:28"));
    }

    // The game off the schedule while a tool still downloads is not the end.
    void aDownloadStillRunningIsNotTheEnd() {
        const QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("content_log.txt"));
        SteamContentLog log(path);
        log.startLaunch(QStringLiteral("292780"));
        append(path, later("AppID 1070560 scheduler update : Priority First, not played for 1 "
                           "seconds, update disabled for 0 seconds") +
                         later("AppID 1070560 update started : download 0/75258128, store 0/0, "
                               "reuse 0/0, delta 0/0, stage 0/223012461 ") +
                         gameOffSchedule());
        log.read();
        QVERIFY(!log.offScheduleAt());
    }

    // A shader cache's download never writes `finished update`: it ends with
    // its own scheduler line, as on the dev PC on 2026-10-30.
    void aShaderDownloadEndsWithItsSchedulerLine() {
        const QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("content_log.txt"));
        SteamContentLog log(path);
        log.startLaunch(QStringLiteral("294650"));
        append(path,
               "[2026-10-30 14:01:01] AppID 294650 scheduler update : Priority First, not played "
               "for 1 seconds, update disabled for 0 seconds\n"
               "[2026-10-30 14:01:01] AppID 294650 Shader update changed : Running Update,\n"
               "[2026-10-30 14:01:01] AppID 294650 update started : download 0/21744, store 0/0, "
               "reuse 0/0, delta 0/0, stage 0/25322 \n"
               "[2026-10-30 14:01:01] AppID 294650 Shader update changed : None\n"
               "[2026-10-30 14:01:01] AppID 294650 scheduler finished : staying in schedule "
               "(result No Error, state 0xc) \n"
               "[2026-10-30 14:01:02] AppID 294650 Shader update changed : Running Update,\n"
               "[2026-10-30 14:01:02] AppID 294650 update started : download 0/121815904, store "
               "0/0, reuse 0/0, delta 0/0, stage 0/123041112 \n"
               "[2026-10-30 14:01:14] AppID 294650 Shader update changed : None\n"
               "[2026-10-30 14:01:14] AppID 294650 scheduler finished : removed from schedule "
               "(result No Error, state 0xc) \n");
        log.read();
        QCOMPARE(log.offScheduleAt(), at("2026-10-30 14:01:14"));
    }

    // A tool's shader download has not ended until its scheduler line.
    void aShaderDownloadStillRunningIsNotTheEnd() {
        const QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("content_log.txt"));
        SteamContentLog log(path);
        log.startLaunch(QStringLiteral("292780"));
        append(path, later("AppID 1070560 scheduler update : Priority First, not played for 1 "
                           "seconds, update disabled for 0 seconds") +
                         later("AppID 1070560 update started : download 0/21744, store 0/0, "
                               "reuse 0/0, delta 0/0, stage 0/25322 ") +
                         later("AppID 1070560 Shader update changed : None") + gameOffSchedule());
        log.read();
        QVERIFY(!log.offScheduleAt());
        append(path, later("AppID 1070560 scheduler finished : staying in schedule (result No "
                           "Error, state 0xc) ") +
                         gameOffSchedule());
        log.read();
        QCOMPARE(log.offScheduleAt(), at("2026-10-30 17:10:28"));
    }

    // Only the game's own removal counts.
    void anotherAppsRemovalIsNotTheGames_data() {
        QTest::addColumn<QByteArray>("lastLine");
        QTest::newRow("only the tool's") << QByteArray();
        QTest::newRow("a longer app id")
            << later("AppID 2927801 scheduler finished : removed from schedule (result "
                     "Suspended, state 0xc) ");
        QTest::newRow("a longer app id, scheduled first")
            << later("AppID 2927801 scheduler update : Priority First, not played for 1 seconds, "
                     "update disabled for 0 seconds") +
                   later("AppID 2927801 scheduler finished : removed from schedule (result "
                         "Suspended, state 0xc) ");
    }

    void anotherAppsRemovalIsNotTheGames() {
        QFETCH(const QByteArray, lastLine);
        const QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("content_log.txt"));
        SteamContentLog log(path);
        log.startLaunch(QStringLiteral("292780"));
        const QByteArray run = stuckRun();
        append(path, run.first(run.size() - gameOffSchedule().size()) + lastLine);
        log.read();
        QVERIFY(!log.offScheduleAt());
    }

    // A removal from before the tap, even of the same game, says nothing.
    void onlyLinesAfterTheTapCount() {
        const QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("content_log.txt"));
        append(path, stuckRun());
        SteamContentLog log(path);
        log.startLaunch(QStringLiteral("292780"));
        log.read();
        QVERIFY(!log.offScheduleAt());
        append(path, gameOffSchedule());
        log.read();
        QCOMPARE(log.offScheduleAt(), at("2026-10-30 17:10:28"));
    }

    // Steam may be part-way through a line; it is read once it is whole.
    void aHalfWrittenLineIsReadWhenWhole() {
        const QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("content_log.txt"));
        SteamContentLog log(path);
        log.startLaunch(QStringLiteral("292780"));
        const QByteArray whole = gameOffSchedule();
        append(path, whole.first(40));
        log.read();
        QVERIFY(!log.offScheduleAt());
        append(path, whole.mid(40));
        log.read();
        QCOMPARE(log.offScheduleAt(), at("2026-10-30 17:10:28"));
    }

    // Steam began a new log after the tap: all of it is new.
    void aNewLogIsReadFromTheStart() {
        const QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("content_log.txt"));
        append(path, QByteArray(5000, 'x') + '\n');
        SteamContentLog log(path);
        log.startLaunch(QStringLiteral("292780"));
        QVERIFY(QFile::remove(path));
        append(path, stuckRun());
        log.read();
        QCOMPARE(log.offScheduleAt(), at("2026-10-30 17:10:28"));
    }

    // A new tap, or Steam started again, forgets what came before.
    void eachLaunchStartsAfresh() {
        const QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("content_log.txt"));
        SteamContentLog log(path);
        log.startLaunch(QStringLiteral("292780"));
        append(path, stuckRun());
        log.read();
        QVERIFY(log.offScheduleAt());
        log.startLaunch(QStringLiteral("292780"));
        QVERIFY(!log.offScheduleAt());
        log.read();
        QVERIFY(!log.offScheduleAt());
        // Scout was a tool of the last launch, not of this one.
        append(path, later("AppID 1070560 update started : download 0/75258128, store 0/0, reuse "
                           "0/0, delta 0/0, stage 0/223012461 ") +
                         gameOffSchedule());
        log.read();
        QCOMPARE(log.offScheduleAt(), at("2026-10-30 17:10:28"));
    }

    void noGameSaysNothing() {
        const QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("content_log.txt"));
        SteamContentLog log(path);
        log.startLaunch({});
        append(path, stuckRun());
        log.read();
        QVERIFY(!log.offScheduleAt());
    }

    void theLogIsInTheChildsSteamFolder() {
        QVERIFY(
            SteamContentLog::defaultPath().endsWith(QStringLiteral("/Steam/logs/content_log.txt")));
    }
};

} // namespace

QTEST_GUILESS_MAIN(SteamContentLogTest)
#include "tst_steamcontentlog.moc"
