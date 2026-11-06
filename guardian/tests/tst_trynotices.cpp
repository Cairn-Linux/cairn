// SPDX-License-Identifier: Apache-2.0
#include "TryNotices.h"

#include <QTest>

#include <utility>

namespace {

constexpr qint64 second = 1000000;
constexpr qint64 minute = 60 * second;
// 2026-11-04 16:02 UTC.
constexpr qint64 start = 1793808120LL * second;

const Account guardian{.uid = 1000,
                       .groups = {"guardian", "wheel", "cairn-guardian"},
                       .login = "guardian",
                       .fullName = "Guardian"};
const Account ben{.uid = 1002, .groups = {"ben", "cairn-l3"}, .login = "ben", .fullName = "Ben"};

PasswordTry tryAt(qint64 time, TryPlace place = TryPlace::LoginScreen,
                  std::optional<Account> account = guardian, qint64 pid = 0) {
    PasswordTry found;
    found.time = time;
    found.account = std::move(account);
    found.place = place;
    found.pid = pid;
    return found;
}

GoodPassword goodAt(qint64 time, const Account& account, qint64 pid, bool accountPart) {
    GoodPassword good;
    good.time = time;
    good.account = account;
    good.pid = pid;
    good.accountPart = accountPart;
    return good;
}

LockEvent lockAt(qint64 time, TryPlace place, qint64 pid, const Account& account = guardian) {
    LockEvent lock;
    lock.time = time;
    lock.account = account;
    lock.place = place;
    lock.pid = pid;
    return lock;
}

class TryNoticesTest : public QObject {
    Q_OBJECT

private slots:
    // Tries each within fail_interval of the last are one notice.
    void triesCloseTogetherAreOneNotice() {
        TryNotices notices(FaillockSettings{});
        notices.addTry(tryAt(start, TryPlace::LoginScreen, guardian, 1));
        notices.addTry(tryAt(start + minute, TryPlace::LoginScreen, guardian, 2));
        notices.addTry(tryAt(start + 14 * minute, TryPlace::LoginScreen, guardian, 3));
        // More than 15 minutes after the last: another notice.
        notices.addTry(tryAt(start + 30 * minute, TryPlace::LoginScreen, guardian, 4));
        const auto shown = notices.notices().shown;
        QCOMPARE(shown.size(), 2U);
        QCOMPARE(shown[0].wrong, 1);
        QCOMPARE(shown[0].first, start + 30 * minute);
        QCOMPARE(shown[1].wrong, 3);
        QCOMPARE(shown[1].first, start);
        QCOMPARE(shown[1].last, start + 14 * minute);
        QVERIFY(!shown[1].lockTime.has_value());
    }

    // Another place, another account or another login is another notice.
    void eachPlaceAccountAndLoginOnItsOwn() {
        TryNotices notices(FaillockSettings{});
        notices.addTry(tryAt(start, TryPlace::LoginScreen));
        notices.addTry(tryAt(start + second, TryPlace::Sudo));
        notices.addTry(tryAt(start + 2 * second, TryPlace::LoginScreen, ben));
        notices.addTry(tryAt(start + 3 * second, TryPlace::LoginScreen, std::nullopt));
        PasswordTry fromBen = tryAt(start + 4 * second, TryPlace::Su);
        fromBen.from = ben;
        notices.addTry(fromBen);
        notices.addTry(tryAt(start + 5 * second, TryPlace::Su));
        QCOMPARE(notices.notices().shown.size(), 6U);
    }

    // sudo: faillock's record comes first, from the process whose try then
    // fails; the next tries are refused while locked.
    void aLockAndTriesWhileLocked() {
        TryNotices notices(FaillockSettings{});
        notices.addTry(tryAt(start, TryPlace::Sudo, guardian, 10));
        notices.addTry(tryAt(start + minute, TryPlace::Sudo, guardian, 11));
        notices.addLock(lockAt(start + 2 * minute, TryPlace::Sudo, 12));
        notices.addTry(tryAt(start + 2 * minute + 1000, TryPlace::Sudo, guardian, 12));
        notices.addTry(tryAt(start + 3 * minute, TryPlace::Sudo, guardian, 13));
        notices.addTry(tryAt(start + 4 * minute, TryPlace::Sudo, guardian, 14));
        // After the 10 minutes: wrong again, and a notice of its own, so
        // the lock goes with the tries that caused it.
        notices.addTry(tryAt(start + 13 * minute, TryPlace::Sudo, guardian, 15));
        const auto shown = notices.notices().shown;
        QCOMPARE(shown.size(), 2U);
        QCOMPARE(shown[0].wrong, 1);
        QCOMPARE(shown[0].whileLocked, 0);
        QVERIFY(!shown[0].lockTime.has_value());
        QCOMPARE(shown[1].wrong, 3);
        QCOMPARE(shown[1].whileLocked, 2);
        QCOMPARE(shown[1].lockTime, start + 2 * minute);
    }

