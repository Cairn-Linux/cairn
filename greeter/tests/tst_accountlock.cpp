// SPDX-License-Identifier: Apache-2.0
#include "AccountLock.h"
#include "StandInFaillock.h"

#include <QSignalSpy>
#include <QTest>
#include <QTimer>

#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

namespace {

using State = AccountLock::State;

constexpr qint64 t = 1'800'000'000;

QDateTime at(qint64 seconds) {
    return QDateTime::fromSecsSinceEpoch(seconds);
}

// A lock watching guardian in faillock's folder, at time now.
void watch(AccountLock& lock, const StandInFaillock& faillock, qint64 now) {
    lock.setSettingsFile(faillock.settingsFile());
    lock.setFixedTime(at(now));
    lock.setLoginName(QStringLiteral("guardian"));
}

class AccountLockTest : public QObject {
    Q_OBJECT

private slots:
    void theDefaultIsTheSystemsSettings() {
        const AccountLock lock;
        QCOMPARE(lock.settingsFile().toLocalFile(), QStringLiteral("/etc/security/faillock.conf"));
        QCOMPARE(lock.state(), State::Unknown);
    }

    void noOneWatchedIsUnknown() {
        const StandInFaillock faillock;
        AccountLock lock;
        lock.setSettingsFile(faillock.settingsFile());
        QCOMPARE(lock.state(), State::Unknown);
        QVERIFY(!lock.locked());
    }

    void noWrongTriesIsOpen() {
        const StandInFaillock faillock;
        AccountLock lock;
        watch(lock, faillock, t);
        QCOMPARE(lock.state(), State::Open);
        QCOMPARE(lock.lockMinutes(), 10);
    }

    void twoWrongTriesLeaveOne() {
        const StandInFaillock faillock;
        faillock.addFailure(QStringLiteral("guardian"), t);
        faillock.addFailure(QStringLiteral("guardian"), t + 10);
        AccountLock lock;
        watch(lock, faillock, t + 20);
        QCOMPARE(lock.state(), State::OneTryLeft);
        QCOMPARE(lock.lockMinutes(), 10);
        QVERIFY(!lock.locked());
    }

    // The count is the account's alone.
    void anotherAccountsTriesDoNotCount() {
        const StandInFaillock faillock;
        for (int i = 0; i < 3; ++i) {
            faillock.addFailure(QStringLiteral("ben"), t);
        }
        AccountLock lock;
        watch(lock, faillock, t);
        QCOMPARE(lock.state(), State::Open);
    }

    void threeWrongTriesLock() {
        const StandInFaillock faillock;
        for (int i = 0; i < 3; ++i) {
            faillock.addFailure(QStringLiteral("guardian"), t);
        }
        AccountLock lock;
        watch(lock, faillock, t);
        QCOMPARE(lock.state(), State::Locked);
        QVERIFY(lock.locked());
        QCOMPARE(lock.minutesLeft(), 10);
    }

    // Rounded up, as faillock rounds: one second gone is still 10 minutes,
    // and the count reaches "under a minute" only at 60 seconds left.
    void theMinutesLeft_data() {
        QTest::addColumn<qint64>("elapsed");
        QTest::addColumn<State>("state");
        QTest::addColumn<int>("minutesLeft");
        QTest::newRow("just locked") << 0LL << State::Locked << 10;
        QTest::newRow("a second later") << 1LL << State::Locked << 10;
        QTest::newRow("a minute later") << 60LL << State::Locked << 9;
        QTest::newRow("61 seconds later") << 61LL << State::Locked << 9;
        QTest::newRow("61 seconds left") << 539LL << State::Locked << 2;
        QTest::newRow("60 seconds left") << 540LL << State::LockedUnderAMinute << 1;
        QTest::newRow("1 second left") << 599LL << State::LockedUnderAMinute << 1;
        QTest::newRow("0 seconds left, still refused") << 600LL << State::LockedUnderAMinute << 1;
        QTest::newRow("open again") << 601LL << State::Open << 0;
    }
    void theMinutesLeft() {
        QFETCH(const qint64, elapsed);
        QFETCH(const State, state);
        QFETCH(const int, minutesLeft);
        const StandInFaillock faillock;
        for (int i = 0; i < 3; ++i) {
            faillock.addFailure(QStringLiteral("guardian"), t);
        }
        AccountLock lock;
        watch(lock, faillock, t + elapsed);
        QCOMPARE(lock.state(), state);
        QCOMPARE(lock.minutesLeft(), minutesLeft);
    }

    // The words move once a minute, not every second.
    void itAnnouncesOnlyWholeMinutes() {
        const StandInFaillock faillock;
        for (int i = 0; i < 3; ++i) {
            faillock.addFailure(QStringLiteral("guardian"), t);
        }
        AccountLock lock;
        watch(lock, faillock, t);
        const QSignalSpy changes(&lock, &AccountLock::stateChanged);
        for (qint64 second = 1; second <= 180; ++second) {
            lock.setFixedTime(at(t + second));
        }
        // 10 to 9 at 60, to 8 at 120, to 7 at 180.
        QCOMPARE(changes.count(), 3);
        QCOMPARE(lock.minutesLeft(), 7);
    }

