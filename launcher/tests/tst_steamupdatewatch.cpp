// SPDX-License-Identifier: Apache-2.0
#include "SteamUpdateWatch.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

namespace {

// When most cases read the logs: an evening of the dev PC's 2026-10-27
// update. A fixed time, so a case reads the same at any hour of the year.
QDateTime fixedNow() {
    return {QDate(2026, 10, 27), QTime(23, 10)};
}

// The child's home data folder for these tests, so Steam's logs are read
// from a folder of the test's, never from the Steam of whoever runs them.
const QTemporaryDir& dataHome() {
    static const QTemporaryDir home;
    return home;
}

// A line as Steam writes it in one of its logs, at `when`.
void steamLogs(const QString& name, const QDateTime& when, const QByteArray& says) {
    QFile log(dataHome().filePath(QStringLiteral("Steam/logs/") + name));
    QVERIFY(log.open(QIODevice::WriteOnly | QIODevice::Append));
    log.write('[' + when.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")).toLatin1() + "] " + says +
              '\n');
}

void updaterSays(const QDateTime& when, const QByteArray& says) {
    steamLogs(QStringLiteral("bootstrap_log.txt"), when, says);
}

// The updater starting the client and finding an update to install first.
void steamStartsUpdating(const QDateTime& when) {
    updaterSays(when, "Startup - updater built Sep  3 2026 01:31:48");
    updaterSays(when, "Found pending update");
    updaterSays(when, "Installing update...");
}

// The client's own start command, which it runs once it has started.
void steamStartsItself(const QDateTime& when) {
    steamLogs(QStringLiteral("console_log.txt"), when,
              "ExecCommandLine: \"'~/.steam/root/ubuntu12_32/steam' '-srt-logger-opened' "
              "'-silent'\"");
}

// A process list of the test's own: one process of this user with the name
// `name`, or none when `name` is empty.
struct Processes {
    QTemporaryDir dir;
    explicit Processes(const QByteArray& name) {
        if (name.isEmpty()) {
            return;
        }
        QDir(dir.path()).mkpath(QStringLiteral("4242"));
        QFile comm(dir.filePath(QStringLiteral("4242/comm")));
        if (comm.open(QIODevice::WriteOnly)) {
            comm.write(name + '\n');
        }
    }
};

// The watch's own timer, which reads every 5 s while it is active.
bool timerRuns(const SteamUpdateWatch& watch) {
    const QTimer* timer = watch.findChild<QTimer*>();
    return timer != nullptr && timer->isActive();
}

int timerInterval(const SteamUpdateWatch& watch) {
    const QTimer* timer = watch.findChild<QTimer*>();
    return timer != nullptr ? timer->interval() : 0;
}

// What the launcher wrote to the journal since the case began.
QStringList& journal() {
    static QStringList lines;
    return lines;
}

QtMessageHandler& testHandler() {
    static QtMessageHandler handler = nullptr;
    return handler;
}

void keepJournal(QtMsgType type, const QMessageLogContext& context, const QString& message) {
    if (type == QtInfoMsg) {
        journal().append(message);
    }
    if (testHandler() != nullptr) {
        testHandler()(type, context, message);
    }
}

class SteamUpdateWatchTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        QVERIFY(dataHome().isValid());
        qputenv("XDG_DATA_HOME", dataHome().path().toUtf8());
        QVERIFY(QDir(dataHome().path()).mkpath(QStringLiteral("Steam/logs")));
        testHandler() = qInstallMessageHandler(keepJournal);
    }

    void cleanupTestCase() {
        qInstallMessageHandler(testHandler());
    }

    void init() {
        for (const QString& name :
             {QStringLiteral("bootstrap_log.txt"), QStringLiteral("console_log.txt")}) {
            QFile::remove(dataHome().filePath(QStringLiteral("Steam/logs/") + name));
        }
        journal().clear();
    }

    void anUpdateWithSteamRunningTurnsItOn_data() {
        QTest::addColumn<bool>("restarting");
        QTest::newRow("installing") << false;
        QTest::newRow("started again after it") << true;
    }

    // Updating, and Restarting right after it, with a `steam` running.
    void anUpdateWithSteamRunningTurnsItOn() {
        QFETCH(const bool, restarting);
        const Processes steam("steam");
        steamStartsUpdating(fixedNow().addSecs(-40));
        if (restarting) {
            updaterSays(fixedNow().addSecs(-3), "Update complete, launching Steam...");
            updaterSays(fixedNow().addSecs(-3), "Shutdown");
            updaterSays(fixedNow().addSecs(-3), "Startup - updater built Oct 13 2026 18:44:25");
            updaterSays(fixedNow().addSecs(-2), "Verifying installation...");
        }
        SteamUpdateWatch watch(this);
        watch.setProcessRoot(steam.dir.path());
        const QSignalSpy changed(&watch, &SteamUpdateWatch::updatingChanged);
        watch.readAt(fixedNow());
        QVERIFY(watch.updating());
        QCOMPARE(changed.count(), 1);
        const QString started =
            restarting ? QStringLiteral("23:09:57") : QStringLiteral("23:09:20");
        QCOMPARE(journal(), QStringList{QStringLiteral("Steam started updating itself at %1 by its "
                                                       "log, so its games wait on the tiles.")
                                            .arg(started)});
        watch.readAt(fixedNow().addSecs(1));
        QCOMPARE(changed.count(), 1);
        QCOMPARE(journal().size(), 1);
    }

    void otherwiseItStaysOff_data() {
        QTest::addColumn<QByteArray>("process");
        QTest::addColumn<QString>("steam");
        QTest::newRow("a plain start") << QByteArray("steam") << QStringLiteral("starting");
        QTest::newRow("started") << QByteArray("steam") << QStringLiteral("started");
        QTest::newRow("a start over 20 minutes ago")
            << QByteArray("steam") << QStringLiteral("long ago");
        QTest::newRow("no steam running") << QByteArray() << QStringLiteral("updating");
        QTest::newRow("only another of Steam's programs")
            << QByteArray("steamwebhelper") << QStringLiteral("updating");
    }

    void otherwiseItStaysOff() {
        QFETCH(const QByteArray, process);
        QFETCH(const QString, steam);
        const Processes processes(process);
        if (steam == QStringLiteral("starting")) {
            updaterSays(fixedNow().addSecs(-5), "Startup - updater built Oct 13 2026 18:44:25");
            updaterSays(fixedNow().addSecs(-5), "Checking for update on startup");
            updaterSays(fixedNow().addSecs(-4), "Nothing to do");
        } else if (steam == QStringLiteral("started")) {
            steamStartsUpdating(fixedNow().addSecs(-80));
            steamStartsItself(fixedNow().addSecs(-5));
        } else if (steam == QStringLiteral("long ago")) {
            steamStartsUpdating(fixedNow().addSecs(qint64{-21} * 60));
        } else {
            steamStartsUpdating(fixedNow().addSecs(-40));
        }
        SteamUpdateWatch watch(this);
        watch.setProcessRoot(processes.dir.path());
        const QSignalSpy changed(&watch, &SteamUpdateWatch::updatingChanged);
        watch.readAt(fixedNow());
        QVERIFY(!watch.updating());
        QCOMPARE(changed.count(), 0);
        QVERIFY(journal().isEmpty());
    }

    void itEndsAtTheClientsStartCommand_data() {
        QTest::addColumn<qint64>("seconds");
        QTest::addColumn<qint64>("readLater");
        QTest::addColumn<QString>("after");
        QTest::newRow("a minute") << qint64{57} << qint64{300} << QStringLiteral("57 seconds");
        QTest::newRow("thirteen minutes")
            << qint64{(13 * 60) + 10} << qint64{300} << QStringLiteral("13 minutes");
        // Paused until the last start is over 20 minutes old: Steam's own
        // start command is still found, so the line is the same.
        QTest::newRow("read again after 30 minutes")
            << qint64{57} << (qint64{30} * 60) << QStringLiteral("57 seconds");
    }

    // The journal's times are Steam's own, from the start it saw the
    // update in to the client's start command.
    void itEndsAtTheClientsStartCommand() {
        QFETCH(const qint64, seconds);
        QFETCH(const qint64, readLater);
        QFETCH(const QString, after);
        const Processes steam("steam");
        const QDateTime start = fixedNow();
        steamStartsUpdating(start);
        SteamUpdateWatch watch(this);
        watch.setProcessRoot(steam.dir.path());
        watch.readAt(start.addSecs(5));
        QVERIFY(watch.updating());
        updaterSays(start.addSecs(seconds - 18), "Update complete, launching Steam...");
        updaterSays(start.addSecs(seconds - 18), "Startup - updater built Oct 21 2026 21:09:45");
        watch.readAt(start.addSecs(seconds - 10));
        QVERIFY(watch.updating());
        steamStartsItself(start.addSecs(seconds));
        // Read a while later, as after a pause: the line keeps Steam's times.
        watch.readAt(start.addSecs(seconds + readLater));
        QVERIFY(!watch.updating());
        QCOMPARE(journal().size(), 2);
        QCOMPARE(journal().last(),
                 QStringLiteral("Steam finished updating itself at %1 by its "
                                "log, after %2. Its games open again.")
                     .arg(start.addSecs(seconds).toString(QStringLiteral("HH:mm:ss")), after));
    }

    void itEndsWhenTheLastStartIsTwentyMinutesOld() {
        const Processes steam("steam");
        steamStartsUpdating(fixedNow());
        SteamUpdateWatch watch(this);
        watch.setProcessRoot(steam.dir.path());
        watch.readAt(fixedNow().addSecs(60));
        QVERIFY(watch.updating());
        watch.readAt(fixedNow().addSecs((qint64{20} * 60) + 1));
        QVERIFY(!watch.updating());
        QCOMPARE(journal().last(),
                 QStringLiteral("Steam's log has shown no start for 20 minutes, so its games open "
                                "again on the tiles. Steam may still be updating."));
    }

    void itEndsWhenSteamStops() {
        const Processes steam("steam");
        const Processes none("");
        steamStartsUpdating(fixedNow());
        SteamUpdateWatch watch(this);
        watch.setProcessRoot(steam.dir.path());
        watch.readAt(fixedNow().addSecs(10));
        QVERIFY(watch.updating());
        watch.setProcessRoot(none.dir.path());
        watch.readAt(fixedNow().addSecs(15));
        QVERIFY(!watch.updating());
        QCOMPARE(journal().last(),
                 QStringLiteral("Steam is not running, so its games open again on the tiles."));
    }

    // Not active: nothing is read and the last answer stays. Becoming
    // active reads at once, and then every 5 s.
    void itReadsOnlyWhileActive() {
        const Processes steam("steam");
        const QDateTime now = QDateTime::currentDateTime();
        steamStartsUpdating(now.addSecs(-30));
        SteamUpdateWatch watch(this);
        watch.setProcessRoot(steam.dir.path());
        QVERIFY(!watch.updating());
        QVERIFY(!timerRuns(watch));
        const QSignalSpy activeChanged(&watch, &SteamUpdateWatch::activeChanged);
        watch.setActive(true);
        QCOMPARE(activeChanged.count(), 1);
        QVERIFY(watch.updating());
        QVERIFY(timerRuns(watch));
        QCOMPARE(timerInterval(watch), 5000);
        watch.setActive(false);
        QVERIFY(!timerRuns(watch));
        steamStartsItself(now);
        const Processes none("");
        watch.setProcessRoot(none.dir.path());
        QVERIFY(watch.updating());
        QCOMPARE(journal().size(), 1);
        watch.setActive(true);
        QVERIFY(!watch.updating());
        QCOMPARE(journal().size(), 2);
        QVERIFY(journal().last().startsWith(QStringLiteral("Steam finished updating itself")));
    }

    // A pause in the middle of an update writes no second "started" line.
    void oneStartedLineForAnUpdateAcrossAPause() {
        const Processes steam("steam");
        steamStartsUpdating(QDateTime::currentDateTime().addSecs(-30));
        SteamUpdateWatch watch(this);
        watch.setProcessRoot(steam.dir.path());
        watch.setActive(true);
        QVERIFY(watch.updating());
        watch.setActive(false);
        watch.setActive(true);
        watch.readAt(QDateTime::currentDateTime());
        QVERIFY(watch.updating());
        QCOMPARE(journal().size(), 1);
        QVERIFY(journal().first().startsWith(QStringLiteral("Steam started updating itself")));
    }

    // Only a game that starts in Steam is held back, and only while Steam
    // is updating itself by a read made at the press.
    void aPressIsHeldBackOnlyForSteamWhileItUpdates() {
        const Processes steam("steam");
        const QStringList game = {QStringLiteral("steam"), QStringLiteral("-applaunch"),
                                  QStringLiteral("292780")};
        const QStringList draw = {QStringLiteral("tuxpaint")};
        SteamUpdateWatch watch(this);
        watch.setProcessRoot(steam.dir.path());
        watch.setActive(true);
        QVERIFY(!watch.holdsBack(QStringLiteral("Pajama Sam 2"), game));
        steamStartsUpdating(QDateTime::currentDateTime().addSecs(-30));
        watch.readAt(QDateTime::currentDateTime());
        QVERIFY(watch.updating());
        QVERIFY(!watch.holdsBack(QStringLiteral("Draw"), draw));
        QVERIFY(watch.holdsBack(QStringLiteral("Pajama Sam 2"), game));
        QVERIFY(watch.holdsBack(QStringLiteral("Pajama Sam 2"), game));
        QCOMPARE(journal().count(QStringLiteral("Pajama Sam 2 did not open: Steam is updating "
                                                "itself.")),
                 1);
        steamStartsItself(QDateTime::currentDateTime());
        QVERIFY(!watch.holdsBack(QStringLiteral("Pajama Sam 2"), game));
        QVERIFY(!watch.updating());
    }
};

} // namespace

QTEST_GUILESS_MAIN(SteamUpdateWatchTest)
#include "tst_steamupdatewatch.moc"
