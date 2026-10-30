// SPDX-License-Identifier: Apache-2.0
#include "SteamAtLogin.h"
#include "SteamRestart.h"

#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

namespace {

// Long enough for a few of SteamRestart's checks a second apart.
constexpr int answerMilliseconds = 10000;

// Stand-ins for steam, systemctl and systemd-run in a folder of the test's
// own, first on PATH while this lives. systemctl prints the first line of
// `states` each time it is asked, and keeps the last; steam writes down how
// it was run and copies `after-shutdown` into `states` when told to shut
// down; systemd-run writes down what it was asked and makes the service
// active.
class StandIns {
public:
    StandIns() : m_path(qgetenv("PATH")) {
        standIn(QStringLiteral("steam"), "printf '%s\\n' \"$*\" >> runs\n"
                                         "if [ \"$1\" = -shutdown ]; then\n"
                                         "    cp after-shutdown states\n"
                                         "fi\n"
                                         "exit 0\n");
        standIn(QStringLiteral("systemctl"),
                "printf '%s\\n' \"$*\" >> checks\n"
                "state=$(head -n 1 states)\n"
                "if [ \"$(wc -l < states)\" -gt 1 ]; then\n"
                "    tail -n +2 states > states.next && mv states.next states\n"
                "fi\n"
                "printf '%s\\n' \"$state\"\n"
                "[ \"$state\" = active ]\n");
        startsWith(0);
        qputenv("PATH", m_dir.path().toUtf8() + ':' + m_path);
    }

    ~StandIns() {
        qputenv("PATH", m_path);
    }

    // systemd-run's stand-in ends with `code`.
    void startsWith(int code) const {
        standIn(QStringLiteral("systemd-run"), "printf '%s\\n' \"$@\" > started\n"
                                               "echo active > states\n"
                                               "exit " +
                                                   QByteArray::number(code) + '\n');
    }

    void write(const QString& name, const QByteArray& text) const {
        QFile file(m_dir.filePath(name));
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(text);
    }

    // The lines a stand-in wrote to the file `name`, or none.
    QStringList lines(const QString& name) const {
        QFile file(m_dir.filePath(name));
        if (!file.open(QIODevice::ReadOnly)) {
            return {};
        }
        return QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    }

    bool wrote(const QString& name) const {
        return QFileInfo::exists(m_dir.filePath(name));
    }

private:
    // A stand-in program called `name`, run in the test's folder.
    void standIn(const QString& name, const QByteArray& body) const {
        QFile file(m_dir.filePath(name));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("#!/bin/sh\ncd '" + m_dir.path().toUtf8() + "' || exit 1\n" + body);
        file.close();
        QVERIFY(file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
    }

    QTemporaryDir m_dir;
    QByteArray m_path;
};

// Stopping the child's Steam and starting it again, against the stand-ins
// (#206).
class SteamRestartTest : public QObject {
    Q_OBJECT

private slots:
    void theCommandsAreSystemdsAndSteamsOwn() {
        QCOMPARE(SteamRestart::checkCommand(),
                 (QStringList{QStringLiteral("systemctl"), QStringLiteral("--user"),
                              QStringLiteral("is-active"), QStringLiteral("cairn-steam")}));
        QCOMPARE(SteamRestart::stopCommand(),
                 (QStringList{QStringLiteral("steam"), QStringLiteral("-shutdown")}));
    }

    // As by hand on the test VM (2026-10-30): told to shut down, the service
    // ends, and Steam starts again with the command from the login.
    void steamTheLauncherStartedStopsAndStartsAgain() {
        const StandIns standIns;
        standIns.write(QStringLiteral("states"), "active\n");
        standIns.write(QStringLiteral("after-shutdown"), "inactive\n");
        SteamRestart restart;
        const QSignalSpy finished(&restart, &SteamRestart::finished);
        QTest::ignoreMessage(QtInfoMsg, "Steam stopped and was started again.");
        restart.start();
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, answerMilliseconds);
        QCOMPARE(finished.at(0).at(0).toBool(), true);
        QCOMPARE(standIns.lines(QStringLiteral("runs")), QStringList{QStringLiteral("-shutdown")});
        QCOMPARE(standIns.lines(QStringLiteral("started")), SteamAtLogin::command().mid(1));
        QCOMPARE(standIns.lines(QStringLiteral("checks")).first(),
                 QStringLiteral("--user is-active cairn-steam"));
    }