    // Watched through the end of the lock, the screen can say so.
    void aLockThatEndsWhileWatchedIsOpenAgain() {
        const StandInFaillock faillock;
        for (int i = 0; i < 3; ++i) {
            faillock.addFailure(QStringLiteral("guardian"), t);
        }
        AccountLock lock;
        watch(lock, faillock, t + 590);
        QCOMPARE(lock.state(), State::LockedUnderAMinute);
        lock.setFixedTime(at(t + 601));
        QCOMPARE(lock.state(), State::OpenAgain);
        QVERIFY(!lock.locked());
        // Another account, or the same one chosen again, starts afresh.
        lock.setLoginName(QStringLiteral("ben"));
        lock.setLoginName(QStringLiteral("guardian"));
        QCOMPARE(lock.state(), State::Open);
    }

    // While locked it reads again by itself: a grown-up's faillock --reset
    // shows on the screen without anyone touching it.
    void itReadsAgainWhileLocked() {
        const StandInFaillock faillock;
        for (int i = 0; i < 3; ++i) {
            faillock.addFailure(QStringLiteral("guardian"), t);
        }
        AccountLock lock;
        watch(lock, faillock, t);
        QVERIFY(lock.locked());
        faillock.write(QStringLiteral("guardian"), {});
        QTRY_COMPARE(lock.state(), State::OpenAgain);
    }

    void aTryFromSudoCountsToo() {
        const StandInFaillock faillock;
        faillock.write(
            QStringLiteral("guardian"),
            StandInFaillock::record(t, StandInFaillock::valid | StandInFaillock::fromATerminal) +
                StandInFaillock::record(t + 1,
                                        StandInFaillock::valid | StandInFaillock::fromATerminal) +
                StandInFaillock::record(t + 2));
        AccountLock lock;
        watch(lock, faillock, t + 2);
        QCOMPARE(lock.state(), State::Locked);
    }

    void neverUnlockingIsLockedUntilReset() {
        const StandInFaillock faillock("unlock_time = never\n");
        for (int i = 0; i < 3; ++i) {
            faillock.addFailure(QStringLiteral("guardian"), t);
        }
        AccountLock lock;
        watch(lock, faillock, t + 86400);
        QCOMPARE(lock.state(), State::LockedUntilReset);
        QVERIFY(lock.locked());
    }

    // "for 1 minutes" would read wrong, and "for 0 minutes" more so.
    void aLockNotSaidInMinutes_data() {
        QTest::addColumn<QByteArray>("settings");
        QTest::newRow("never") << QByteArray("unlock_time = never\n");
        QTest::newRow("a minute") << QByteArray("unlock_time = 60\n");
        QTest::newRow("a few seconds") << QByteArray("unlock_time = 5\n");
    }
    void aLockNotSaidInMinutes() {
        QFETCH(const QByteArray, settings);
        const StandInFaillock faillock(settings);
        faillock.addFailure(QStringLiteral("guardian"), t);
        faillock.addFailure(QStringLiteral("guardian"), t);
        AccountLock lock;
        watch(lock, faillock, t);
        QCOMPARE(lock.state(), State::OneTryLeftNoTime);
    }

    void twoMinutesIsSaidInMinutes() {
        const StandInFaillock faillock("unlock_time = 61\n");
        faillock.addFailure(QStringLiteral("guardian"), t);
        faillock.addFailure(QStringLiteral("guardian"), t);
        AccountLock lock;
        watch(lock, faillock, t);
        QCOMPARE(lock.state(), State::OneTryLeft);
        QCOMPARE(lock.lockMinutes(), 2);
    }

    void aClockSetBackIsLockedForUnknownTime() {
        const StandInFaillock faillock;
        for (int i = 0; i < 3; ++i) {
            faillock.addFailure(QStringLiteral("guardian"), t);
        }
        AccountLock lock;
        watch(lock, faillock, t - 3600);
        QCOMPARE(lock.state(), State::LockedForUnknownTime);
        QVERIFY(lock.locked());
    }

    // Settings this copy of the rule does not follow mean "not sure".
    void unclearSettingsAreUnknown() {
        const StandInFaillock faillock("admin_group = wheel\n");
        for (int i = 0; i < 3; ++i) {
            faillock.addFailure(QStringLiteral("guardian"), t);
        }
        AccountLock lock;
        watch(lock, faillock, t);
        QCOMPARE(lock.state(), State::Unknown);
        QVERIFY(!lock.locked());
    }

    void aNameWithASlashIsUnknown() {
        const StandInFaillock faillock;
        AccountLock lock;
        watch(lock, faillock, t);
        lock.setLoginName(QStringLiteral("../guardian"));
        QCOMPARE(lock.state(), State::Unknown);
    }

