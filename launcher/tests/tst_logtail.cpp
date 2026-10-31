// SPDX-License-Identifier: Apache-2.0
#include "LogTail.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

namespace {

using Lines = QList<QByteArray>;

void append(const QString& path, const QByteArray& text) {
    QFile log(path);
    QVERIFY(log.open(QIODevice::WriteOnly | QIODevice::Append));
    log.write(text);
}

// The lines a log file gains, read a part at a time, as the launcher reads
// Steam's logs (ADR-0036, #206).
class LogTailTest : public QObject {
    Q_OBJECT

private slots:
    // No log yet, as before Steam's first start: the log that comes later
    // is read from its start, and is not a new log of a program started
    // again.
    void aLogThatComesLaterIsReadFromItsStart() {
        const QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("console_log.txt"));
        LogTail tail(path);
        tail.skipToEnd();
        QCOMPARE(tail.newLines(), Lines());
        QVERIFY(!tail.startedAgain());
        append(path, "first\nsecond\n");
        QCOMPARE(tail.newLines(), (Lines{"first", "second"}));
        QVERIFY(!tail.startedAgain());
    }

    // Only what the log gains after skipToEnd() counts, each line once.
    void onlyWhatTheLogGainsCounts() {
        const QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("console_log.txt"));
        append(path, "before\n");
        LogTail tail(path);
        tail.skipToEnd();
        QCOMPARE(tail.newLines(), Lines());
        append(path, "after\n");
        QCOMPARE(tail.newLines(), Lines{"after"});
        QCOMPARE(tail.newLines(), Lines());
        append(path, "later\n");
        QCOMPARE(tail.newLines(), Lines{"later"});
        // skipToEnd() again, as at a new tap: what came before is passed.
        append(path, "before the next tap\n");
        tail.skipToEnd();
        append(path, "after the next tap\n");
        QCOMPARE(tail.newLines(), Lines{"after the next tap"});
    }

    // A program may be part-way through a line; it is read once it is
    // whole.
    void aHalfWrittenLineIsReadOnceWhole() {
        const QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("console_log.txt"));
        LogTail tail(path);
        tail.skipToEnd();
        append(path, "whole\nhalf");
        QCOMPARE(tail.newLines(), Lines{"whole"});
        QCOMPARE(tail.newLines(), Lines());
        append(path, " written\n");
        QCOMPARE(tail.newLines(), Lines{"half written"});
    }

    // A log smaller than what was read before is a new one, begun when its
    // program started again: all of it is new, and only that read says so.
    void aNewSmallerLogIsReadFromItsStart() {
        const QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("console_log.txt"));
        append(path, QByteArray(500, 'x') + '\n');
        LogTail tail(path);
        tail.skipToEnd();
        QVERIFY(QFile::remove(path));
        append(path, "new\n");
        QCOMPARE(tail.newLines(), Lines{"new"});
        QVERIFY(tail.startedAgain());
        append(path, "next\n");
        QCOMPARE(tail.newLines(), Lines{"next"});
        QVERIFY(!tail.startedAgain());
    }

    // A log that cannot be read gives nothing, and is read once it can be.
    void aLogThatCannotBeReadGivesNothing() {
        const QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("console_log.txt"));
        append(path, "before\n");
        LogTail tail(path);
        tail.skipToEnd();
        append(path, "after\n");
        QVERIFY(QFile::setPermissions(path, QFile::WriteOwner));
        if (QFile(path).open(QIODevice::ReadOnly)) {
            QSKIP("This user can read a file without read permission.");
        }
        QCOMPARE(tail.newLines(), Lines());
        QVERIFY(QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner));
        QCOMPARE(tail.newLines(), Lines{"after"});
    }

    // More than 4 MB gained since the last look is read from its last part.
    void aLogThatGainedALotIsReadFromItsLastPart() {
        const QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("console_log.txt"));
        LogTail tail(path);
        tail.skipToEnd();
        const QByteArray line(1023, 'x');
        QByteArray lots;
        for (int i = 0; i < 5 * 1024; ++i) {
            lots += line + '\n';
        }
        append(path, "the first line\n" + lots + "the last line\n");
        const Lines lines = tail.newLines();
        QVERIFY(!lines.isEmpty());
        QVERIFY(!lines.contains("the first line"));
        QCOMPARE(lines.last(), QByteArray("the last line"));
        qint64 read = 0;
        for (const QByteArray& each : lines) {
            read += each.size() + 1;
        }
        QVERIFY(read <= qint64{4} * 1024 * 1024);
        // The next read goes on from the end.
        append(path, "next\n");
        QCOMPARE(tail.newLines(), Lines{"next"});
    }
};

} // namespace

QTEST_GUILESS_MAIN(LogTailTest)
#include "tst_logtail.moc"
