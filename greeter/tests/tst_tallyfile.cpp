// SPDX-License-Identifier: Apache-2.0
#include "StandInFaillock.h"
#include "TallyFile.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

namespace {

using Outcome = TallyFile::Outcome;

class TallyFileTest : public QObject {
    Q_OBJECT

private slots:
    // faillock makes the file at the first wrong try, so none means none.
    void noFileIsNoWrongTries() {
        const StandInFaillock faillock;
        const auto result = TallyFile::read(faillock.dir(), QStringLiteral("guardian"));
        QCOMPARE(result.outcome, Outcome::Read);
        QVERIFY(result.failureTimes.isEmpty());
    }

    // A good login empties the file rather than removing it.
    void anEmptyFileIsNoWrongTries() {
        const StandInFaillock faillock;
        faillock.write(QStringLiteral("guardian"), {});
        const auto result = TallyFile::read(faillock.dir(), QStringLiteral("guardian"));
        QCOMPARE(result.outcome, Outcome::Read);
        QVERIFY(result.failureTimes.isEmpty());
    }

    // In a folder that is not there, every account would look unlocked.
    void noFolderIsUnknown() {
        const auto result =
            TallyFile::read(QStringLiteral("/nonexistent/faillock"), QStringLiteral("guardian"));
        QCOMPARE(result.outcome, Outcome::Unknown);
    }

    void aFolderThatCannotBeReadIsUnknown() {
        if (geteuid() == 0) {
            QSKIP("root reads any folder");
        }
        const StandInFaillock faillock;
        QVERIFY(QFile::setPermissions(faillock.dir(), QFileDevice::Permissions()));
        const auto result = TallyFile::read(faillock.dir(), QStringLiteral("guardian"));
        QFile::setPermissions(faillock.dir(), QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                                  QFileDevice::ExeOwner);
        QCOMPARE(result.outcome, Outcome::Unknown);
    }

    // As the login screen sees a record today, without the read permission.
    void aFileThatCannotBeReadIsUnknown() {
        if (geteuid() == 0) {
            QSKIP("root reads any file");
        }
        const StandInFaillock faillock;
        faillock.write(QStringLiteral("guardian"), StandInFaillock::record(1000));
        QVERIFY(QFile::setPermissions(faillock.dir() + QStringLiteral("/guardian"),
                                      QFileDevice::Permissions()));
        const auto result = TallyFile::read(faillock.dir(), QStringLiteral("guardian"));
        QCOMPARE(result.outcome, Outcome::Unknown);
    }

    void aFolderInPlaceOfTheFileIsUnknown() {
        const StandInFaillock faillock;
        QVERIFY(QDir(faillock.dir()).mkdir(QStringLiteral("guardian")));
        const auto result = TallyFile::read(faillock.dir(), QStringLiteral("guardian"));
        QCOMPARE(result.outcome, Outcome::Unknown);
    }

    // A name is a file in the folder, never a way out of it.
    void aNameThatIsNotAFileNameIsUnknown_data() {
        QTest::addColumn<QString>("loginName");
        QTest::newRow("empty") << QString();
        QTest::newRow("a slash") << QStringLiteral("../etc/passwd");
        QTest::newRow("dot") << QStringLiteral(".");
        QTest::newRow("dot dot") << QStringLiteral("..");
    }
    void aNameThatIsNotAFileNameIsUnknown() {
        QFETCH(const QString, loginName);
        const StandInFaillock faillock;
        QCOMPARE(TallyFile::read(faillock.dir(), loginName).outcome, Outcome::Unknown);
    }

    void everyValidRecordIsRead() {
        const StandInFaillock faillock;
        faillock.write(QStringLiteral("guardian"), StandInFaillock::record(1000) +
                                                       StandInFaillock::record(1100) +
                                                       StandInFaillock::record(1200));
        const auto result = TallyFile::read(faillock.dir(), QStringLiteral("guardian"));
        QCOMPARE(result.outcome, Outcome::Read);
        QCOMPARE(result.failureTimes, (QList<qint64>{1000, 1100, 1200}));
    }

    // faillock tests the valid bit alone: sudo's tries carry the terminal
    // bit too, and ssh's the remote-computer bit.
    void theValidBitIsTestedAlone() {
        const StandInFaillock faillock;
        faillock.write(
            QStringLiteral("guardian"),
            StandInFaillock::record(1000, StandInFaillock::valid | StandInFaillock::fromATerminal,
                                    "/dev/pts/1") +
                StandInFaillock::record(
                    1100, StandInFaillock::valid | StandInFaillock::fromAnotherComputer,
                    "10.0.0.2") +
                StandInFaillock::record(1200, StandInFaillock::fromATerminal, "/dev/pts/2") +
                StandInFaillock::record(1300, 0, "sddm"));
        const auto result = TallyFile::read(faillock.dir(), QStringLiteral("guardian"));
        QCOMPARE(result.outcome, Outcome::Read);
        QCOMPARE(result.failureTimes, (QList<qint64>{1000, 1100}));
    }

