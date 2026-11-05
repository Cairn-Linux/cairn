// SPDX-License-Identifier: Apache-2.0
#include "FailedLogin.h"
#include "StandInFaillock.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

namespace {

using Reason = FailedLogin::Reason;

// A list of children on a visit, as the root helper writes it.
QString writeList(const QTemporaryDir& dir, const QByteArray& text) {
    const QString path = dir.filePath(QStringLiteral("steam-visits"));
    QFile list(path);
    if (list.open(QIODevice::WriteOnly)) {
        list.write(text);
    }
    return path;
}

constexpr qint64 t = 1'800'000'000;

// A lock watching loginName, with that many wrong tries just made.
void watch(AccountLock& lock, const StandInFaillock& faillock, const QString& loginName,
           int wrongTries) {
    for (int i = 0; i < wrongTries; ++i) {
        faillock.addFailure(loginName, t);
    }
    lock.setSettingsFile(faillock.settingsFile());
    lock.setFixedTime(QDateTime::fromSecsSinceEpoch(t));
    lock.setLoginName(loginName);
}

class FailedLoginTest : public QObject {
    Q_OBJECT

private slots:
    void theDefaultIsTheListPamReads() {
        const FailedLogin failed;
        QCOMPARE(failed.visitsFile().toLocalFile(), QStringLiteral("/run/cairn/steam-visits"));
    }

    void aChildOnAVisitIsTheReason() {
        const QTemporaryDir dir;
        FailedLogin failed;
        failed.setVisitsFile(QUrl::fromLocalFile(writeList(dir, "ada\n")));
        QCOMPARE(failed.reasonFor(QStringLiteral("ada"), false), Reason::SteamVisit);
    }

    // PAM refuses a listed child whatever the password, so a right one
    // is not called wrong.
    void theVisitComesBeforeThePassword() {
        const QTemporaryDir dir;
        FailedLogin failed;
        failed.setVisitsFile(QUrl::fromLocalFile(writeList(dir, "ben\n")));
        QCOMPARE(failed.reasonFor(QStringLiteral("ben"), true), Reason::SteamVisit);
    }

    void anotherChildIsNotOnAVisit() {
        const QTemporaryDir dir;
        FailedLogin failed;
        failed.setVisitsFile(QUrl::fromLocalFile(writeList(dir, "ada\n")));
        QCOMPARE(failed.reasonFor(QStringLiteral("ben"), false), Reason::NeedsGrownUp);
        QCOMPARE(failed.reasonFor(QStringLiteral("guardian"), true), Reason::WrongPassword);
    }

    // Only a whole line names an account.
    void aNameInsideAnotherIsNotIt() {
        const QTemporaryDir dir;
        const QString list = writeList(dir, "adam\n");
        QVERIFY(!FailedLogin::lists(list, QStringLiteral("ada")));
        QVERIFY(FailedLogin::lists(list, QStringLiteral("adam")));
    }

    void everyLineCounts() {
        const QTemporaryDir dir;
        const QString list = writeList(dir, "ada\nben");
        QVERIFY(FailedLogin::lists(list, QStringLiteral("ada")));
        QVERIFY(FailedLogin::lists(list, QStringLiteral("ben")));
    }

    void noListNamesNoOne() {
        QVERIFY(!FailedLogin::lists(QStringLiteral("/nonexistent/steam-visits"),
                                    QStringLiteral("ada")));
        FailedLogin failed;
        QCOMPARE(failed.reasonFor(QStringLiteral("nobody-here"), false), Reason::NeedsGrownUp);
    }

    // With no lock to look at, a wrong password keeps the plain words.
    void noLockIsTheOldWords() {
        FailedLogin failed;
        QCOMPARE(failed.reasonFor(QStringLiteral("guardian"), true), Reason::WrongPassword);
    }

    void aLockedAccountIsLocked() {
        const StandInFaillock faillock;
        AccountLock lock;
        watch(lock, faillock, QStringLiteral("guardian"), 3);
        FailedLogin failed;
        failed.setAccountLock(&lock);
        QCOMPARE(failed.reasonFor(QStringLiteral("guardian"), true), Reason::Locked);
    }

    // SDDM says a login failed twice for each try; both get the same words.
    void theSameAnswerTwice() {
        const StandInFaillock faillock;
        AccountLock lock;
        watch(lock, faillock, QStringLiteral("guardian"), 2);
        FailedLogin failed;
        failed.setAccountLock(&lock);
        QCOMPARE(failed.reasonFor(QStringLiteral("guardian"), true), Reason::OneTryLeft);
        faillock.addFailure(QStringLiteral("guardian"), t);
        QCOMPARE(failed.reasonFor(QStringLiteral("guardian"), true), Reason::Locked);
        QCOMPARE(failed.reasonFor(QStringLiteral("guardian"), true), Reason::Locked);
    }

