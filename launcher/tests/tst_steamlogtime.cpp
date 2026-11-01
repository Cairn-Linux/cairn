// SPDX-License-Identifier: Apache-2.0
#include "SteamLaunchLog.h"

#include <QDateTime>
#include <QTest>
#include <QTimeZone>

#include <optional>

namespace {

// A line of Steam's logs written at the local `time`.
QByteArray line(const QByteArray& time) {
    return '[' + time + "] System startup time: 16.79 seconds";
}

// A moment, given in UTC so that each row says plainly which one it means.
QDateTime utc(QDate date, QTime time) {
    return {date, time, QTimeZone::UTC};
}

// The moment a time in Steam's logs means, in a zone whose clocks change
// (#248). CTest runs this program in Mountain time, written as a POSIX rule:
// MDT is UTC-6 and MST is UTC-7. On 2026-11-01 its clocks go back from 02:00
// MDT to 01:00 MST, so 01:00 to 02:00 comes twice: 01:30 is 07:30 UTC in the
// first pass and 08:30 UTC in the second. On 2026-03-08 they go forward from
// 02:00 MST to 03:00 MDT, so 02:00 to 03:00 never comes.
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
        // Most rows are read at 01:30 in the second pass.
        const QDateTime secondPass = utc(night, QTime(8, 30));
        QTest::newRow("an ordinary evening")
            << line("2026-10-20 21:32:29") << utc(QDate(2026, 10, 21), QTime(3, 40))
            << utc(QDate(2026, 10, 21), QTime(3, 32, 29));
        QTest::newRow("just before the hour")
            << line("2026-11-01 00:59:59") << secondPass << utc(night, QTime(6, 59, 59));
        QTest::newRow("the second pass, 20 minutes old")
            << line("2026-11-01 01:10:00") << secondPass << utc(night, QTime(8, 10));
        QTest::newRow("the second pass, written as it is read")
            << line("2026-11-01 01:30:00") << secondPass << utc(night, QTime(8, 30));
        // Read as the second pass, it would be 20 minutes after now.
        QTest::newRow("the first pass, 40 minutes old")
            << line("2026-11-01 01:50:00") << secondPass << utc(night, QTime(7, 50));
        QTest::newRow("the first pass, its last second")
            << line("2026-11-01 01:59:59") << secondPass << utc(night, QTime(7, 59, 59));
        // Written in the first pass, this line is 70 minutes old; in the
        // second, 10. Nothing in it says which, so it reads as the second.
        QTest::newRow("either pass, 10 or 70 minutes old")
            << line("2026-11-01 01:20:00") << secondPass << utc(night, QTime(8, 20));
        QTest::newRow("just after the hour, 30 minutes from now")
            << line("2026-11-01 02:00:00") << secondPass << utc(night, QTime(9, 0));
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
};

} // namespace

QTEST_GUILESS_MAIN(SteamLogTimeTest)
#include "tst_steamlogtime.moc"