    // sudo makes its three tries in one process. Two wrong tries at the
    // lock screen, then sudo's first try locks: that process writes the
    // lock, and its next two tries were refused while locked.
    void triesInTheProcessThatLocked() {
        TryNotices notices(FaillockSettings{});
        notices.addTry(tryAt(start, TryPlace::OwnLogin, guardian, 30));
        notices.addTry(tryAt(start + second, TryPlace::OwnLogin, guardian, 31));
        notices.addLock(lockAt(start + minute, TryPlace::Sudo, 50));
        notices.addTry(tryAt(start + minute + 1000, TryPlace::Sudo, guardian, 50));
        notices.addTry(tryAt(start + minute + 3 * second, TryPlace::Sudo, guardian, 50));
        notices.addTry(tryAt(start + minute + 9 * second, TryPlace::Sudo, guardian, 50));
        const auto shown = notices.notices().shown;
        QCOMPARE(shown.size(), 2U);
        QCOMPARE(shown[0].place, TryPlace::Sudo);
        QCOMPARE(shown[0].wrong, 1);
        QCOMPARE(shown[0].whileLocked, 2);
        QCOMPARE(shown[0].lockTime, start + minute);
        QCOMPARE(shown[1].wrong, 2);
    }

    // Locked at sudo, then tried at the login screen: a notice that only
    // has tries while locked.
    void triesWhileLockedElsewhere() {
        TryNotices notices(FaillockSettings{});
        notices.addLock(lockAt(start, TryPlace::Sudo, 12));
        notices.addTry(tryAt(start + 1000, TryPlace::Sudo, guardian, 12));
        notices.addTry(tryAt(start + minute, TryPlace::LoginScreen, guardian, 20));
        const auto shown = notices.notices().shown;
        QCOMPARE(shown.size(), 2U);
        QCOMPARE(shown[0].place, TryPlace::LoginScreen);
        QCOMPARE(shown[0].wrong, 0);
        QCOMPARE(shown[0].whileLocked, 1);
        QVERIFY(!shown[0].lockTime.has_value());
        QCOMPARE(shown[1].lockTime, start);
    }

    // The lock screen's lock comes from its own program, after the tries
    // unix_chkpwd recorded.
    void theLockScreensLockGoesWithItsTries() {
        TryNotices notices(FaillockSettings{});
        notices.addLock(lockAt(start + 2 * minute + second, TryPlace::OwnLogin, 4242));
        notices.addTry(tryAt(start, TryPlace::OwnLogin, guardian, 30));
        notices.addTry(tryAt(start + minute, TryPlace::OwnLogin, guardian, 31));
        notices.addTry(tryAt(start + 2 * minute, TryPlace::OwnLogin, guardian, 32));
        const auto shown = notices.notices().shown;
        QCOMPARE(shown.size(), 1U);
        QCOMPARE(shown[0].wrong, 3);
        QCOMPARE(shown[0].lockTime, start + 2 * minute + second);
    }

    // Tries refused while the lock screen's lock held keep the run of
    // tries open; the lock still goes with that run.
    void theLockScreensLockWithTriesAfterIt() {
        TryNotices notices(FaillockSettings{});
        notices.addLock(lockAt(start + 2 * minute + second, TryPlace::OwnLogin, 4242));
        notices.addTry(tryAt(start, TryPlace::OwnLogin, guardian, 30));
        notices.addTry(tryAt(start + minute, TryPlace::OwnLogin, guardian, 31));
        notices.addTry(tryAt(start + 2 * minute, TryPlace::OwnLogin, guardian, 32));
        notices.addTry(tryAt(start + 5 * minute, TryPlace::OwnLogin, guardian, 33));
        const auto shown = notices.notices().shown;
        QCOMPARE(shown.size(), 1U);
        QCOMPARE(shown[0].wrong, 3);
        QCOMPARE(shown[0].whileLocked, 1);
        QCOMPARE(shown[0].lockTime, start + 2 * minute + second);
    }

