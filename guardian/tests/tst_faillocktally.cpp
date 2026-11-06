// SPDX-License-Identifier: Apache-2.0
#include "FaillockTally.h"

#include <QTest>

namespace {

constexpr qint64 second = 1000000;
constexpr qint64 start = 1793808120LL * second;

using Outcome = FaillockTally::Outcome;

// Three wrong tries ten seconds apart, the last locking: faillock's
// defaults, deny 3, fail_interval 15 minutes, unlock_time 10 minutes.
FaillockTally lockedAtStart() {
    FaillockTally tally;
    tally.fail(start - 20 * second);
    tally.fail(start - 10 * second);
    tally.fail(start);
    return tally;
}

class FaillockTallyTest : public QObject {
    Q_OBJECT

private slots:
    void threeWrongLock() {
        FaillockTally tally;
        QCOMPARE(tally.fail(start), Outcome::Wrong);
        QCOMPARE(tally.fail(start + second), Outcome::Wrong);
        QVERIFY(!tally.refuses(start + 2 * second));
        QCOMPARE(tally.fail(start + 2 * second), Outcome::Locked);
        QVERIFY(tally.refuses(start + 3 * second));
        QCOMPARE(tally.lockStart(), start + 2 * second);
        QCOMPARE(tally.fail(start + 3 * second), Outcome::Refused);
        // unlock_time after the newest record, and not a second before.
        QVERIFY(tally.refuses(start + 602 * second));
        QVERIFY(!tally.refuses(start + 603 * second));
    }

    // Tries further apart than fail_interval never add up.
    void triesFarApart() {
        FaillockTally tally;
        for (int i = 0; i < 10; ++i) {
            QCOMPARE(tally.fail(start + i * 900 * second), Outcome::Wrong);
        }
    }

    // pam 1.7.2: after a lock ends by itself, its records stay in the file,
    // no longer valid, and write_tally still counts them while they are
    // under fail_interval old. Its count passes deny without equalling it.
    void aSilentLockAfterAnEndedLock() {
        FaillockTally tally = lockedAtStart();
        const qint64 later = start + 649 * second;
        QCOMPARE(tally.fail(later), Outcome::Wrong);
        QCOMPARE(tally.fail(later + 3 * second), Outcome::Wrong);
        QCOMPARE(tally.fail(later + 6 * second), Outcome::SilentlyLocked);
        QCOMPARE(tally.fail(later + 9 * second), Outcome::Refused);
    }

    // Once the old records are fail_interval old, a lock is said again.
    void aLockLongAfter() {
        FaillockTally tally = lockedAtStart();
        const qint64 later = start + 3600 * second;
        QCOMPARE(tally.fail(later), Outcome::Wrong);
        QCOMPARE(tally.fail(later + second), Outcome::Wrong);
        QCOMPARE(tally.fail(later + 2 * second), Outcome::Locked);
    }

    // And the other way round: one old record still counts where another
    // no longer does, so write_tally's count equals deny one try early,
    // and the lock itself comes, silently, at the next.
    void announcedOneTryEarly() {
        FaillockTally tally = lockedAtStart();
        QCOMPARE(tally.fail(start + 610 * second), Outcome::Wrong);
        QCOMPARE(tally.fail(start + 891 * second), Outcome::AnnouncedOnly);
        QVERIFY(!tally.refuses(start + 891 * second));
        QCOMPARE(tally.fail(start + 892 * second), Outcome::SilentlyLocked);
    }

    void aResetEmpties() {
        FaillockTally tally = lockedAtStart();
        tally.reset();
        QVERIFY(!tally.refuses(start + second));
        QCOMPARE(tally.fail(start + second), Outcome::Wrong);
        QCOMPARE(tally.fail(start + 2 * second), Outcome::Wrong);
        QCOMPARE(tally.fail(start + 3 * second), Outcome::Locked);
    }

    // A lock the journal shows after tries this count did not see.
    void aLockFromTriesNotSeen() {
        FaillockTally tally;
        tally.fail(start);
        tally.lockAt(start + 5 * second);
        QVERIFY(tally.refuses(start + 6 * second));
        QCOMPARE(tally.lockStart(), start + 5 * second);
        tally.lockAt(start + 6 * second);
        QVERIFY(tally.refuses(start + 7 * second));
        // An earlier time than a record there still locks, and ends.
        FaillockTally earlier;
        earlier.fail(start + 1000 * second);
        earlier.lockAt(start);
        QVERIFY(earlier.refuses(start + 1001 * second));
    }

    // unlock_time "never": locked until someone resets the count. deny 0:
    // never locked.
    void neverAndNoDeny() {
        FaillockSettings never;
        never.unlockTime = 0;
        FaillockTally untilReset(never);
        untilReset.fail(start);
        untilReset.fail(start + second);
        QCOMPARE(untilReset.fail(start + 2 * second), Outcome::Locked);
        QVERIFY(untilReset.refuses(start + 86400 * second));
        FaillockSettings noDeny;
        noDeny.deny = 0;
        FaillockTally open(noDeny);
        for (int i = 0; i < 10; ++i) {
            QCOMPARE(open.fail(start + i * second), Outcome::Wrong);
        }
        open.lockAt(start + 20 * second);
        QVERIFY(!open.refuses(start + 21 * second));
    }
};

} // namespace

QTEST_GUILESS_MAIN(FaillockTallyTest)
#include "tst_faillocktally.moc"
