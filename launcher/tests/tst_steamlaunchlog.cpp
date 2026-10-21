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

void append(const QString& path, const QByteArray& text) {
    QFile log(path);
    QVERIFY(log.open(QIODevice::WriteOnly | QIODevice::Append));
    log.write(text);
}

// What Steam's own log says about a launch (ADR-0036).
class SteamLaunchLogTest : public QObject {
    Q_OBJECT

private slots:
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

    void theLogIsInTheChildsSteamFolder() {
        QVERIFY(
            SteamLaunchLog::defaultPath().endsWith(QStringLiteral("/Steam/logs/console_log.txt")));
    }
};

} // namespace

QTEST_GUILESS_MAIN(SteamLaunchLogTest)
#include "tst_steamlaunchlog.moc"
