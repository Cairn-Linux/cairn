// SPDX-License-Identifier: Apache-2.0
#include "GiveUpWatch.h"

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
        file.write("1727740000\n");
        file.close();
        QTRY_COMPARE(presses.count(), 1);
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
