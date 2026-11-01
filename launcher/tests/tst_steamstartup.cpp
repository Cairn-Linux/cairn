// SPDX-License-Identifier: Apache-2.0
#include "SteamStartup.h"

#include <QDateTime>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

namespace {

using Now = SteamStartup::Now;

// When the test reads the logs: the evening of the login below. A fixed
// time, so the test reads the same at any hour of the year. No zone's
// clocks change that day; the hours they change in have a test of their
// own (tst_steamlogtime).
QDateTime fixedNow() {
    return {QDate(2026, 10, 20), QTime(21, 40)};
}

// A line as Steam writes it, `secondsAgo` before the fixed now.
QByteArray line(qint64 secondsAgo, const QByteArray& says) {
    const QString time =
        fixedNow().addSecs(-secondsAgo).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    return '[' + time.toLatin1() + "] " + says + '\n';
}

// The client's own start command, which it runs once it has started.
QByteArray ownStart() {
    return "ExecCommandLine: \"'/var/home/child/.local/share/Steam/ubuntu12_32/steam' "
           "'-srt-logger-opened' '-silent'\"";
}

void append(const QString& path, const QByteArray& text) {
    QFile log(path);
    QVERIFY(log.open(QIODevice::WriteOnly | QIODevice::Append));
    log.write(text);
}

// The two logs of one Steam in a folder of the test's own.
struct Logs {
    QTemporaryDir dir;
    QString bootstrap = dir.filePath(QStringLiteral("bootstrap_log.txt"));
    QString console = dir.filePath(QStringLiteral("console_log.txt"));
    Now now() const {
        return SteamStartup(bootstrap, console).read(fixedNow());
    }
};

// Each case is lines the updater and the client wrote on the kids' laptop.
// Where Steam's client is in its start (ADR-0037).
class SteamStartupTest : public QObject {
    Q_OBJECT

private slots:
    // The login of 2026-10-20 21:31, from the update to the client started.
    void anUpdateAtLoginThenTheRestartedClient() {
        const Logs logs;
        append(logs.console, line(7200, ownStart()));
        append(logs.bootstrap,
               line(40, "Startup - updater built Sep  3 2026 01:31:48") +
                   line(40, "Startup - Steam Client launched with: "
                            "'/var/home/child/.local/share/Steam/ubuntu12_32/steam' "
                            "'-srt-logger-opened' '-silent'") +
                   line(40,
                        "uninstalled manifest found in "
                        "/var/home/child/.local/share/Steam/package/steam_client_ubuntu12 (1).") +
                   line(40, "Found pending update") + line(40, "Installing update...") +
                   line(39, "Extracting package..."));
        QCOMPARE(logs.now(), Now::Updating);
        append(logs.bootstrap, line(10, "Installing update...") + line(4, "Cleaning up...") +
                                   line(3, "Update complete, launching Steam..."));
        QCOMPARE(logs.now(), Now::Updating);
        append(logs.bootstrap,
               line(3, "Shutdown") + line(3, "Startup - updater built Oct 13 2026 18:44:25") +
                   line(3, "Verifying installation...") + line(2, "Verification complete"));
        QCOMPARE(logs.now(), Now::Restarting);
        append(logs.console, line(1, "Waiting for compat in post-logon took: 4.822680s") +
                                 line(1, "System startup time: 16.79 seconds"));
        QCOMPARE(logs.now(), Now::Restarting);
        append(logs.console, line(0, ownStart()));
        QCOMPARE(logs.now(), Now::Ready);
    }

    // A client with no update to install is only starting.
    void aLoginWithNoUpdateIsStarting() {
        const Logs logs;
        append(logs.console, line(7200, ownStart()));
        append(logs.bootstrap, line(5, "Startup - updater built Oct 13 2026 18:44:25") +
                                   line(5, "Checking for update on startup") +
                                   line(4, "Download skipped by HTTP 304 Not Modified") +
                                   line(4, "Nothing to do"));
        QCOMPARE(logs.now(), Now::Starting);
    }