    // Wrong fingers leave no password try: the lock is a notice alone.
    void aLockWithNoTries() {
        TryNotices notices(FaillockSettings{});
        notices.addLock(lockAt(start, TryPlace::OwnLogin, 4242, ben));
        const auto shown = notices.notices().shown;
        QCOMPARE(shown.size(), 1U);
        QCOMPARE(shown[0].account->login, QStringLiteral("ben"));
        QCOMPARE(shown[0].wrong, 0);
        QCOMPARE(shown[0].lockTime, start);
    }

    // sddm writes faillock's line and its audit record; only the record
    // is read, and one lock never shows twice.
    void oneLockOnce() {
        TryNotices notices(FaillockSettings{});
        notices.addTry(tryAt(start, TryPlace::LoginScreen, guardian, 40));
        notices.addTry(tryAt(start + second, TryPlace::LoginScreen, guardian, 41));
        notices.addLock(lockAt(start + 2 * second, TryPlace::LoginScreen, 42));
        notices.addTry(tryAt(start + 2 * second + 10, TryPlace::LoginScreen, guardian, 42));
        const auto shown = notices.notices().shown;
        QCOMPARE(shown.size(), 1U);
        QCOMPARE(shown[0].wrong, 3);
        QCOMPARE(shown[0].whileLocked, 0);
    }

    // Without faillock's settings no try is called "while locked", and
    // with unlock_time "never" neither.
    void noSettingsNoWhileLocked() {
        FaillockSettings never;
        never.unlockTime = 0;
        for (const std::optional<FaillockSettings>& settings :
             {std::optional<FaillockSettings>(), std::optional<FaillockSettings>(never)}) {
            TryNotices notices(settings);
            notices.addLock(lockAt(start, TryPlace::Sudo, 1));
            notices.addTry(tryAt(start + second, TryPlace::Sudo, guardian, 2));
            QCOMPARE(notices.notices().shown[0].whileLocked, 0);
            QCOMPARE(notices.notices().shown[0].wrong, 1);
        }
    }

    // pam_faillock 1.7.2 locks without a word when the tries start within
    // fail_interval of a lock that ended by itself: its count of tries
    // takes in the old lock's records, passes deny without ever equalling
    // it, and neither the record nor the line is written (cairn-min,
    // 2026-11-05, 00:15). The page works the lock out from the tries, so
    // the three refused after it are never called wrong.
    void aLockFaillockDidNotRecord() {
        TryNotices notices(FaillockSettings{});
        notices.addTry(tryAt(start, TryPlace::Su, ben, 1));
        notices.addTry(tryAt(start + 3 * second, TryPlace::Su, ben, 2));
        notices.addLock(lockAt(start + 6 * second, TryPlace::Su, 3, ben));
        notices.addTry(tryAt(start + 6 * second, TryPlace::Su, ben, 3));
        // 49 seconds after that lock ended.
        const qint64 later = start + 6 * second + 10 * minute + 49 * second;
        for (int i = 0; i < 6; ++i) {
            notices.addTry(tryAt(later + i * 3 * second, TryPlace::Su, ben, 10 + i));
        }
        const auto shown = notices.notices().shown;
        QCOMPARE(shown.size(), 2U);
        QCOMPARE(shown[0].wrong, 3);
        QCOMPARE(shown[0].whileLocked, 3);
        QCOMPARE(shown[0].lockTime, later + 6 * second);
        QCOMPARE(shown[1].wrong, 3);
        QCOMPARE(shown[1].whileLocked, 0);
        QCOMPARE(shown[1].lockTime, start + 6 * second);
    }

