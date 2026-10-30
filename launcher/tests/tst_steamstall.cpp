// SPDX-License-Identifier: Apache-2.0
#include "SteamLaunchLog.h"
#include "SteamStall.h"

#include <QDateTime>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

namespace {

using Now = SteamStall::Now;

QString game() {
    return QStringLiteral("292780");
}

QDateTime at(const char* time) {
    return QDateTime::fromString(QString::fromLatin1(time), QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

void append(const QString& path, const QByteArray& text) {
    QFile log(path);
    QVERIFY(log.open(QIODevice::WriteOnly | QIODevice::Append));
    log.write(text);
}

// A launch step as Steam writes it for Pajama Sam 2 at `time`.
QByteArray step(const char* time, const QByteArray& says) {
    return '[' + QByteArray(time) + "] GameAction [AppID 292780, ActionID 1] : LaunchApp " + says +
           '\n';
}

// Steam taking the tap at `time`.
QByteArray tapTaken(const char* time) {
    return '[' + QByteArray(time) +
           "] ExecCommandLine: \"'~/.steam/root/ubuntu12_32/steam' '-applaunch' '292780'\"\n";
}

// A client's own start command at `time`, as the login's `steam -silent`
// runs it.
QByteArray ownStart(const char* time) {
    return '[' + QByteArray(time) +
           "] ExecCommandLine: \"'/var/home/child/.local/share/Steam/ubuntu12_32/steam' "
           "'-srt-logger-opened' '-silent'\"\n";
}

// The game taken off Steam's schedule at `time`.
QByteArray offSchedule(const char* time) {
    return '[' + QByteArray(time) +
           "] AppID 292780 scheduler finished : removed from schedule (result Suspended, state "
           "0xc) \n";
}

// What the launch log said on the test VM's stuck run (2026-10-30).
QByteArray stuckLaunch() {
    return tapTaken("2026-10-30 17:10:06") +
           step("2026-10-30 17:10:06", "changed task to CheckShaderDepotManifest with \"\"") +
           step("2026-10-30 17:10:08", "changed task to DownloadingDepots with \"\"") +
           step("2026-10-30 17:10:08", "changed task to DownloadingDepots with \"\"");
}

// What the content log said on the same run: scout fetched, the game taken
// off the schedule, and nothing more.
QByteArray stuckContent() {
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
           offSchedule("2026-10-30 17:10:28");
}

// Steam's two logs for one launch, in a folder of the test's own, and what
// reads them.
class Launch {
public:
    // A tap on Pajama Sam 2: both logs count from now.
    void tap() {
        launch.startLaunch(game());
        stall.startLaunch(game());
    }

    void console(const QByteArray& lines) const {
        append(dir.filePath(QStringLiteral("console_log.txt")), lines);
    }

    void content(const QByteArray& lines) const {
        append(dir.filePath(QStringLiteral("content_log.txt")), lines);
    }

    // Reads what the launch log gained, as SteamGameWait does just before it
    // asks, and asks where the launch is at `now`.
    Now read(const QDateTime& now, SteamStartup::Now steam = SteamStartup::Now::Ready) {
        launch.read();
        return stall.read(launch, steam, now);
    }

    // The stuck run, the restart it asked for, and Steam running again. The
    // launch log counts from then, as SteamGameWait has it.
    void restartedAfterAStall() {
        tap();
        console(stuckLaunch());
        content(stuckContent());
        QCOMPARE(read(at("2026-10-30 17:12:00")), Now::Stalled);
        QVERIFY(stall.askRestart());
        QVERIFY(stall.restarted(true));
        launch.startLaunch(game());
    }

    QTemporaryDir dir;
    SteamLaunchLog launch{dir.filePath(QStringLiteral("console_log.txt"))};
    SteamStall stall{dir.filePath(QStringLiteral("content_log.txt"))};
};

// Whether Steam stopped part-way through getting a game ready, and the one
// restart in each tap (#206). Each case reads at a fixed time, so only the
// limit counted from Steam's start again depends on the clock.
class SteamStallTest : public QObject {
    Q_OBJECT

private slots:
    // The stuck run: Steam took the game off its schedule at 17:10:28 and
    // its launch stayed at DownloadingDepots.
    void theStuckRunIsAStallAfterAMinute() {
        Launch run;
        run.tap();
        run.console(stuckLaunch());
        run.content(stuckContent());
        QCOMPARE(run.read(at("2026-10-30 17:11:27")), Now::Fine);
        QVERIFY(!run.stall.logOutWouldHelp());
        QCOMPARE(run.read(at("2026-10-30 17:11:28")), Now::Stalled);
        QVERIFY(run.stall.logOutWouldHelp());
    }

    // The working run: the launch went on in the same second as the removal.
    void theWorkingRunIsNotAStall() {
        Launch run;
        run.tap();
        run.console(tapTaken("2026-10-30 17:13:00") +
                    step("2026-10-30 17:13:00", "changed task to DownloadingDepots with \"\""));
        run.content("[2026-10-30 17:13:00] AppID 1391110 scheduler update : Priority First, not "
                    "played for 1 seconds, update disabled for 0 seconds\n"
                    "[2026-10-30 17:13:00] AppID 1391110 update started : download 0/192183760, "
                    "store 0/0, reuse 0/0, delta 0/0, stage 0/677401998 \n"
                    "[2026-10-30 17:13:41] AppID 1391110 finished update, 1 mounted depots : "
                    "1391111,\n"
                    "[2026-10-30 17:13:41] AppID 1391110 scheduler finished : removed from "
                    "schedule (result No Error, state 0xc) \n" +
                    offSchedule("2026-10-30 17:13:41"));
        // Read in that very second, before the next step: not a minute yet.
        QCOMPARE(run.read(at("2026-10-30 17:13:41")), Now::Fine);
        run.console(step("2026-10-30 17:13:41", "changed task to RunningInstallScript with \"\""));
        QCOMPARE(run.read(at("2026-10-30 17:20:00")), Now::Fine);
        QVERIFY(!run.stall.logOutWouldHelp());
    }

    // Only a launch still downloading has stopped: a question, a refusal, the
    // game's own process, or no word at all are something else.
    void aLaunchThatIsNotDownloadingIsNotAStall_data() {
        QTest::addColumn<QByteArray>("lastStep");
        QTest::newRow("question") << step("2026-10-30 17:10:30",
                                          "waiting for user response to SynchronizingCloud "
                                          "\"syncfailed\"");
        QTest::newRow("refusal") << step("2026-10-30 17:10:30",
                                         "failed with AppError_33 with \"\"");
        QTest::newRow("the game started")
            << step("2026-10-30 17:10:30", "changed task to Completed with \"\"");
        QTest::newRow("no word since the tap") << QByteArray();
    }

    void aLaunchThatIsNotDownloadingIsNotAStall() {
        QFETCH(const QByteArray, lastStep);
        Launch run;
        run.tap();
        if (!lastStep.isEmpty()) {
            run.console(stuckLaunch() + lastStep);
        }
        run.content(stuckContent());
        QCOMPARE(run.read(at("2026-10-30 17:20:00")), Now::Fine);
    }

    void theRestartIsAskedOncePerTap() {
        Launch run;
        run.tap();
        run.console(stuckLaunch());
        run.content(stuckContent());
        QVERIFY(!run.stall.askRestart());
        QCOMPARE(run.read(at("2026-10-30 17:12:00")), Now::Stalled);
        QVERIFY(run.stall.askRestart());
        QVERIFY(!run.stall.askRestart());
        QCOMPARE(run.read(at("2026-10-30 17:12:15")), Now::Restarting);
        QVERIFY(!run.stall.askRestart());

        // A new tap that stalls again may start Steam again.
        run.tap();
        run.console(stuckLaunch());
        run.content(stuckContent());
        QCOMPARE(run.read(at("2026-10-30 17:12:30")), Now::Stalled);
        QVERIFY(run.stall.askRestart());
    }

    // A Steam started again that never takes the tap does not keep the child
    // waiting for ever. One that takes it is fine.
    void steamStartedAgainHasALimitToTakeTheTap() {
        Launch run;
        run.stall.setRetakeLimitMilliseconds(200);
        run.restartedAfterAStall();
        QCOMPARE(run.read(QDateTime::currentDateTime()), Now::Restarting);
        QTRY_COMPARE(run.read(QDateTime::currentDateTime()), Now::Stuck);
        QVERIFY(run.stall.logOutWouldHelp());

        run.restartedAfterAStall();
        run.console(tapTaken("2026-10-30 17:13:00"));
        QCOMPARE(run.read(at("2026-10-30 17:20:00")), Now::Fine);
    }

    // A Steam started again may update itself first: that is bounded by the
    // work limit, not by the time it gets to take the tap.
    void aSteamStartingOrUpdatingAgainIsWaitedFor() {
        Launch run;
        run.stall.setRetakeLimitMilliseconds(200);
        run.restartedAfterAStall();
        QTest::qWait(300);
        const QDateTime now = QDateTime::currentDateTime();
        QCOMPARE(run.read(now, SteamStartup::Now::Updating), Now::Restarting);
        QCOMPARE(run.read(now, SteamStartup::Now::Restarting), Now::Restarting);
        QCOMPARE(run.read(now, SteamStartup::Now::Starting), Now::Restarting);
        // Started by now and never ran its own start command: it never will.
        QCOMPARE(run.read(now), Now::Stuck);
    }

    // Once the new client has run its own start command, it gets the limit
    // from that line.
    void theLimitCountsFromTheNewClientsStart() {
        Launch run;
        run.restartedAfterAStall();
        run.console(ownStart("2026-10-30 17:13:00"));
        QCOMPARE(run.read(at("2026-10-30 17:14:59")), Now::Restarting);
        QCOMPARE(run.read(at("2026-10-30 17:15:00")), Now::Stuck);
    }

    // The restarted Steam took the tap and stopped the same way again: there
    // is no second restart in a tap.
    void aStallAfterTheRestartIsStuck() {
        Launch run;
        run.restartedAfterAStall();
        run.console(ownStart("2026-10-30 17:13:00") + tapTaken("2026-10-30 17:13:03") +
                    step("2026-10-30 17:13:03", "changed task to DownloadingDepots with \"\""));
        run.content(offSchedule("2026-10-30 17:13:30"));
        QCOMPARE(run.read(at("2026-10-30 17:14:00")), Now::Fine);
        QCOMPARE(run.read(at("2026-10-30 17:14:30")), Now::Stuck);
        QVERIFY(!run.stall.askRestart());
        QVERIFY(run.stall.logOutWouldHelp());
    }

    void aRestartThatFailedIsStuck() {
        Launch run;
        run.tap();
        run.console(stuckLaunch());
        run.content(stuckContent());
        QCOMPARE(run.read(at("2026-10-30 17:12:00")), Now::Stalled);
        QVERIFY(run.stall.askRestart());
        QVERIFY(run.stall.restarted(false));
        QCOMPARE(run.read(at("2026-10-30 17:12:15")), Now::Stuck);
        QVERIFY(run.stall.logOutWouldHelp());
    }

    // Only a restart this tap asked for counts.
    void aRestartNobodyAskedForChangesNothing() {
        Launch run;
        run.tap();
        QVERIFY(!run.stall.restarted(true));
        QCOMPARE(run.read(at("2026-10-30 17:12:00")), Now::Fine);

        // One asked for by the last tap, ending after a new tap.
        run.console(stuckLaunch());
        run.content(stuckContent());
        QCOMPARE(run.read(at("2026-10-30 17:12:00")), Now::Stalled);
        QVERIFY(run.stall.askRestart());
        run.tap();
        QVERIFY(!run.stall.restarted(true));
        QVERIFY(!run.stall.restarted(false));
        QCOMPARE(run.read(at("2026-10-30 17:20:00")), Now::Fine);
    }

    // The old client's removal, before Steam was started again, does not
    // make a second stall.
    void theContentLogCountsFromTheRestart() {
        Launch run;
        run.restartedAfterAStall();
        run.console(ownStart("2026-10-30 17:13:00") + tapTaken("2026-10-30 17:13:03") +
                    step("2026-10-30 17:13:03", "changed task to DownloadingDepots with \"\""));
        QCOMPARE(run.read(at("2026-10-30 17:20:00")), Now::Fine);
    }

    // Steam keeps a stuck launch and takes a later tap on the game without a
    // word; until it works on that game again, logging out is what helps.
    void aStuckGameKeepsTheLineUntilSteamMovesAgain() {
        Launch run;
        run.tap();
        run.console(stuckLaunch());
        run.content(stuckContent());
        QCOMPARE(run.read(at("2026-10-30 17:12:00")), Now::Stalled);
        QVERIFY(run.stall.askRestart());
        QVERIFY(run.stall.restarted(false));
        QCOMPARE(run.read(at("2026-10-30 17:12:15")), Now::Stuck);

        run.launch.startLaunch(QStringLiteral("294650"));
        run.stall.startLaunch(QStringLiteral("294650"));
        QVERIFY(!run.stall.logOutWouldHelp());
        run.stall.startLaunch({});
        QVERIFY(!run.stall.logOutWouldHelp());
        QCOMPARE(run.read(at("2026-10-30 17:13:00")), Now::Fine);

        run.tap();
        QVERIFY(run.stall.logOutWouldHelp());
        run.console(tapTaken("2026-10-30 17:14:00"));
        QCOMPARE(run.read(at("2026-10-30 17:14:15")), Now::Fine);
        QVERIFY(run.stall.logOutWouldHelp());
        run.console(step("2026-10-30 17:20:00", "changed task to RunningInstallScript with \"\""));
        QCOMPARE(run.read(at("2026-10-30 17:20:15")), Now::Fine);
        QVERIFY(!run.stall.logOutWouldHelp());
    }

    void journalNoteSaysWhatToDo() {
        Launch run;
        run.tap();
        QCOMPARE(run.stall.journalNote(QStringLiteral("Pajama Sam 2")), QString());
        run.console(stuckLaunch());
        run.content(stuckContent());
        run.read(at("2026-10-30 17:12:00"));
        QCOMPARE(run.stall.journalNote(QStringLiteral("Pajama Sam 2")),
                 QStringLiteral("Steam stopped getting Pajama Sam 2 ready, so Pajama Sam 2 did not "
                                "start. Log the child out and back in, then try it again."));
    }

    void notASteamGameIsNeverStalled() {
        Launch run;
        run.launch.startLaunch({});
        run.stall.startLaunch({});
        run.console(stuckLaunch());
        run.content(stuckContent());
        QCOMPARE(run.read(at("2026-10-30 17:20:00")), Now::Fine);
        QVERIFY(!run.stall.askRestart());
        QVERIFY(!run.stall.logOutWouldHelp());
    }
};

} // namespace

QTEST_GUILESS_MAIN(SteamStallTest)
#include "tst_steamstall.moc"
