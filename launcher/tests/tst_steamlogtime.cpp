// SPDX-License-Identifier: Apache-2.0
#include "SteamLaunchLog.h"
#include "SteamSignIn.h"
#include "SteamStall.h"
#include "SteamStartup.h"

#include <QDateTime>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>

#include <optional>

namespace {

// A line of Steam's logs written at the local `time`.
QByteArray line(const QByteArray& time) {
    return '[' + time + "] System startup time: 16.79 seconds";
}

// The updater starting the client, at the local `time`.
QByteArray startup(const QByteArray& time) {
    return '[' + time + "] Startup - updater built Oct 13 2026 18:44:25\n";
}

// The client running its own start command, at the local `time`.
QByteArray ownStart(const QByteArray& time) {
    return '[' + time +
           "] ExecCommandLine: \"'/var/home/child/.local/share/Steam/ubuntu12_32/steam' "
           "'-srt-logger-opened' '-silent'\"\n";
}

// A moment, given in UTC so that each row says plainly which one it means.
QDateTime utc(QDate date, QTime time) {
    return {date, time, QTimeZone::UTC};
}

// The look most cases make: 01:30 in the second pass.
QDateTime look() {
    return utc(QDate(2026, 11, 1), QTime(8, 30));
}

void append(const QString& path, const QByteArray& text) {
    QFile log(path);
    QVERIFY(log.open(QIODevice::WriteOnly | QIODevice::Append));
    log.write(text);
}

// The logs of one Steam, in a folder of the test's own.
struct Logs {
    QTemporaryDir dir;
    QString bootstrap = dir.filePath(QStringLiteral("bootstrap_log.txt"));
    QString console = dir.filePath(QStringLiteral("console_log.txt"));
    QString content = dir.filePath(QStringLiteral("content_log.txt"));
};

// The moment a time in Steam's logs means, in a zone whose clocks change,
// and what each of the launcher's readers of those logs makes of it (#248).
// CTest runs this program in Mountain time, written as a POSIX rule: MDT is
// UTC-6 and MST is UTC-7. On 2026-11-01 its clocks go back from 02:00 MDT
// to 01:00 MST, so 01:00 to 02:00 comes twice: 01:30 is 07:30 UTC in the
// first pass and 08:30 UTC in the second. On 2026-03-08 they go forward
// from 02:00 MST to 03:00 MDT, so 02:00 to 03:00 never comes.
class SteamLogTimeTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        const QDateTime summer(QDate(2026, 7, 1), QTime(12, 0));
        const QDateTime winter(QDate(2026, 12, 1), QTime(12, 0));
        if (summer.offsetFromUtc() != -6 * 3600 || winter.offsetFromUtc() != -7 * 3600) {
            QSKIP("This test needs TZ=MST7MDT,M3.2.0,M11.1.0, which CTest sets.");
        }
    }

    void theMomentALineMeans_data() {
        QTest::addColumn<QByteArray>("line");
        QTest::addColumn<QDateTime>("now");
        QTest::addColumn<QDateTime>("moment");
        const QDate night(2026, 11, 1);
        QTest::newRow("an ordinary evening")
            << line("2026-10-20 21:32:29") << utc(QDate(2026, 10, 21), QTime(3, 40))
            << utc(QDate(2026, 10, 21), QTime(3, 32, 29));
        QTest::newRow("just before the hour")
            << line("2026-11-01 00:59:59") << look() << utc(night, QTime(6, 59, 59));
        QTest::newRow("the second pass, 20 minutes old")
            << line("2026-11-01 01:10:00") << look() << utc(night, QTime(8, 10));
        QTest::newRow("the second pass, written as it is read")
            << line("2026-11-01 01:30:00") << look() << utc(night, QTime(8, 30));
        // Read as the second pass, it would be 20 minutes after now.
        QTest::newRow("the first pass, 40 minutes old")
            << line("2026-11-01 01:50:00") << look() << utc(night, QTime(7, 50));
        QTest::newRow("the first pass, its last second")
            << line("2026-11-01 01:59:59") << look() << utc(night, QTime(7, 59, 59));
        // Written in the first pass, this line is 70 minutes old; in the
        // second, 10. Nothing in it says which, so it reads as the second.
        QTest::newRow("either pass, 10 or 70 minutes old")
            << line("2026-11-01 01:20:00") << look() << utc(night, QTime(8, 20));
        QTest::newRow("just after the hour, 30 minutes from now")
            << line("2026-11-01 02:00:00") << look() << utc(night, QTime(9, 0));
        // Read at 01:30 in the first pass, both of its moments are still to
        // come, as after the clock was set back.
        QTest::newRow("from the future")
            << line("2026-11-01 01:45:00") << utc(night, QTime(7, 30)) << utc(night, QTime(7, 45));
        // 02:30 that morning is 01:30 MST or 03:30 MDT.
        const QDate spring(2026, 3, 8);
        QTest::newRow("the skipped hour, read at 03:40")
            << line("2026-03-08 02:30:00") << utc(spring, QTime(9, 40))
            << utc(spring, QTime(9, 30));
        QTest::newRow("the skipped hour, read at 03:10")
            << line("2026-03-08 02:30:00") << utc(spring, QTime(9, 10))
            << utc(spring, QTime(8, 30));
    }

    void theMomentALineMeans() {
        QFETCH(const QByteArray, line);
        QFETCH(const QDateTime, now);
        QFETCH(const QDateTime, moment);
        const std::optional<QDateTime> time = SteamLaunchLog::timeOf(line, now);
        QVERIFY(time.has_value());
        QCOMPARE(time.value_or(QDateTime()).toUTC(), moment);
    }

    // Each reader below, at a look in the second pass, is given a line that
    // would be an hour older read as the first pass, or a line from the
    // first pass that would be an hour younger read as the second. Between
    // them, each place a reader reads a time in Steam's logs is checked.

    // Steam took the game off its schedule 30 seconds before the look: no
    // stall yet, so Steam is not started again. A minute after, it is one
    // (#206).
    void aStallIsTimedFromWhenItBegan() {
        const Logs logs;
        SteamLaunchLog launch(logs.console);
        SteamStall stall(logs.content);
        launch.startLaunch(QStringLiteral("292780"));
        stall.startLaunch(QStringLiteral("292780"));
        append(logs.console, "[2026-11-01 01:29:10] GameAction [AppID 292780, ActionID 1] : "
                             "LaunchApp changed task to DownloadingDepots with \"\"\n");
        append(logs.content, "[2026-11-01 01:29:30] AppID 292780 scheduler finished : removed "
                             "from schedule (result Suspended, state 0xc) \n");
        launch.read(look());
        QCOMPARE(stall.read(launch, SteamStartup::Now::Ready, look()), SteamStall::Now::Fine);
        const QDateTime aMinuteOn = look().addSecs(31);
        launch.read(aMinuteOn);
        QCOMPARE(stall.read(launch, SteamStartup::Now::Ready, aMinuteOn), SteamStall::Now::Stalled);
    }

    // A client that started two seconds before the look has just started,
    // so it still has its moment to take the tap (ADR-0037).
    void aClientStartTwoSecondsAgoIsJustNow() {
        const Logs logs;
        SteamLaunchLog launch(logs.console);
        launch.startLaunch(QStringLiteral("292780"));
        append(logs.console, ownStart("2026-11-01 01:29:58"));
        launch.read(look());
        QCOMPARE(launch.steamStartedAt().value_or(QDateTime()).toUTC(),
                 utc(QDate(2026, 11, 1), QTime(8, 29, 58)));
    }

    // The updater started Steam five minutes before the look, and the client
    // has not run its own start command yet: Steam is still starting, not a
    // start from long ago that never finished (ADR-0037).
    void aStartFiveMinutesAgoIsStillStarting() {
        const Logs logs;
        append(logs.bootstrap, startup("2026-11-01 01:25:00"));
        QCOMPARE(SteamStartup(logs.bootstrap, logs.console).read(look()),
                 SteamStartup::Now::Starting);
    }

    // The updater started Steam just before the clocks went back, and the
    // client ran its own start command just after. Read ten minutes after
    // that start, Steam has started: it is not still starting.
    void aStartCommandAcrossTheChangeIsReady() {
        const Logs logs;
        append(logs.bootstrap, startup("2026-11-01 01:59:50"));
        append(logs.console, ownStart("2026-11-01 01:00:05"));
        const QDateTime tenMinutesOn = utc(QDate(2026, 11, 1), QTime(8, 10));
        QCOMPARE(SteamStartup(logs.bootstrap, logs.console).read(tenMinutesOn),
                 SteamStartup::Now::Ready);
    }

    // The updater installed an update of Steam and started it again five
    // seconds later, both in the second pass: Steam is restarting after its
    // update, and the updater's windows are its own (#183).
    void aStartJustAfterAnUpdateIsARestart() {
        const Logs logs;
        append(logs.bootstrap, "[2026-11-01 01:20:00] Update complete, launching Steam...\n" +
                                   startup("2026-11-01 01:20:05"));
        QCOMPARE(SteamStartup(logs.bootstrap, logs.console).read(look()),
                 SteamStartup::Now::Restarting);
    }

    // The updater started Steam just before the clocks went back, and Steam
    // could not reach its servers just after: that is the client that runs,
    // not an earlier one (#182).
    void aSignInAcrossTheChangeIsTheRunningClients() {
        const Logs logs;
        append(logs.bootstrap, startup("2026-11-01 01:59:50"));
        append(logs.console, "[2026-11-01 01:00:10] LogonFailure No Connection\n");
        QCOMPARE(SteamSignIn(logs.console, logs.bootstrap).read(look()),
                 SteamSignIn::Now::NoConnection);
    }

    // The updater started Steam in the second pass, and an earlier client
    // was refused its sign-in in the first: the client that runs was not
    // refused (#182).
    void anEarlierClientsRefusalIsNotTheRunningClients() {
        const Logs logs;
        append(logs.bootstrap, startup("2026-11-01 01:10:00"));
        append(logs.console, "[2026-11-01 01:40:00] LogonFailure Access Denied\n");
        QCOMPARE(SteamSignIn(logs.console, logs.bootstrap).read(look()), SteamSignIn::Now::Fine);
    }
};

} // namespace

QTEST_GUILESS_MAIN(SteamLogTimeTest)
#include "tst_steamlogtime.moc"