    // The same rule across places (cairn-min, 2026-11-05, 23:16-23:28):
    // the lock screen locks, a wrong try after that lock ended, then sudo
    // locks silently at its second try and refuses its third.
    void aSilentLockAtSudoAfterTheLockScreen() {
        TryNotices notices(FaillockSettings{});
        notices.addLock(lockAt(start + 18 * second + 200, TryPlace::OwnLogin, 4242));
        notices.addTry(tryAt(start, TryPlace::OwnLogin, guardian, 30));
        notices.addTry(tryAt(start + 9 * second, TryPlace::OwnLogin, guardian, 31));
        notices.addTry(tryAt(start + 18 * second, TryPlace::OwnLogin, guardian, 32));
        notices.addTry(tryAt(start + 10 * minute + 53 * second, TryPlace::OwnLogin, guardian, 33));
        const qint64 sudo = start + 12 * minute + 19 * second;
        notices.addTry(tryAt(sudo, TryPlace::Sudo, guardian, 50));
        notices.addTry(tryAt(sudo + 2 * second, TryPlace::Sudo, guardian, 50));
        notices.addTry(tryAt(sudo + 5 * second, TryPlace::Sudo, guardian, 50));
        const auto shown = notices.notices().shown;
        QCOMPARE(shown.size(), 3U);
        QCOMPARE(shown[0].place, TryPlace::Sudo);
        QCOMPARE(shown[0].wrong, 2);
        QCOMPARE(shown[0].whileLocked, 1);
        QCOMPARE(shown[0].lockTime, sudo + 2 * second);
        QCOMPARE(shown[1].place, TryPlace::OwnLogin);
        QCOMPARE(shown[1].wrong, 1);
        QVERIFY(!shown[1].lockTime.has_value());
        QCOMPARE(shown[2].wrong, 3);
        QCOMPARE(shown[2].lockTime, start + 18 * second + 200);
    }

    // Someone ran faillock --reset, which the journal does not show, and
    // three tries in a password box locked again inside the first lock's
    // ten minutes. faillock locks only an account that is not locked, so
    // those tries were wrong passwords, not tries refused while locked.
    void aLockInsideALockMeansAReset() {
        TryNotices notices(FaillockSettings{});
        notices.addTry(tryAt(start, TryPlace::LoginScreen, guardian, 1));
        notices.addTry(tryAt(start + 7 * second, TryPlace::LoginScreen, guardian, 2));
        notices.addLock(lockAt(start + 15 * second, TryPlace::LoginScreen, 3));
        notices.addTry(tryAt(start + 15 * second, TryPlace::LoginScreen, guardian, 3));
        const qint64 box = start + 5 * minute;
        notices.addTry(tryAt(box, TryPlace::PasswordBox, guardian, 20));
        notices.addTry(tryAt(box + 8 * second, TryPlace::PasswordBox, guardian, 21));
        notices.addLock(lockAt(box + 16 * second, TryPlace::PasswordBox, 22));
        notices.addTry(tryAt(box + 16 * second, TryPlace::PasswordBox, guardian, 22));
        const auto shown = notices.notices().shown;
        QCOMPARE(shown.size(), 2U);
        QCOMPARE(shown[0].place, TryPlace::PasswordBox);
        QCOMPARE(shown[0].wrong, 3);
        QCOMPARE(shown[0].whileLocked, 0);
        QCOMPARE(shown[0].lockTime, box + 16 * second);
        QCOMPARE(shown[1].wrong, 3);
    }

    // Two locks from one place, the second less than fail_interval after
    // the first run of tries: each lock goes with the tries that caused
    // it, never a notice of its own beside them.
    void twoLocksFromOnePlace() {
        TryNotices notices(FaillockSettings{});
        for (const qint64 at : {start, start + 11 * minute}) {
            notices.addTry(tryAt(at, TryPlace::Su, guardian, at / second));
            notices.addTry(tryAt(at + second, TryPlace::Su, guardian, at / second + 1));
            notices.addLock(lockAt(at + 2 * second, TryPlace::Su, at / second + 2));
            notices.addTry(tryAt(at + 2 * second, TryPlace::Su, guardian, at / second + 2));
        }
        const auto shown = notices.notices().shown;
        QCOMPARE(shown.size(), 2U);
        QCOMPARE(shown[0].wrong, 3);
        QCOMPARE(shown[0].lockTime, start + 11 * minute + 2 * second);
        QCOMPARE(shown[1].wrong, 3);
        QCOMPARE(shown[1].lockTime, start + 2 * second);
    }

