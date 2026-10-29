// SPDX-License-Identifier: Apache-2.0
#include "SaveRun.h"

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

QStringList saveTwo() {
    return {QStringLiteral("save-games"), QStringLiteral("ada=1001,1002"), QStringLiteral("ben=")};
}

struct Heard {
    QStringList lines;
    QString answer;
};

// Every line and the answer a scenario of the stand-ins gives.
Heard hear(const QString& scenario) {
    qputenv("CAIRN_TEST_SCENARIO", scenario.toLocal8Bit());
    SaveRun run(pkexec(), command());
    Heard heard;
    QObject::connect(&run, &SaveRun::lineRead,
                     [&heard](const QString& line) { heard.lines.append(line); });
    QSignalSpy finished(&run, &SaveRun::finished);
    if (!run.start(saveTwo())) {
        heard.answer = QStringLiteral("not started");
    } else if (!finished.wait(kAnswerMilliseconds) || finished.size() != 1) {
        heard.answer = QStringLiteral("no answer");
    } else {
        heard.answer = finished.constFirst().constFirst().toString();
    }
    return heard;
}

class SaveRunTest : public QObject {
    Q_OBJECT

private slots:
    void answers_data() {
        QTest::addColumn<QString>("scenario");
        QTest::addColumn<QString>("answer");

        QTest::newRow("a whole Save") << "save-done" << "done";
        QTest::newRow("done alone") << "done"
                                    << "done";
        // The command's refusal words for a Save (ADR-0041).
        for (const char* word : {"not-root", "not-a-grown-up", "usage", "bad-name", "bad-appid",
                                 "not-a-child", "busy"}) {
            QTest::newRow(word) << QStringLiteral("refused-") + word << QString::fromLatin1(word);
        }
        // pkexec's own, when the command never ran.
        QTest::newRow("box dismissed") << "dismissed" << "dismissed";
        QTest::newRow("password refused") << "not-authorized" << "not-authorized";
        // Anything else is an answer the window cannot read.
        QTest::newRow("no answer") << "silent" << "unknown";
        QTest::newRow("garbage") << "garbage" << "unknown";
        QTest::newRow("done with an exit of 1") << "done-but-failed" << "unknown";
        QTest::newRow("refused with an exit of 0") << "zero-after-refused" << "unknown";
        QTest::newRow("crashed") << "crashes" << "unknown";
        QTest::newRow("cut off before done") << "save-cut-off" << "unknown";
    }

    void answers() {
        QFETCH(const QString, scenario);
        QFETCH(const QString, answer);
        QCOMPARE(hear(scenario).answer, answer);
    }

    // Every line is passed on, in order, the last one too.
    void everyLineIsPassedOn() {
        const Heard heard = hear(QStringLiteral("save-done"));
        QCOMPARE(heard.lines,
                 QStringList({"pictures made", "catalogue made", "copying ada", "copied ada 1001",
                              "copied ada 1002", "child ada added 2 removed 0",
                              "child ben added 0 removed 0", "done"}));
    }

    // A line too long to be the command's is dropped, whole.
    void aLongLineIsDropped() {
        const Heard heard = hear(QStringLiteral("long-line"));
        QCOMPARE(heard.lines, QStringList({"done"}));
        QCOMPARE(heard.answer, QStringLiteral("done"));
    }

    // Lines come while the command still runs, so the page can say a copy
    // is under way.
    void linesComeAsTheyAreSaid() {
        qputenv("CAIRN_TEST_SCENARIO", "save-copying");
        SaveRun run(pkexec(), command());
        const QSignalSpy lines(&run, &SaveRun::lineRead);
        const QSignalSpy finished(&run, &SaveRun::finished);
        QVERIFY(run.start(saveTwo()));
        QTRY_COMPARE_WITH_TIMEOUT(lines.size(), 3, kAnswerMilliseconds);
        QCOMPARE(lines.at(2).constFirst().toString(), QStringLiteral("copying ada"));
        QVERIFY(run.running());
        QVERIFY(finished.isEmpty());
    }

    // pkexec gets the command's path, then the arguments as they are, one
    // each, with no shell to split or expand them.
    void theArgumentsGoAsTheyAre() {
        const QTemporaryDir folder;
        const QString record = folder.filePath(QStringLiteral("arguments"));
        qputenv("CAIRN_TEST_ARGUMENTS", record.toLocal8Bit());
        hear(QStringLiteral("done"));
        qunsetenv("CAIRN_TEST_ARGUMENTS");
        QFile file(record);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(QString::fromUtf8(file.readAll()),
                 QStringLiteral("run\nsave-games\nada=1001,1002\nben=\n"));
    }

    void pkexecMissing() {
        qputenv("CAIRN_TEST_SCENARIO", "done");
        SaveRun run(QStringLiteral(FIXTURES "/no-such-pkexec"), command());
        QSignalSpy finished(&run, &SaveRun::finished);
        QVERIFY(run.start(saveTwo()));
        QVERIFY(finished.wait(kAnswerMilliseconds));
        QCOMPARE(finished.constFirst().constFirst().toString(), QStringLiteral("unknown"));
        QVERIFY(!run.running());
    }

    // pkexec would say 127, as for a refused password, without asking for
    // one. The window does not run it, and says it cannot read an answer.
    void commandMissing() {
        const QTemporaryDir folder;
        const QString record = folder.filePath(QStringLiteral("arguments"));
        qputenv("CAIRN_TEST_ARGUMENTS", record.toLocal8Bit());
        qputenv("CAIRN_TEST_SCENARIO", "not-authorized");
        SaveRun run(pkexec(), QStringLiteral(FIXTURES "/no-such-cairn-family"));
        QSignalSpy finished(&run, &SaveRun::finished);
        QVERIFY(run.start(saveTwo()));
        QVERIFY(finished.isEmpty());
        QVERIFY(run.running());
        QVERIFY(!run.start(saveTwo()));
        QVERIFY(finished.wait(kAnswerMilliseconds));
        qunsetenv("CAIRN_TEST_ARGUMENTS");
        QCOMPARE(finished.size(), 1);
        QCOMPARE(finished.constFirst().constFirst().toString(), QStringLiteral("unknown"));
        QVERIFY(!run.running());
        QVERIFY(!QFile::exists(record));
    }

    // While the password box is open, a second Save does not start.
    void oneSaveAtATime() {
        qputenv("CAIRN_TEST_SCENARIO", "asking");
        SaveRun run(pkexec(), command());
        QVERIFY(run.start(saveTwo()));
        QVERIFY(run.running());
        QVERIFY(!run.start(saveTwo()));
    }

    // Once the answer is in, the next Save may start, from the very slot
    // that hears the answer.
    void theNextSaveAfterTheAnswer() {
        qputenv("CAIRN_TEST_SCENARIO", "done");
        SaveRun run(pkexec(), command());
        const QSignalSpy finished(&run, &SaveRun::finished);
        bool startedAgain = false;
        connect(&run, &SaveRun::finished, this, [&run, &startedAgain] {
            if (!startedAgain) {
                startedAgain = run.start(saveTwo());
            }
        });
        QVERIFY(run.start(saveTwo()));
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 2, kAnswerMilliseconds);
        QVERIFY(startedAgain);
        QVERIFY(!run.running());
    }
};

} // namespace

QTEST_GUILESS_MAIN(SaveRunTest)
#include "tst_saverun.moc"
