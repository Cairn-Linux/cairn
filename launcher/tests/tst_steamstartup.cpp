// SPDX-License-Identifier: Apache-2.0
#include "SteamStartup.h"

#include <QDateTime>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

namespace {

using Now = SteamStartup::Now;

// A line as Steam writes it, `secondsAgo` before now.
QByteArray line(qint64 secondsAgo, const QByteArray& says) {
    const QString time = QDateTime::currentDateTime()
                             .addSecs(-secondsAgo)
                             .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
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
        return SteamStartup(bootstrap, console).read(QDateTime::currentDateTime());
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
        QCOMPARE(logs.now(), Now::Starting);
        append(logs.console, line(1, "Waiting for compat in post-logon took: 4.822680s") +
                                 line(1, "System startup time: 16.79 seconds"));
        QCOMPARE(logs.now(), Now::Starting);
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
        QCOMPARE(logs.now(), Now::Starting);
    }

    void theLogIsInTheChildsSteamFolder() {
        QVERIFY(
            SteamStartup::defaultPath().endsWith(QStringLiteral("/Steam/logs/bootstrap_log.txt")));
    }
};

} // namespace

QTEST_GUILESS_MAIN(SteamStartupTest)
#include "tst_steamstartup.moc"
