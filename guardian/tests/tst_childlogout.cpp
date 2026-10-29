// SPDX-License-Identifier: Apache-2.0
#include "ChildLogOut.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

namespace {

constexpr int kAnswerMilliseconds = 10000;

class ChildLogOutTest : public QObject {
    Q_OBJECT

    // Logs ada out with the stand-ins answering as the scenario says, and
    // returns the arguments cairn-family was given.
    QStringList logOutWith(ChildLogOut& logOut, const QString& scenario) {
        QFile::remove(m_arguments);
        qputenv("CAIRN_TEST_SCENARIO", scenario.toLocal8Bit());
        qputenv("CAIRN_TEST_ARGUMENTS", m_arguments.toLocal8Bit());
        QSignalSpy ended(&logOut, &ChildLogOut::ended);
        logOut.ask("ada", "Ada");
        logOut.confirm();
        const bool answered = !ended.isEmpty() || ended.wait(kAnswerMilliseconds);
        qunsetenv("CAIRN_TEST_ARGUMENTS");
        if (!answered || ended.size() != 1) {
            return {QStringLiteral("no answer")};
        }
        QFile file(m_arguments);
        return file.open(QIODevice::ReadOnly)
                   ? QString::fromUtf8(file.readAll()).split('\n', Qt::SkipEmptyParts)
                   : QStringList();
    }

    QTemporaryDir m_temporary;
    QString m_arguments;
    const QString m_pkexec = QStringLiteral(FIXTURES "/pkexec-family");
    const QString m_command = QStringLiteral(FIXTURES "/cairn-family");

private slots:
    void init() {
        QVERIFY(m_temporary.isValid());
        m_arguments = m_temporary.filePath("arguments");
    }

    void itStartsAskingNothing() {
        const ChildLogOut logOut(m_pkexec, m_command);
        QCOMPARE(logOut.step(), ChildLogOut::Step::NotAsking);
        QVERIFY(!logOut.busy());
        QVERIFY(!logOut.isAbout("ada"));
    }

    // The warning first; nothing runs until the parent confirms.
    void theWarningComesFirst() {
        ChildLogOut logOut(m_pkexec, m_command);
        QFile::remove(m_arguments);
        qputenv("CAIRN_TEST_ARGUMENTS", m_arguments.toLocal8Bit());
        logOut.ask("ada", "Ada");
        QCOMPARE(logOut.step(), ChildLogOut::Step::Warning);
        QCOMPARE(logOut.login(), QStringLiteral("ada"));
        QCOMPARE(logOut.name(), QStringLiteral("Ada"));
        QVERIFY(logOut.isAbout("ada"));
        QVERIFY(!logOut.isAbout("ben"));
        QVERIFY(!logOut.busy());
        logOut.notNow();
        QCOMPARE(logOut.step(), ChildLogOut::Step::NotAsking);
        QCOMPARE(logOut.login(), QString());
        qunsetenv("CAIRN_TEST_ARGUMENTS");
        QVERIFY(!QFile::exists(m_arguments));
        // Confirming with no warning up runs nothing.
        logOut.confirm();
        QCOMPARE(logOut.step(), ChildLogOut::Step::NotAsking);
    }

    // One password, one subcommand, and nothing but a logout.
    void aLogoutRunsLogOutForTheChild() {
        ChildLogOut logOut(m_pkexec, m_command);
        QCOMPARE(logOutWith(logOut, "log-out-done"), QStringList({"run", "log-out", "ada"}));
        QCOMPARE(logOut.step(), ChildLogOut::Step::NotAsking);
        QCOMPARE(logOut.reason(), ChildLogOut::Reason::NoReason);
        QVERIFY(!logOut.isAbout("ada"));
    }

    // The password box, then the logout once the command's first line came.
    void theStepsWhileItRuns() {
        ChildLogOut logOut(m_pkexec, m_command);
        qputenv("CAIRN_TEST_SCENARIO", "log-out-running");
        logOut.ask("ada", "Ada");
        logOut.confirm();
        QCOMPARE(logOut.step(), ChildLogOut::Step::Asking);
        QVERIFY(logOut.busy());
        QTRY_COMPARE_WITH_TIMEOUT(logOut.step(), ChildLogOut::Step::LoggingOut,
                                  kAnswerMilliseconds);
        QVERIFY(logOut.busy());
        // A second ask while it runs changes nothing.
        logOut.ask("ben", "Ben");
        QCOMPARE(logOut.login(), QStringLiteral("ada"));
        logOut.notNow();
        logOut.close();
        QCOMPARE(logOut.step(), ChildLogOut::Step::LoggingOut);
    }

    void notLoggedOut_data() {
        QTest::addColumn<QString>("scenario");
        QTest::addColumn<ChildLogOut::Reason>("reason");
        QTest::newRow("stuck") << "log-out-held-stuck" << ChildLogOut::Reason::Stuck;
        QTest::newRow("still closing")
            << "log-out-held-logged-in" << ChildLogOut::Reason::StillClosing;
        QTest::newRow("stopped") << "log-out-held-stopped" << ChildLogOut::Reason::StoppedEarly;
        QTest::newRow("a visit") << "log-out-held-busy" << ChildLogOut::Reason::VisitOpen;
        QTest::newRow("a copy") << "log-out-held-copying" << ChildLogOut::Reason::CopyRunning;
        QTest::newRow("not set up") << "log-out-held-not-set-up" << ChildLogOut::Reason::NotSetUp;
        QTest::newRow("job not started")
            << "log-out-held-not-started" << ChildLogOut::Reason::NotStarted;
        QTest::newRow("job failed") << "log-out-held-failed" << ChildLogOut::Reason::Unknown;
        QTest::newRow("dismissed") << "dismissed" << ChildLogOut::Reason::Dismissed;
        QTest::newRow("not authorized") << "not-authorized" << ChildLogOut::Reason::NotAuthorized;
        QTest::newRow("not a grown-up")
            << "refused-not-a-grown-up" << ChildLogOut::Reason::NotAGrownUp;
        QTest::newRow("not a child") << "refused-not-a-child" << ChildLogOut::Reason::NotAChild;
        QTest::newRow("usage") << "refused-usage" << ChildLogOut::Reason::Unknown;
        QTest::newRow("garbage") << "garbage" << ChildLogOut::Reason::Unknown;
    }

    // Why the child is not logged out, in one value, until it is closed.
    void notLoggedOut() {
        QFETCH(const QString, scenario);
        QFETCH(const ChildLogOut::Reason, reason);
        ChildLogOut logOut(m_pkexec, m_command);
        logOutWith(logOut, scenario);
        QCOMPARE(logOut.step(), ChildLogOut::Step::NotLoggedOut);
        QCOMPARE(logOut.reason(), reason);
        QVERIFY(logOut.isAbout("ada"));
        QVERIFY(!logOut.busy());
        logOut.close();
        QCOMPARE(logOut.step(), ChildLogOut::Step::NotAsking);
        QCOMPARE(logOut.reason(), ChildLogOut::Reason::NoReason);
    }

    void aMissingCommandRunsNothing() {
        ChildLogOut logOut(m_pkexec, QStringLiteral("/nonexistent/cairn-family"));
        QSignalSpy ended(&logOut, &ChildLogOut::ended);
        logOut.ask("ada", "Ada");
        logOut.confirm();
        QVERIFY(!ended.isEmpty() || ended.wait(kAnswerMilliseconds));
        QCOMPARE(logOut.reason(), ChildLogOut::Reason::CommandNotStarted);
    }
};

} // namespace

QTEST_GUILESS_MAIN(ChildLogOutTest)
#include "tst_childlogout.moc"
