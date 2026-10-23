// SPDX-License-Identifier: Apache-2.0
#include "FamilyChange.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

namespace {

constexpr int kAnswerMilliseconds = 10000;

QString pkexec() {
    return QStringLiteral(FIXTURES "/pkexec-family");
}

QString command() {
    return QStringLiteral(FIXTURES "/cairn-family");
}

// The one answer a scenario of the stand-ins gives.
QString answerTo(const QString& scenario) {
    qputenv("CAIRN_TEST_SCENARIO", scenario.toLocal8Bit());
    FamilyChange change(pkexec(), command());
    QSignalSpy finished(&change, &FamilyChange::finished);
    if (!change.start({QStringLiteral("set-level"), QStringLiteral("ada"), QStringLiteral("2")})) {
        return QStringLiteral("not started");
    }
    if (!finished.wait(kAnswerMilliseconds) || finished.size() != 1) {
        return QStringLiteral("no answer");
    }
    return finished.constFirst().constFirst().toString();
}

class FamilyChangeTest : public QObject {
    Q_OBJECT

private slots:
    void answers_data() {
        QTest::addColumn<QString>("scenario");
        QTest::addColumn<QString>("answer");

        QTest::newRow("done") << "done" << "done";
        // Every word cairn-family can give after "refused" (ADR-0039).
        for (const char* word : {"not-root", "not-a-grown-up", "usage", "bad-name", "bad-full-name",
                                 "bad-level", "taken", "not-a-child", "failed"}) {
            QTest::newRow(word) << QStringLiteral("refused-") + word << QString::fromLatin1(word);
        }
        // pkexec's own, when the command never ran.
        QTest::newRow("box dismissed") << "dismissed" << "dismissed";
        QTest::newRow("password refused") << "not-authorized" << "not-authorized";
        // Anything else is an answer the window cannot read.
        QTest::newRow("no answer") << "silent" << "unknown";
        QTest::newRow("garbage") << "garbage" << "unknown";
        QTest::newRow("two lines") << "two-lines" << "unknown";
        QTest::newRow("done with an exit of 1") << "done-but-failed" << "unknown";
        QTest::newRow("refused with an exit of 0") << "zero-after-refused" << "unknown";
        QTest::newRow("crashed") << "crashes" << "unknown";
        QTest::newRow("exit 1, nothing said") << "no-such-scenario" << "unknown";
    }

    void answers() {
        QFETCH(const QString, scenario);
        QFETCH(const QString, answer);
        QCOMPARE(answerTo(scenario), answer);
    }

    void pkexecMissing() {
        qputenv("CAIRN_TEST_SCENARIO", "done");
        FamilyChange change(QStringLiteral(FIXTURES "/no-such-pkexec"), command());
        QSignalSpy finished(&change, &FamilyChange::finished);
        QVERIFY(change.start(
            {QStringLiteral("set-level"), QStringLiteral("ada"), QStringLiteral("2")}));
        QVERIFY(finished.wait(kAnswerMilliseconds));
        QCOMPARE(finished.size(), 1);
        QCOMPARE(finished.constFirst().constFirst().toString(), QStringLiteral("unknown"));
        QVERIFY(!change.running());
    }

    // pkexec would say 127, as for a refused password, without asking for
    // one. The window does not run it, and says it cannot read an answer.
    void commandMissing() {
        const QTemporaryDir folder;
        const QString record = folder.filePath(QStringLiteral("arguments"));
        qputenv("CAIRN_TEST_ARGUMENTS", record.toLocal8Bit());
        qputenv("CAIRN_TEST_SCENARIO", "not-authorized");
        FamilyChange change(pkexec(), QStringLiteral(FIXTURES "/no-such-cairn-family"));
        QSignalSpy finished(&change, &FamilyChange::finished);
        QVERIFY(change.start(
            {QStringLiteral("set-level"), QStringLiteral("ada"), QStringLiteral("2")}));
        // The answer comes after start() returns, and nothing else starts
        // before it.
        QVERIFY(finished.isEmpty());
        QVERIFY(change.running());
        QVERIFY(!change.start(
            {QStringLiteral("set-level"), QStringLiteral("ben"), QStringLiteral("1")}));
        QVERIFY(finished.wait(kAnswerMilliseconds));
        qunsetenv("CAIRN_TEST_ARGUMENTS");
        QCOMPARE(finished.size(), 1);
        QCOMPARE(finished.constFirst().constFirst().toString(), QStringLiteral("unknown"));
        QVERIFY(!change.running());
        QVERIFY(!QFile::exists(record));
    }

    // pkexec gets the command's path, then the arguments as they are, one
    // each, with no shell to split or expand them.
    void theArgumentsGoAsTheyAre() {
        const QTemporaryDir folder;
        const QString record = folder.filePath(QStringLiteral("arguments"));
        qputenv("CAIRN_TEST_ARGUMENTS", record.toLocal8Bit());
        qputenv("CAIRN_TEST_SCENARIO", "done");
        FamilyChange change(pkexec(), command());
        QSignalSpy finished(&change, &FamilyChange::finished);
        QVERIFY(change.start({QStringLiteral("create-child"), QStringLiteral("ada"),
                              QStringLiteral("1"), QStringLiteral("--full-name"),
                              QStringLiteral("Ada $HOME * ; Lovelace")}));
        QVERIFY(finished.wait(kAnswerMilliseconds));
        qunsetenv("CAIRN_TEST_ARGUMENTS");
        QFile file(record);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(
            QString::fromUtf8(file.readAll()),
            QStringLiteral("run\ncreate-child\nada\n1\n--full-name\nAda $HOME * ; Lovelace\n"));
    }

    // While the password box is open, a second change does not start.
    void oneChangeAtATime() {
        const QTemporaryDir folder;
        const QString record = folder.filePath(QStringLiteral("arguments"));
        qputenv("CAIRN_TEST_ARGUMENTS", record.toLocal8Bit());
        qputenv("CAIRN_TEST_SCENARIO", "asking");
        FamilyChange change(pkexec(), command());
        const QSignalSpy finished(&change, &FamilyChange::finished);
        QVERIFY(change.start(
            {QStringLiteral("set-level"), QStringLiteral("ada"), QStringLiteral("2")}));
        QVERIFY(change.running());
        QVERIFY(!change.start(
            {QStringLiteral("set-level"), QStringLiteral("ben"), QStringLiteral("1")}));
        QTest::qWait(300);
        qunsetenv("CAIRN_TEST_ARGUMENTS");
        QVERIFY(finished.isEmpty());
        QFile file(record);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll().count("run\n"), 1);
    }

    // Once the answer is in, the next change may start, from the very
    // slot that hears the answer.
    void theNextChangeAfterTheAnswer() {
        qputenv("CAIRN_TEST_SCENARIO", "done");
        FamilyChange change(pkexec(), command());
        const QSignalSpy finished(&change, &FamilyChange::finished);
        bool startedAgain = false;
        connect(&change, &FamilyChange::finished, this, [&change, &startedAgain] {
            if (!startedAgain) {
                startedAgain = change.start(
                    {QStringLiteral("set-level"), QStringLiteral("ada"), QStringLiteral("1")});
            }
        });
        QVERIFY(change.start(
            {QStringLiteral("set-level"), QStringLiteral("ada"), QStringLiteral("2")}));
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 2, kAnswerMilliseconds);
        QVERIFY(startedAgain);
        QVERIFY(!change.running());
    }
};

} // namespace

QTEST_GUILESS_MAIN(FamilyChangeTest)
#include "tst_familychange.moc"