    // Read only while there is something to count down: not at all once
    // the account is open.
    void itDoesNotReadAgainWhenOpen() {
        const StandInFaillock faillock;
        for (int i = 0; i < 3; ++i) {
            faillock.addFailure(QStringLiteral("guardian"), t);
        }
        AccountLock lock;
        watch(lock, faillock, t);
        const auto* timer = lock.findChild<QTimer*>();
        QVERIFY(timer != nullptr);
        QVERIFY(timer->isActive());
        lock.setFixedTime(at(t + 601));
        QCOMPARE(lock.state(), State::OpenAgain);
        QVERIFY(!timer->isActive());
        lock.setLoginName(QStringLiteral("ben"));
        QCOMPARE(lock.state(), State::Open);
        QVERIFY(!timer->isActive());
    }

    // With deny = 1 a lock that runs out leaves one try. The screen first
    // says the lock is over, rather than going quiet with the lock's
    // words still up.
    void aLockOfOneTryThatEndsIsOpenAgain() {
        const StandInFaillock faillock("deny = 1\nunlock_time = 600\n");
        faillock.addFailure(QStringLiteral("guardian"), t);
        AccountLock lock;
        watch(lock, faillock, t + 590);
        QCOMPARE(lock.state(), State::LockedUnderAMinute);
        lock.setFixedTime(at(t + 601));
        QCOMPARE(lock.state(), State::OpenAgain);
    }

    // A try sent while locked is remembered, so a lock that runs out
    // while PAM answers is still why that try failed.
    void itRemembersWhetherATryWasSentWhileLocked() {
        const StandInFaillock faillock;
        for (int i = 0; i < 3; ++i) {
            faillock.addFailure(QStringLiteral("guardian"), t);
        }
        AccountLock lock;
        watch(lock, faillock, t + 599);
        QVERIFY(!lock.lockedAtLastTry());
        lock.noteTry();
        QVERIFY(lock.lockedAtLastTry());
        lock.setFixedTime(at(t + 601));
        QVERIFY(lock.lockedAtLastTry());
        lock.noteTry();
        QVERIFY(!lock.lockedAtLastTry());
        lock.setFixedTime(at(t + 599));
        lock.noteTry();
        lock.setLoginName(QStringLiteral("ben"));
        QVERIFY(!lock.lockedAtLastTry());
    }

    // Choosing another account while faillock writes that account's
    // record never shows the last account's lock.
    void aBusyFirstReadIsNotTheLastAccountsLock() {
        const StandInFaillock faillock;
        for (int i = 0; i < 3; ++i) {
            faillock.addFailure(QStringLiteral("guardian"), t);
        }
        faillock.addFailure(QStringLiteral("ben"), t);
        AccountLock lock;
        watch(lock, faillock, t);
        QCOMPARE(lock.state(), State::Locked);
        const QByteArray path = (faillock.dir() + QStringLiteral("/ben")).toLocal8Bit();
        const int writer = open(path.constData(), O_RDWR | O_CLOEXEC);
        QVERIFY(writer >= 0);
        QCOMPARE(flock(writer, LOCK_EX), 0);
        const QSignalSpy changes(&lock, &AccountLock::stateChanged);
        lock.setLoginName(QStringLiteral("ben"));
        const State whileBusy = lock.state();
        const bool lockedWhileBusy = lock.locked();
        const int minutesWhileBusy = lock.minutesLeft();
        close(writer);
        QCOMPARE(whileBusy, State::Unknown);
        QVERIFY(!lockedWhileBusy);
        QCOMPARE(minutesWhileBusy, 0);
        QVERIFY(changes.count() >= 1);
        QTRY_COMPARE(lock.state(), State::Open);
    }

    // While faillock writes, the last reading stands.
    void whileFaillockWritesTheLastReadingStands() {
        const StandInFaillock faillock;
        for (int i = 0; i < 3; ++i) {
            faillock.addFailure(QStringLiteral("guardian"), t);
        }
        AccountLock lock;
        watch(lock, faillock, t);
        const QByteArray path = (faillock.dir() + QStringLiteral("/guardian")).toLocal8Bit();
        const int writer = open(path.constData(), O_RDWR | O_CLOEXEC);
        QVERIFY(writer >= 0);
        QCOMPARE(flock(writer, LOCK_EX), 0);
        lock.setFixedTime(at(t + 120));
        const State whileBusy = lock.state();
        const int minutesWhileBusy = lock.minutesLeft();
        close(writer);
        QCOMPARE(whileBusy, State::Locked);
        QCOMPARE(minutesWhileBusy, 10);
        lock.refresh();
        QCOMPARE(lock.minutesLeft(), 8);
    }
};

} // namespace

QTEST_GUILESS_MAIN(AccountLockTest)
#include "tst_accountlock.moc"