    // Asked to shut down when it is not running, Steam starts: a Steam that
    // is not the launcher's service is never asked.
    void aSteamTheLauncherDidNotStartIsLeftAlone() {
        const StandIns standIns;
        standIns.write(QStringLiteral("states"), "inactive\n");
        SteamRestart restart;
        const QSignalSpy finished(&restart, &SteamRestart::finished);
        QTest::ignoreMessage(
            QtWarningMsg,
            "Steam is not running as the launcher started it, so it was not started again.");
        restart.start();
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, answerMilliseconds);
        QCOMPARE(finished.at(0).at(0).toBool(), false);
        QVERIFY(!standIns.wrote(QStringLiteral("runs")));
        QVERIFY(!standIns.wrote(QStringLiteral("started")));
    }

    // A Steam that will not stop is left for logging out to end.
    void aSteamThatDoesNotStopIsNotStartedAgain() {
        const StandIns standIns;
        standIns.write(QStringLiteral("states"), "active\n");
        standIns.write(QStringLiteral("after-shutdown"), "active\n");
        SteamRestart restart;
        restart.setStopLimitMilliseconds(1500);
        const QSignalSpy finished(&restart, &SteamRestart::finished);
        QTest::ignoreMessage(QtWarningMsg,
                             "Steam did not stop when asked, so it was not started again.");
        restart.start();
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, answerMilliseconds);
        QCOMPARE(finished.at(0).at(0).toBool(), false);
        QCOMPARE(standIns.lines(QStringLiteral("runs")), QStringList{QStringLiteral("-shutdown")});
        QVERIFY(!standIns.wrote(QStringLiteral("started")));
    }

    // Steam is started again only once its service has ended.
    void aServiceStillStoppingIsWaitedFor() {
        const StandIns standIns;
        standIns.write(QStringLiteral("states"), "active\n");
        standIns.write(QStringLiteral("after-shutdown"), "deactivating\ndeactivating\ninactive\n");
        SteamRestart restart;
        const QSignalSpy finished(&restart, &SteamRestart::finished);
        QTest::ignoreMessage(QtInfoMsg, "Steam stopped and was started again.");
        restart.start();
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, answerMilliseconds);
        QCOMPARE(finished.at(0).at(0).toBool(), true);
        // Once before it was told, then deactivating twice and inactive.
        QCOMPARE(standIns.lines(QStringLiteral("checks")).size(), 4);
        QCOMPARE(standIns.lines(QStringLiteral("started")), SteamAtLogin::command().mid(1));
    }

    void aSteamThatCannotBeStartedSaysSo() {
        const StandIns standIns;
        standIns.write(QStringLiteral("states"), "active\n");
        standIns.write(QStringLiteral("after-shutdown"), "inactive\n");
        standIns.startsWith(1);
        SteamRestart restart;
        const QSignalSpy finished(&restart, &SteamRestart::finished);
        QTest::ignoreMessage(QtWarningMsg, "Steam could not be started again.");
        restart.start();
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, answerMilliseconds);
        QCOMPARE(finished.at(0).at(0).toBool(), false);
        QCOMPARE(standIns.lines(QStringLiteral("started")), SteamAtLogin::command().mid(1));
    }

    // A restart asked for while one is under way is the same restart.
    void aSecondStartWhileBusyDoesNothingMore() {
        const StandIns standIns;
        standIns.write(QStringLiteral("states"), "active\n");
        standIns.write(QStringLiteral("after-shutdown"), "inactive\n");
        SteamRestart restart;
        const QSignalSpy finished(&restart, &SteamRestart::finished);
        QTest::ignoreMessage(QtInfoMsg, "Steam stopped and was started again.");
        restart.start();
        restart.start();
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, answerMilliseconds);
        QTest::qWait(1500);
        QCOMPARE(finished.count(), 1);
        QCOMPARE(standIns.lines(QStringLiteral("runs")), QStringList{QStringLiteral("-shutdown")});
        // Once before Steam was told, once after.
        QCOMPARE(standIns.lines(QStringLiteral("checks")).size(), 2);
    }
};

} // namespace

QTEST_GUILESS_MAIN(SteamRestartTest)
#include "tst_steamrestart.moc"