    // A flood of tries is counted, not kept: one notice for a burst, and
    // only the newest notices when there are very many.
    void aFlood() {
        TryNotices notices(FaillockSettings{}, 5);
        for (int i = 0; i < 100000; ++i) {
            notices.addTry(tryAt(start + i * 1000, TryPlace::Su, guardian, i + 1));
        }
        QCOMPARE(notices.notices().shown.size(), 1U);
        QCOMPARE(notices.notices().shown[0].wrong, 100000);
        for (int i = 0; i < 8; ++i) {
            notices.addTry(tryAt(start + (i + 1) * 20 * minute, TryPlace::Su, ben, i + 1));
        }
        const auto result = notices.notices();
        QCOMPARE(result.shown.size(), 5U);
        QCOMPARE(result.notShown, 4);
        QCOMPARE(result.shown[0].first, start + 8 * 20 * minute);
    }

    // A child can fake the lock screen's line about their own account as
    // often as they like. Each is settled without looking through the
    // others, so the page does not freeze; one within moments of the lock
    // before it is that lock said again, and others are notices alone.
    void aFloodOfLocks() {
        TryNotices notices(FaillockSettings{}, 5);
        for (int i = 0; i < 100000; ++i) {
            notices.addLock(lockAt(start + i * 1000, TryPlace::OwnLogin, 4242, ben));
        }
        notices.addTry(tryAt(start, TryPlace::Su, ben, 1));
        const auto result = notices.notices();
        QCOMPARE(result.shown.size(), 5U);
        QVERIFY(result.notShown > 0);
        // A minute apart, each is a lock of its own, inside the one before:
        // the count was emptied between, as far as the page can tell.
        TryNotices apart(FaillockSettings{}, 100000);
        for (int i = 0; i < 1000; ++i) {
            apart.addLock(lockAt(start + i * minute, TryPlace::OwnLogin, 4242, ben));
        }
        QCOMPARE(apart.notices().shown.size(), 1000U);
    }

    // A good login, where faillock's account part ran after the password,
    // empties the count: the tries after it start again from none, so no
    // lock is worked out from tries on both sides of it.
    void aGoodLoginEmptiesTheCount() {
        TryNotices notices(FaillockSettings{});
        notices.addTry(tryAt(start, TryPlace::Su, ben, 1));
        notices.addTry(tryAt(start + second, TryPlace::Su, ben, 2));
        notices.addLock(lockAt(start + 2 * second, TryPlace::Su, 3, ben));
        notices.addTry(tryAt(start + 2 * second, TryPlace::Su, ben, 3));
        // The lock ends; one wrong, a good login, two wrong.
        const qint64 later = start + 11 * minute;
        notices.addTry(tryAt(later, TryPlace::LoginScreen, ben, 10));
        notices.addGood(goodAt(later + minute, ben, 11, false));
        notices.addGood(goodAt(later + minute + 1000, ben, 11, true));
        notices.addTry(tryAt(later + 2 * minute, TryPlace::LoginScreen, ben, 12));
        notices.addTry(tryAt(later + 3 * minute, TryPlace::LoginScreen, ben, 13));
        auto shown = notices.notices().shown;
        QCOMPARE(shown.size(), 2U);
        QCOMPARE(shown[0].wrong, 3);
        QVERIFY(!shown[0].lockTime.has_value());
        // The account part alone, as for sudo with no password asked, or
        // from another process, empties nothing.
        TryNotices notEmptied(FaillockSettings{});
        notEmptied.addTry(tryAt(start, TryPlace::Su, ben, 1));
        notEmptied.addGood(goodAt(start + second, ben, 5, false));
        notEmptied.addGood(goodAt(start + 2 * second, ben, 6, true));
        notEmptied.addTry(tryAt(start + 3 * second, TryPlace::Su, ben, 7));
        notEmptied.addLock(lockAt(start + 4 * second, TryPlace::Su, 8, ben));
        notEmptied.addTry(tryAt(start + 4 * second, TryPlace::Su, ben, 8));
        shown = notEmptied.notices().shown;
        QCOMPARE(shown.size(), 1U);
        QCOMPARE(shown[0].wrong, 3);
        QCOMPARE(shown[0].lockTime, start + 4 * second);
    }

