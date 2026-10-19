// SPDX-License-Identifier: Apache-2.0
#include "LogOut.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

namespace {

using State = LogOut::State;

// The wait for the session to end, short enough for a test to wait out.
constexpr int shortWaitMilliseconds = 300;

// How many times the fixture has run: it adds a line to the mark each time.
int runs(const QString& mark) {
    QFile file(mark);
    if (!file.open(QIODevice::ReadOnly)) {
        return 0;
    }
    return static_cast<int>(file.readAll().count('\n'));
}

class LogOutTest : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;
    QString m_mark;

private slots:
    void initTestCase() {
        QVERIFY(m_dir.isValid());
    }

    // Each test has a mark of its own, so a run left over from an earlier
    // test cannot count in this one.
    void init() {
        m_mark = m_dir.filePath(QString::fromLatin1(QTest::currentTestFunction()));
        qputenv("CAIRN_TEST_LOG_OUT_MARK", m_mark.toLocal8Bit());
    }

    void cleanup() {
        qunsetenv("CAIRN_TEST_LOG_OUT_MARK");
    }

    // No program, no Log out: a grown-up's run under Plasma never ends the
    // grown-up's own session.
    void withoutAProgramThereIsNoLogOut() {
        LogOut logOut;
        QVERIFY(!logOut.available());
        QVERIFY(!logOut.start());
        QCOMPARE(logOut.state(), State::Idle);
    }

    void settingTheProgramMakesItAvailable() {
        LogOut logOut;
        const QSignalSpy changed(&logOut, &LogOut::programChanged);
        logOut.setProgram(QStringLiteral(LOG_OUT_FIXTURE));
        QVERIFY(logOut.available());
        QCOMPARE(changed.count(), 1);
        logOut.setProgram(QStringLiteral(LOG_OUT_FIXTURE));
        QCOMPARE(changed.count(), 1);
    }

    // The wait covers cairn-log-out's ten seconds for Steam several times
    // over; tests set it short.
    void theWaitIsLongAndCanBeSet() {
        LogOut logOut;
        QCOMPARE(logOut.waitMilliseconds(), 30000);
        const QSignalSpy changed(&logOut, &LogOut::waitMillisecondsChanged);
        logOut.setWaitMilliseconds(shortWaitMilliseconds);
        QCOMPARE(logOut.waitMilliseconds(), shortWaitMilliseconds);
        QCOMPARE(changed.count(), 1);
        logOut.setWaitMilliseconds(shortWaitMilliseconds);
        QCOMPARE(changed.count(), 1);
    }

    // The program really runs, on its own, and the launcher says it is
    // leaving.
    void startRunsTheProgram() {
        LogOut logOut;
        logOut.setProgram(QStringLiteral(LOG_OUT_FIXTURE));
        const QSignalSpy changed(&logOut, &LogOut::stateChanged);
        QVERIFY(logOut.start());
        QCOMPARE(logOut.state(), State::Leaving);
        QCOMPARE(changed.count(), 1);
        QTRY_COMPARE(runs(m_mark), 1);
    }

    // A child pressed Log out six times in two seconds on the laptop
    // (2026-10-18). Every press after the first is the same log-out: true,
    // so the child is not sent back, and no second run.
    void aSecondStartWhileLeavingRunsNothingMore() {
        LogOut logOut;
        logOut.setProgram(QStringLiteral(LOG_OUT_FIXTURE));
        const QSignalSpy changed(&logOut, &LogOut::stateChanged);
        QVERIFY(logOut.start());
        QVERIFY(logOut.start());
        QVERIFY(logOut.start());
        QCOMPARE(logOut.state(), State::Leaving);
        QCOMPARE(changed.count(), 1);
        QTRY_COMPARE(runs(m_mark), 1);
        QTest::qWait(500); // time for a second run, had one started, to leave its line.
        QCOMPARE(runs(m_mark), 1);
    }

    // The launcher never hears how the program ends. A session still here
    // when the wait runs out is one it did not end, and the child can choose
    // again: a press then is a new log-out.
    void theWaitRunningOutLetsItStartAgain() {
        LogOut logOut;
        logOut.setProgram(QStringLiteral(LOG_OUT_FIXTURE));
        logOut.setWaitMilliseconds(shortWaitMilliseconds);
        const QSignalSpy changed(&logOut, &LogOut::stateChanged);
        QVERIFY(logOut.start());
        QTRY_COMPARE(logOut.state(), State::DidNotFinish);
        QCOMPARE(changed.count(), 2);
        QTRY_COMPARE(runs(m_mark), 1);
        QVERIFY(logOut.start());
        QCOMPARE(logOut.state(), State::Leaving);
        QCOMPARE(changed.count(), 3);
        QTRY_COMPARE(runs(m_mark), 2);
    }

    // Back goes back only once the wait has run out; while Leaving the
    // session is ending and there is nothing to go back to.
    void dismissWaitsForTheWaitToRunOut() {
        LogOut logOut;
        logOut.setProgram(QStringLiteral(LOG_OUT_FIXTURE));
        logOut.setWaitMilliseconds(shortWaitMilliseconds);
        logOut.dismiss();
        QCOMPARE(logOut.state(), State::Idle);
        QVERIFY(logOut.start());
        logOut.dismiss();
        QCOMPARE(logOut.state(), State::Leaving);
        QTRY_COMPARE(logOut.state(), State::DidNotFinish);
        logOut.dismiss();
        QCOMPARE(logOut.state(), State::Idle);
    }

    // A program that cannot start is not a log-out under way: false, so the
    // child goes back, and nothing waits for a session to end.
    void aMissingProgramDoesNotStart() {
        LogOut logOut;
        logOut.setProgram(QStringLiteral("/nonexistent/cairn-log-out"));
        logOut.setWaitMilliseconds(shortWaitMilliseconds);
        const QSignalSpy changed(&logOut, &LogOut::stateChanged);
        QVERIFY(logOut.available());
        QVERIFY(!logOut.start());
        QCOMPARE(logOut.state(), State::Idle);
        QTest::qWait(shortWaitMilliseconds * 2);
        QCOMPARE(logOut.state(), State::Idle);
        QCOMPARE(changed.count(), 0);
    }

    // The same after a log-out that did not finish: not under way.
    void aMissingProgramAfterTheWaitDoesNotStart() {
        LogOut logOut;
        logOut.setProgram(QStringLiteral(LOG_OUT_FIXTURE));
        logOut.setWaitMilliseconds(shortWaitMilliseconds);
        QVERIFY(logOut.start());
        QTRY_COMPARE(logOut.state(), State::DidNotFinish);
        logOut.setProgram(QStringLiteral("/nonexistent/cairn-log-out"));
        QVERIFY(!logOut.start());
        QCOMPARE(logOut.state(), State::Idle);
    }
};

} // namespace

QTEST_GUILESS_MAIN(LogOutTest)
#include "tst_logout.moc"