    // The first start of a new account downloads the whole client first.
    void aFirstDownloadIsUpdating() {
        const Logs logs;
        append(logs.bootstrap, line(20, "Startup - updater built Jun 24 2026 02:12:51") +
                                   line(20, "Downloading Update...") +
                                   line(12, "Downloading update (22,269 of 496,367 KB)..."));
        QCOMPARE(logs.now(), Now::Updating);
    }

    // While the client runs, the updater downloads the next update in the
    // background; the client is not held up by it.
    void aDownloadWhileTheClientRunsIsReady() {
        const Logs logs;
        append(logs.bootstrap, line(600, "Startup - updater built Sep  3 2026 01:31:48"));
        append(logs.console,
               line(584, "System startup time: 16.79 seconds") + line(583, ownStart()));
        append(logs.bootstrap,
               line(480, "Background update loop checking for update. . .") +
                   line(479, "Downloaded new manifest: /steam_client_ubuntu12 version "
                             "1791918634, installed version 1788652215, existing pending "
                             "version 0") +
                   line(470, "Downloading update (22,269 of 496,367 KB)...") +
                   line(380, "Download complete.") +
                   line(60,
                        "uninstalled manifest found in "
                        "/var/home/child/.local/share/Steam/package/steam_client_ubuntu12 (1)."));
        QCOMPARE(logs.now(), Now::Ready);
    }

    // A start that never finished, long ago, was stopped or crashed.
    void aStartLongAgoThatNeverFinishedIsReady() {
        const Logs logs;
        append(logs.bootstrap,
               line(qint64{21} * 60, "Startup - updater built Oct 13 2026 18:44:25") +
                   line(qint64{21} * 60, "Found pending update"));
        QCOMPARE(logs.now(), Now::Ready);
    }

    // The dev PC's Steam on 2026-10-27, 23:05 to 23:08: it installed an
    // update it had downloaded, restarted to check itself, was started again
    // by its start script, downloaded more, installed a second update of
    // 424,455 KB and restarted twice more. Its updater's windows are up all
    // the while (#183); the kids' laptop did the same at a login that day.
    void twoUpdatesInARowAreUpdatingUntilTheClientStarts() {
        const Logs logs;
        append(logs.console, line(7200, ownStart()));
        append(logs.bootstrap,
               line(160, "Startup - updater built Sep  3 2026 01:31:48") +
                   line(160, "Startup - Steam Client launched with: "
                             "'/var/home/child/.local/share/Steam/ubuntu12_32/steam' "
                             "'-srt-logger-opened'") +
                   line(160, "Found pending update") + line(160, "Installing update...") +
                   line(160, "Extracting package...") + line(150, "Installing update...") +
                   line(149, "Cleaning up..."));
        QCOMPARE(logs.now(), Now::Updating);
        QVERIFY(SteamStartup(logs.bootstrap, logs.console).updatingItself(fixedNow()));
        append(logs.bootstrap,
               line(149, "Update complete, launching Steam...") + line(149, "Shutdown") +
                   line(149, "Startup - updater built Sep  3 2026 01:31:48") +
                   line(149, "Verifying installation...") + line(149, "Verifying file sizes only") +
                   line(142, "Verification complete"));
        QCOMPARE(logs.now(), Now::Restarting);
        // Started again by Steam's start script, 18 s later, with no Shutdown.
        append(logs.bootstrap,
               line(131, "Startup - updater built Sep  3 2026 01:31:48") +
                   line(131, "Checking for update on startup") +
                   line(131, "Checking for available updates...") +
                   line(131, "Downloading manifest: "
                             "https://client-update.akamai.steamstatic.com/steam_client_ubuntu12"));
        QCOMPARE(logs.now(), Now::Restarting);
        append(logs.bootstrap,
               line(130, "Downloaded new manifest: /steam_client_ubuntu12 version 1792618285, "
                         "installed version 1788652215, existing pending version 0") +
                   line(130, "Package file strings_en_all.zip.e716a67d7fa91ba70490a6417e25d45c"
                             "5435e393 missing or incorrect size"));
        QCOMPARE(logs.now(), Now::Updating);
        append(logs.bootstrap, line(77, "Found pending update") + line(77, "Installing update...") +
                                   line(60, "Update complete, launching Steam...") +
                                   line(60, "Shutdown") +
                                   line(55, "Startup - updater built Oct 21 2026 21:09:45") +
                                   line(55, "Verifying installation..."));
        QCOMPARE(logs.now(), Now::Restarting);
        append(logs.bootstrap,
               line(55, "Downloading update...") + line(55, "Checking for available updates...") +
                   line(35, "Downloading update (416,268 of 424,455 KB)...") +
                   line(34, "Download complete.") + line(34, "Extracting package..."));
        QCOMPARE(logs.now(), Now::Updating);
        append(logs.bootstrap, line(17, "Update complete, launching Steam...") +
                                   line(17, "Shutdown") +
                                   line(17, "Startup - updater built Oct 21 2026 21:09:45") +
                                   line(17, "Verification complete"));
        QCOMPARE(logs.now(), Now::Restarting);
        append(logs.bootstrap, line(0, "Startup - updater built Oct 21 2026 21:09:45") +
                                   line(0, "Checking for update on startup") +
                                   line(0, "Download skipped: /steam_client_steamrt64 version "
                                           "1792618285, installed version 1792618285, existing "
                                           "pending version 0") +
                                   line(0, "Nothing to do"));
        QCOMPARE(logs.now(), Now::Restarting);
        append(logs.console, line(0, ownStart()));
        QCOMPARE(logs.now(), Now::Ready);
        QVERIFY(!SteamStartup(logs.bootstrap, logs.console).updatingItself(fixedNow()));
    }

