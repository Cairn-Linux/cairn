// SPDX-License-Identifier: Apache-2.0
#include "GiveUpWatch.h"

#include <QDateTime>
#include <QFile>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

namespace {

class GiveUpWatchTest : public QObject {
    Q_OBJECT

private slots:
    // cairn-give-up writes the file once it has ended the child's programs;
    // the launcher hears it and goes back to the tiles (ADR-0026).
    void writingTheFileIsAPress() {
        const QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("cairn/give-up"));
        GiveUpWatch watch(this);
        watch.setPath(path);
        QVERIFY2(QFile::exists(path), "the launcher makes the file the helper writes");
        const QSignalSpy presses(&watch, &GiveUpWatch::pressed);

        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write("1727740000123\n");
        file.close();
        QTRY_COMPARE(presses.count(), 1);
        QCOMPARE(presses.first().first().toLongLong(), 1727740000123);
    }

    // The helper empties the file and then writes the time; the empty file
    // alone is no press, or it would read as a press with no time.
    void anEmptyFileIsNoPress() {
        const QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("give-up"));
        GiveUpWatch watch(this);
        watch.setPath(path);
        const QSignalSpy presses(&watch, &GiveUpWatch::pressed);

        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Append));
        file.write("\n");
        file.close();
        QTest::qWait(200);
        QCOMPARE(presses.count(), 0);
    }

    // Anything but a time still means the key was pressed, now.
    void aFileWithNoTimeIsAPressNow() {
        const QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("give-up"));
        GiveUpWatch watch(this);
        watch.setPath(path);
        const QSignalSpy presses(&watch, &GiveUpWatch::pressed);
        const qint64 before = QDateTime::currentMSecsSinceEpoch();

        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write("pressed\n");
        file.close();
        QTRY_COMPARE(presses.count(), 1);
        QVERIFY(presses.first().first().toLongLong() >= before);
    }

    void noPathWatchesNothing() {
        const GiveUpWatch watch(this);
        QVERIFY(watch.path().isEmpty());
    }

    void theSessionsFileIsInItsRuntimeDirectory() {
        QCOMPARE(GiveUpWatch::sessionPath(),
                 QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation) +
                     QStringLiteral("/cairn/give-up"));
    }
};

} // namespace

QTEST_GUILESS_MAIN(GiveUpWatchTest)
#include "tst_giveupwatch.moc"