    // faillock refuses before the visit list is read, so a locked older
    // child on a visit is told about the lock.
    void theLockComesBeforeTheVisit() {
        const QTemporaryDir dir;
        const StandInFaillock faillock;
        AccountLock lock;
        watch(lock, faillock, QStringLiteral("ben"), 3);
        FailedLogin failed;
        failed.setVisitsFile(QUrl::fromLocalFile(writeList(dir, "ben\n")));
        failed.setAccountLock(&lock);
        QCOMPARE(failed.reasonFor(QStringLiteral("ben"), true), Reason::Locked);
    }

    // Not locked, the visit is still why, whatever the count says.
    void theVisitComesBeforeTheCount() {
        const QTemporaryDir dir;
        const StandInFaillock faillock;
        AccountLock lock;
        watch(lock, faillock, QStringLiteral("ben"), 2);
        FailedLogin failed;
        failed.setVisitsFile(QUrl::fromLocalFile(writeList(dir, "ben\n")));
        failed.setAccountLock(&lock);
        QCOMPARE(failed.reasonFor(QStringLiteral("ben"), true), Reason::SteamVisit);
    }

    void theWrongPasswordWords_data() {
        QTest::addColumn<QByteArray>("settings");
        QTest::addColumn<int>("wrongTries");
        QTest::addColumn<Reason>("reason");
        QTest::newRow("two tries left")
            << QByteArray("unlock_time = 600\n") << 1 << Reason::TryAgain;
        QTest::newRow("one try left")
            << QByteArray("unlock_time = 600\n") << 2 << Reason::OneTryLeft;
        QTest::newRow("one try left, no time")
            << QByteArray("unlock_time = never\n") << 2 << Reason::OneTryLeftNoTime;
        QTest::newRow("settings not understood")
            << QByteArray("admin_group = wheel\n") << 2 << Reason::WrongPassword;
    }
    void theWrongPasswordWords() {
        QFETCH(const QByteArray, settings);
        QFETCH(const int, wrongTries);
        QFETCH(const Reason, reason);
        const StandInFaillock faillock(settings);
        AccountLock lock;
        watch(lock, faillock, QStringLiteral("guardian"), wrongTries);
        FailedLogin failed;
        failed.setAccountLock(&lock);
        QCOMPARE(failed.reasonFor(QStringLiteral("guardian"), true), reason);
    }

    // A lock that ran out while the tile was open: a wrong password after
    // it gets the short words, not the old long sentence.
    void aWrongTryAfterALockRunsOutSaysTryAgain() {
        const StandInFaillock faillock;
        AccountLock lock;
        watch(lock, faillock, QStringLiteral("guardian"), 3);
        lock.setFixedTime(QDateTime::fromSecsSinceEpoch(t + 601));
        QCOMPARE(lock.state(), AccountLock::State::OpenAgain);
        lock.noteTry();
        // write_tally forgets the old tries once the lock has run out.
        faillock.write(QStringLiteral("guardian"), StandInFaillock::record(t + 601));
        FailedLogin failed;
        failed.setAccountLock(&lock);
        QCOMPARE(failed.reasonFor(QStringLiteral("guardian"), true), Reason::TryAgain);
    }

    // A try sent in the last seconds of a lock is refused for the lock, and
    // PAM answers about two seconds later, after the lock has run out. The
    // password was never checked, so it is not called wrong.
    void aTrySentWhileLockedIsNotCalledWrongAfterTheLockEnds() {
        const StandInFaillock faillock;
        AccountLock lock;
        watch(lock, faillock, QStringLiteral("guardian"), 3);
        lock.setFixedTime(QDateTime::fromSecsSinceEpoch(t + 599));
        lock.noteTry();
        lock.setFixedTime(QDateTime::fromSecsSinceEpoch(t + 601));
        FailedLogin failed;
        failed.setAccountLock(&lock);
        QCOMPARE(failed.reasonFor(QStringLiteral("guardian"), true), Reason::LockRanOut);
        QCOMPARE(failed.reasonFor(QStringLiteral("guardian"), true), Reason::LockRanOut);
    }

    // A lock read for one account never speaks for another.
    void anotherAccountsLockIsNotThisOnesReason() {
        const StandInFaillock faillock;
        AccountLock lock;
        watch(lock, faillock, QStringLiteral("guardian"), 3);
        FailedLogin failed;
        failed.setAccountLock(&lock);
        QCOMPARE(failed.reasonFor(QStringLiteral("ben"), true), Reason::WrongPassword);
    }

    // L1 and L2 children are let in before faillock, so a tile with no
    // password row is never told about a lock, even with a stray record.
    void aChildWithNoPasswordIsNeverLocked() {
        const StandInFaillock faillock;
        AccountLock lock;
        watch(lock, faillock, QStringLiteral("ada"), 3);
        FailedLogin failed;
        failed.setAccountLock(&lock);
        QCOMPARE(failed.reasonFor(QStringLiteral("ada"), false), Reason::NeedsGrownUp);
    }

    void anEmptyNameIsNeverListed() {
        const QTemporaryDir dir;
        QVERIFY(!FailedLogin::lists(writeList(dir, "\n"), QString()));
    }
};

} // namespace

QTEST_GUILESS_MAIN(FailedLoginTest)
#include "tst_failedlogin.moc"