    // A start a few minutes after an update is a start of its own: the dev
    // PC's Steam was started again at 23:12, four minutes after its last
    // update, and updated nothing.
    void aStartMinutesAfterAnUpdateIsOnlyStarting() {
        const Logs logs;
        append(logs.console, line(7200, ownStart()));
        append(logs.bootstrap,
               line(250, "Update complete, launching Steam...") + line(250, "Shutdown") +
                   line(250, "Startup - updater built Oct 21 2026 21:09:45") +
                   line(233, "Startup - updater built Oct 21 2026 21:09:45") +
                   line(233, "Nothing to do") +
                   line(5, "Startup - updater built Oct 21 2026 21:09:45") +
                   line(5, "Checking for update on startup") + line(5, "Nothing to do"));
        QCOMPARE(logs.now(), Now::Starting);
        QVERIFY(!SteamStartup(logs.bootstrap, logs.console).updatingItself(fixedNow()));
    }

    void noLogsIsReady() {
        const Logs logs;
        QCOMPARE(logs.now(), Now::Ready);
    }

    // Only the last part of a long log is read; its first, cut line is not
    // taken for a line of its own.
    void aLongLogIsReadFromItsLastPart() {
        const Logs logs;
        append(logs.bootstrap, line(30, "Startup - updater built Oct 13 2026 18:44:25"));
        QByteArray filler;
        for (int i = 0; i < 4000; ++i) {
            filler += line(20, "Downloading update (22,269 of 496,367 KB)...");
        }
        append(logs.bootstrap, filler + line(2, "Update complete, launching Steam...") +
                                   line(2, "Shutdown") +
                                   line(1, "Startup - updater built Oct 13 2026 18:44:25"));
        QCOMPARE(logs.now(), Now::Restarting);
    }

    void theLogIsInTheChildsSteamFolder() {
        QVERIFY(
            SteamStartup::defaultPath().endsWith(QStringLiteral("/Steam/logs/bootstrap_log.txt")));
    }
};

} // namespace

QTEST_GUILESS_MAIN(SteamStartupTest)
#include "tst_steamstartup.moc"
