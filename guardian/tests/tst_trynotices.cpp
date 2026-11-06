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
        // After the 10 minutes: wrong again.
        notices.addTry(tryAt(start + 13 * minute, TryPlace::Sudo, guardian, 15));
        const auto shown = notices.notices().shown;
        QCOMPARE(shown.size(), 1U);
        QCOMPARE(shown[0].wrong, 4);
        QCOMPARE(shown[0].whileLocked, 2);
        QCOMPARE(shown[0].lockTime, start + 2 * minute);
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
};

} // namespace

QTEST_GUILESS_MAIN(TryNoticesTest)
#include "tst_trynotices.moc"