    // A full 52-byte source with no ending zero, as strncpy leaves it.
    void aSourceThatFillsItsPlaceIsFine() {
        const StandInFaillock faillock;
        faillock.write(QStringLiteral("guardian"),
                       StandInFaillock::record(1000, StandInFaillock::valid, QByteArray(52, 'x')));
        QCOMPARE(TallyFile::read(faillock.dir(), QStringLiteral("guardian")).failureTimes,
                 QList<qint64>{1000});
    }

    // A piece of a record at the end is left out, as faillock leaves it.
    void aPieceOfARecordIsLeftOut() {
        const StandInFaillock faillock;
        faillock.write(QStringLiteral("guardian"),
                       StandInFaillock::record(1000) + StandInFaillock::record(1100).left(6));
        const auto result = TallyFile::read(faillock.dir(), QStringLiteral("guardian"));
        QCOMPARE(result.outcome, Outcome::Read);
        QCOMPARE(result.failureTimes, QList<qint64>{1000});
    }

    void lessThanOneRecordIsNone() {
        const StandInFaillock faillock;
        faillock.write(QStringLiteral("guardian"), StandInFaillock::record(1000).left(63));
        const auto result = TallyFile::read(faillock.dir(), QStringLiteral("guardian"));
        QCOMPARE(result.outcome, Outcome::Read);
        QVERIFY(result.failureTimes.isEmpty());
    }

    // faillock reads no more than 1024 records.
    void onlyTheFirst1024RecordsAreRead() {
        const StandInFaillock faillock;
        QByteArray bytes;
        for (int i = 0; i < 1030; ++i) {
            bytes += StandInFaillock::record(1000 + i);
        }
        faillock.write(QStringLiteral("guardian"), bytes);
        const auto result = TallyFile::read(faillock.dir(), QStringLiteral("guardian"));
        QCOMPARE(result.failureTimes.size(), 1024);
        QCOMPARE(result.failureTimes.last(), 1000 + 1023);
    }

    // No clock gives a time this large: the file is not what it seems.
    void aTimeTooLargeIsUnknown() {
        const StandInFaillock faillock;
        QByteArray bad = StandInFaillock::record(0);
        bad.replace(56, 8, QByteArray(8, '\xff'));
        faillock.write(QStringLiteral("guardian"), StandInFaillock::record(1000) + bad);
        QCOMPARE(TallyFile::read(faillock.dir(), QStringLiteral("guardian")).outcome,
                 Outcome::Unknown);
    }

    // A record faillock no longer counts may hold anything.
    void aTimeTooLargeInAnUncountedRecordIsSkipped() {
        const StandInFaillock faillock;
        QByteArray old = StandInFaillock::record(0, 0);
        old.replace(56, 8, QByteArray(8, '\xff'));
        faillock.write(QStringLiteral("guardian"), old + StandInFaillock::record(1000));
        const auto result = TallyFile::read(faillock.dir(), QStringLiteral("guardian"));
        QCOMPARE(result.outcome, Outcome::Read);
        QCOMPARE(result.failureTimes, QList<qint64>{1000});
    }

    // While faillock holds its lock to rewrite the file, the screen waits
    // for the next read rather than freeze or read half a record.
    void aFileFaillockIsWritingIsBusy() {
        const StandInFaillock faillock;
        faillock.write(QStringLiteral("guardian"), StandInFaillock::record(1000));
        const QByteArray path = (faillock.dir() + QStringLiteral("/guardian")).toLocal8Bit();
        const int writer = open(path.constData(), O_RDWR | O_CLOEXEC);
        QVERIFY(writer >= 0);
        QCOMPARE(flock(writer, LOCK_EX), 0);
        const auto busy = TallyFile::read(faillock.dir(), QStringLiteral("guardian"));
        close(writer);
        QCOMPARE(busy.outcome, Outcome::Busy);
        QCOMPARE(TallyFile::read(faillock.dir(), QStringLiteral("guardian")).outcome,
                 Outcome::Read);
    }

    void aFolderEndingInASlashWorks() {
        const StandInFaillock faillock;
        faillock.write(QStringLiteral("guardian"), StandInFaillock::record(1000));
        QCOMPARE(TallyFile::read(faillock.dir() + QLatin1Char('/'), QStringLiteral("guardian"))
                     .failureTimes,
                 QList<qint64>{1000});
    }
};

} // namespace

QTEST_GUILESS_MAIN(TallyFileTest)
#include "tst_tallyfile.moc"
