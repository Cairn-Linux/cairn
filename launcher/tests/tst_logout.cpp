// SPDX-License-Identifier: Apache-2.0
#include "LogOut.h"

#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

namespace {

class LogOutTest : public QObject {
    Q_OBJECT

private slots:
    // No program, no Log out: a grown-up's run under Plasma never ends the
    // grown-up's own session.
    void withoutAProgramThereIsNoLogOut() {
        LogOut logOut;
        QVERIFY(!logOut.available());
        QVERIFY(!logOut.start());
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

    // The program really runs, on its own.
    void startRunsTheProgram() {
        const QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString mark = dir.filePath(QStringLiteral("logged-out"));
        qputenv("CAIRN_TEST_LOG_OUT_MARK", mark.toLocal8Bit());
        LogOut logOut;
        logOut.setProgram(QStringLiteral(LOG_OUT_FIXTURE));
        QVERIFY(logOut.start());
        QTRY_VERIFY(QFileInfo::exists(mark));
        qunsetenv("CAIRN_TEST_LOG_OUT_MARK");
    }

    void aMissingProgramDoesNotStart() {
        LogOut logOut;
        logOut.setProgram(QStringLiteral("/nonexistent/cairn-log-out"));
        QVERIFY(logOut.available());
        QVERIFY(!logOut.start());
    }
};

} // namespace

QTEST_GUILESS_MAIN(LogOutTest)
#include "tst_logout.moc"