    // A right password while the count says locked: someone emptied it
    // out of sight (cairn-min, 2026-11-05, 22:55: the login screen locked,
    // the right password was refused, faillock --reset, the right password
    // again). When is not known, so the try before stays refused; the
    // count starts again from none.
    void aRightPasswordWhileCountedLocked() {
        TryNotices notices(FaillockSettings{});
        notices.addTry(tryAt(start, TryPlace::LoginScreen, guardian, 1));
        notices.addTry(tryAt(start + 7 * second, TryPlace::LoginScreen, guardian, 2));
        notices.addLock(lockAt(start + 15 * second, TryPlace::LoginScreen, 3));
        notices.addTry(tryAt(start + 15 * second, TryPlace::LoginScreen, guardian, 3));
        notices.addTry(tryAt(start + 35 * second, TryPlace::LoginScreen, guardian, 4));
        notices.addGood(goodAt(start + 42 * second, guardian, 5, false));
        notices.addGood(goodAt(start + 42 * second + 2000, guardian, 5, true));
        notices.addTry(tryAt(start + minute, TryPlace::Sudo, guardian, 6));
        const auto shown = notices.notices().shown;
        QCOMPARE(shown.size(), 2U);
        QCOMPARE(shown[0].place, TryPlace::Sudo);
        QCOMPARE(shown[0].wrong, 1);
        QCOMPARE(shown[0].whileLocked, 0);
        QCOMPARE(shown[1].wrong, 3);
        QCOMPARE(shown[1].whileLocked, 1);
    }

    // A lock inside a lock needs only deny less one wrong tries before the
    // one that locked; older tries refused while locked stay refused, and
    // those counted wrong after all start a notice of their own after the
    // lock's notice (cairn-min, 2026-11-06, 00:04: three wrong su locked,
    // faillock --reset, three wrong su locked again).
    void onlyTheTriesALockNeeds() {
        TryNotices notices(FaillockSettings{});
        for (int i = 0; i < 3; ++i) {
            if (i == 2) {
                notices.addLock(lockAt(start + i * 3 * second, TryPlace::Su, 1 + i, ben));
            }
            notices.addTry(tryAt(start + i * 3 * second, TryPlace::Su, ben, 1 + i));
        }
        // Refused while locked, then the count emptied, then three more.
        notices.addTry(tryAt(start + 10 * second, TryPlace::Su, ben, 10));
        for (int i = 0; i < 3; ++i) {
            const qint64 at = start + 20 * second + i * 3 * second;
            if (i == 2) {
                notices.addLock(lockAt(at, TryPlace::Su, 20 + i, ben));
            }
            notices.addTry(tryAt(at, TryPlace::Su, ben, 20 + i));
        }
        const auto shown = notices.notices().shown;
        QCOMPARE(shown.size(), 2U);
        QCOMPARE(shown[0].wrong, 3);
        QCOMPARE(shown[0].whileLocked, 0);
        QCOMPARE(shown[0].first, start + 20 * second);
        QCOMPARE(shown[0].lockTime, start + 26 * second);
        QCOMPARE(shown[1].wrong, 3);
        QCOMPARE(shown[1].whileLocked, 1);
        QCOMPARE(shown[1].lockTime, start + 6 * second);
    }

    // pam_faillock 1.7.2 can also say it locked when, by its own rule, it
    // did not: write_tally's count takes in a record an ended lock left
    // behind, while check_tally's does not. The lock then comes one try
    // later, without a word.
    void aLockFaillockSaidButDidNotMake() {
        TryNotices notices(FaillockSettings{});
        notices.addTry(tryAt(start, TryPlace::Su, ben, 1));
        notices.addTry(tryAt(start + 10 * second, TryPlace::Su, ben, 2));
        notices.addLock(lockAt(start + 20 * second, TryPlace::Su, 3, ben));
        notices.addTry(tryAt(start + 20 * second, TryPlace::Su, ben, 3));
        const qint64 lock = start + 20 * second;
        notices.addTry(tryAt(lock + 610 * second, TryPlace::Su, ben, 10));
        // The first try's record is fail_interval old; the lock's is not.
        notices.addLock(lockAt(lock + 895 * second, TryPlace::Su, 11, ben));
        notices.addTry(tryAt(lock + 895 * second, TryPlace::Su, ben, 11));
        notices.addTry(tryAt(lock + 896 * second, TryPlace::Su, ben, 12));
        notices.addTry(tryAt(lock + 897 * second, TryPlace::Su, ben, 13));
        const auto shown = notices.notices().shown;
        QCOMPARE(shown.size(), 2U);
        QCOMPARE(shown[0].wrong, 3);
        QCOMPARE(shown[0].whileLocked, 1);
        QCOMPARE(shown[0].lockTime, lock + 896 * second);
    }
};

} // namespace

QTEST_GUILESS_MAIN(TryNoticesTest)
#include "tst_trynotices.moc"
