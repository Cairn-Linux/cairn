// SPDX-License-Identifier: Apache-2.0
#include "NoticeWords.h"

#include <QTest>

#include <utility>

namespace {

constexpr qint64 second = 1000000;
constexpr qint64 minute = 60 * second;
// Wednesday, 4 November 2026, 16:02 UTC.
constexpr qint64 start = 1793808120LL * second;

const Account guardian{.uid = 1000,
                       .groups = {"guardian", "wheel", "cairn-guardian"},
                       .login = "guardian",
                       .fullName = "Guardian"};
const Account sam{
    .uid = 1003, .groups = {"sam", "wheel", "cairn-guardian"}, .login = "sam", .fullName = "Sam"};
const Account ada{.uid = 1001, .groups = {"ada", "cairn-l1"}, .login = "ada", .fullName = "Ada"};
const Account ben{.uid = 1002, .groups = {"ben", "cairn-l3"}, .login = "ben", .fullName = "Ben"};
const Account installer{
    .uid = 999, .groups = {"bazzite", "wheel"}, .login = "bazzite", .fullName = "cairn"};

// Words for the Guardian, with faillock's defaults, in British English and
// UTC so every date reads the same on any machine.
NoticeWords words(std::optional<FaillockSettings> settings = FaillockSettings{}) {
    return {1000, settings, QTimeZone::utc(), QLocale(QLocale::English, QLocale::UnitedKingdom)};
}

TryEpisode episode(std::optional<Account> account, TryPlace place, int wrong,
                   qint64 last = start + 3 * minute) {
    TryEpisode made;
    made.account = std::move(account);
    made.place = place;
    made.first = start;
    made.last = last;
    made.wrong = wrong;
    return made;
}

class NoticeWordsTest : public QObject {
    Q_OBJECT

private slots:
    void yourOwnPassword() {
        QCOMPARE(words().sentence(episode(guardian, TryPlace::LoginScreen, 3)),
                 QStringLiteral("Your password was typed wrong 3 times at the login screen, on "
                                "Wednesday, 4 November 2026, from 16:02 to 16:05."));
    }

    void onceAndTwice() {
        QCOMPARE(words().sentence(episode(sam, TryPlace::Sudo, 1, start)),
                 QStringLiteral(
                     "The password of Sam (sam) was typed wrong once with sudo, on Wednesday, 4 "
                     "November 2026, at 16:02."));
        QCOMPARE(
            words().sentence(episode(ben, TryPlace::PasswordBox, 2)),
            QStringLiteral("The password of Ben (ben) was typed wrong twice in a password box, on "
                           "Wednesday, 4 November 2026, from 16:02 to 16:05."));
    }

    void anAccountOutsideTheFamily() {
        QCOMPARE(words().sentence(episode(installer, TryPlace::Console, 1, start)),
                 QStringLiteral("The password of the account bazzite was typed wrong once at a "
                                "text console, on Wednesday, 4 November 2026, at 16:02."));
    }

    void aNameThatIsNoAccount() {
        QCOMPARE(
            words().sentence(episode(std::nullopt, TryPlace::Su, 4)),
            QStringLiteral("The password of a login name that is not an account on this "
                           "computer was typed wrong 4 times with su, on Wednesday, 4 November "
                           "2026, from 16:02 to 16:05."));
    }

    // unix_chkpwd checks its caller's own password: the lock screen, or
    // anything else run in that login, a child's own program too.
    void inTheirOwnLogin() {
        QCOMPARE(words().sentence(episode(ben, TryPlace::OwnLogin, 3)),
                 QStringLiteral(
                     "The password of Ben (ben) was typed wrong 3 times in their own login, at the "
                     "lock screen or in another program, on Wednesday, 4 November 2026, "
                     "from 16:02 to 16:05."));
        QVERIFY(words()
                    .sentence(episode(guardian, TryPlace::OwnLogin, 3))
                    .contains(QStringLiteral("in your own login, at the lock screen")));
    }

    void fromAnotherLogin() {
        TryEpisode fromBen = episode(guardian, TryPlace::Su, 3);
        fromBen.from = ben;
        QCOMPARE(
            words().sentence(fromBen),
            QStringLiteral("Your password was typed wrong 3 times with su, from the login of Ben "
                           "(ben), on Wednesday, 4 November 2026, from 16:02 to 16:05."));
        Account gone;
        gone.uid = 1500;
        fromBen.from = gone;
        QVERIFY(words().sentence(fromBen).contains(QStringLiteral("with su, from another login,")));
        TryEpisode fromMe = episode(ben, TryPlace::Su, 1, start);
        fromMe.from = guardian;
        QVERIFY(words().sentence(fromMe).contains(QStringLiteral("with su, from your login,")));
    }

    void anotherProgram() {
        TryEpisode other = episode(guardian, TryPlace::Other, 1, start);
        QVERIFY(words().sentence(other).contains(QStringLiteral(" once by another program, on")));
        other.program = QStringLiteral("sshd-session");
        QVERIFY(words().sentence(other).contains(
            QStringLiteral(" once by another program (sshd-session), on")));
    }

    // A child at level 1 or 2 has no password, so no try is "wrong".
    void aChildWithNoPassword() {
        TryEpisode tried = episode(ada, TryPlace::Su, 3);
        tried.from = ben;
        QCOMPARE(words().sentence(tried),
                 QStringLiteral("Someone tried to use the account of Ada (ada) 3 times with su, "
                                "from the login of Ben (ben), on Wednesday, 4 November 2026, from "
                                "16:02 to 16:05."));
    }

    void aLock() {
        TryEpisode locked = episode(guardian, TryPlace::LoginScreen, 3, start + 5 * minute);
        locked.lockTime = start + 3 * minute;
        locked.whileLocked = 2;
        QCOMPARE(
            words().sentence(locked),
            QStringLiteral("Your password was typed wrong 3 times at the login screen, on "
                           "Wednesday, 4 November 2026, from 16:02 to 16:07. That locked the "
                           "account for 10 minutes, at every password prompt. The right password "
                           "did not work until about 16:15. It was tried twice more while it was "
                           "locked."));
    }

    // "That" is the wrong passwords, so a notice with none never says a
    // lock came of it, whatever it was given (cairn-min, 2026-11-05: "tried
    // 3 times ... while it was locked ... That locked the account").
    void noLockFromTriesRefusedWhileLocked() {
        TryEpisode refused = episode(guardian, TryPlace::PasswordBox, 0);
        refused.whileLocked = 3;
        refused.lockTime = start + 3 * minute;
        const QString words3 = words().sentence(refused);
        QCOMPARE(words3,
                 QStringLiteral("Your account was tried 3 times in a password box while it was "
                                "locked, on Wednesday, 4 November 2026, from 16:02 to 16:05."));
        QVERIFY(!words3.contains(QStringLiteral("That locked")));
    }

    // Fewer wrong passwords here than faillock needs: the rest came from
    // elsewhere, or were fingers.
    void aLockWithFewerTries() {
        TryEpisode locked = episode(guardian, TryPlace::OwnLogin, 1, start);
        locked.lockTime = start;
        QVERIFY(words().sentence(locked).endsWith(
            QStringLiteral("until about 16:12. Wrong tries at other prompts, and wrong fingers at "
                           "the lock screen, count toward a lock too.")));
    }

    // The words on where the rest of the lock came from follow the lock,
    // before the tries refused while it held.
    void aLockWithFewerTriesThenRefused() {
        TryEpisode locked = episode(guardian, TryPlace::OwnLogin, 1, start);
        locked.lockTime = start;
        locked.whileLocked = 1;
        QVERIFY(words().sentence(locked).endsWith(
            QStringLiteral("until about 16:12. Wrong tries at other prompts, and wrong fingers at "
                           "the lock screen, count toward a lock too. It was tried once more "
                           "while it was locked.")));
    }

    // Without a lock of its own, the tries were refused under a lock from
    // elsewhere, which may have ended before the wrong ones: the words do
    // not say they came after.
    void wrongAndRefusedWithoutALock() {
        TryEpisode mixed = episode(guardian, TryPlace::Sudo, 1);
        mixed.whileLocked = 2;
        QVERIFY(words().sentence(mixed).endsWith(
            QStringLiteral("In that time it was also tried twice while the account was locked.")));
    }

    void aLockAlone() {
        TryEpisode alone = episode(ben, TryPlace::OwnLogin, 0, start);
        alone.lockTime = start;
        QCOMPARE(
            words().sentence(alone),
            QStringLiteral(
                "The account of Ben (ben) was locked in their own login, at the lock screen or "
                "in another program, on Wednesday, 4 November 2026, at 16:02. That "
                "locked the account for 10 minutes, at every password prompt. The "
                "right password did not work until about 16:12. Wrong tries at "
                "other prompts, and wrong fingers at the lock screen, count toward "
                "a lock too."));
    }

    // Tries refused while locked are never called wrong.
    void onlyTriesWhileLocked() {
        TryEpisode refused = episode(guardian, TryPlace::LoginScreen, 0);
        refused.whileLocked = 3;
        QCOMPARE(words().sentence(refused),
                 QStringLiteral("Your account was tried 3 times at the login screen while it was "
                                "locked, on Wednesday, 4 November 2026, from 16:02 to 16:05."));
    }

    void lockSettingsUnknownOrNever() {
        TryEpisode locked = episode(guardian, TryPlace::Sudo, 3);
        locked.lockTime = start + 3 * minute;
        QVERIFY(words(std::nullopt)
                    .sentence(locked)
                    .endsWith(QStringLiteral(
                        "That locked the account for a while, at every password prompt.")));
        FaillockSettings never;
        never.unlockTime = 0;
        QVERIFY(words(never).sentence(locked).endsWith(
            QStringLiteral("until someone with an administrator's account reset its count of "
                           "wrong tries.")));
    }

    void overTwoDays() {
        const TryEpisode late = episode(guardian, TryPlace::Sudo, 5, start + 8 * 60 * minute);
        QVERIFY(words().sentence(late).endsWith(QStringLiteral(
            "from Wednesday, 4 November 2026 at 16:02 to Thursday, 5 November 2026 at 00:02.")));
    }

    // A name is put in once, never read again for %1.
    void aNameWithAPercentSign() {
        Account odd = ben;
        odd.fullName = QStringLiteral("%2 %1");
        QVERIFY(words()
                    .sentence(episode(odd, TryPlace::Su, 3))
                    .startsWith(QStringLiteral(
                        "The password of %2 %1 (ben) was typed wrong 3 times with su")));
    }

    // A child can set their own full name with no password (ADR-0043), so
    // a full name is never shown without the login name an administrator
    // gave, and cannot hide or turn it round.
    void aChildCallingThemselvesSomeoneElse() {
        Account mum = ben;
        mum.fullName = QStringLiteral("Mum");
        TryEpisode fromMum = episode(guardian, TryPlace::Su, 1, start);
        fromMum.from = mum;
        QVERIFY(words().sentence(fromMum).contains(
            QStringLiteral("with su, from the login of Mum (ben), on")));
        fromMum.from->fullName =
            QStringLiteral("Mum") + QChar(0x202e) + QChar(0x0a) + QChar(0x2066);
        QVERIFY(words().sentence(fromMum).contains(
            QStringLiteral("with su, from the login of Mum (ben), on")));
        fromMum.from->fullName = QString(QChar(0x202e));
        QVERIFY(words().sentence(fromMum).contains(
            QStringLiteral("with su, from the login of ben, on")));
        Account plain = ben;
        plain.fullName = QStringLiteral("ben");
        QVERIFY(words()
                    .sentence(episode(plain, TryPlace::Su, 1, start))
                    .startsWith(QStringLiteral("The password of ben was typed wrong once")));
    }

    void theFooter() {
        QCOMPARE(words().footer(),
                 QStringLiteral("3 wrong tries within 15 minutes, by password or by finger, lock "
                                "an account for 10 minutes. If this was you, there is nothing to "
                                "do. If it was not, someone may be guessing: a password nobody has "
                                "watched you type is the best protection. Children do not see "
                                "this page."));
        QVERIFY(words(std::nullopt).footer().startsWith(QStringLiteral("A few wrong tries")));
    }

    void theRecordStart() {
        const qint64 day = 24 * 60 * minute;
        QCOMPARE(words().recordStart(start - 31 * day, start, 30), QString());
        QCOMPARE(
            words().recordStart(start - 3 * day, start, 30),
            QStringLiteral("This computer's record goes back only to Sunday, 1 November 2026."));
    }

    void notShown() {
        QCOMPARE(words().notShown(0), QString());
        QCOMPARE(words().notShown(1), QStringLiteral("One older notice is not shown."));
        QCOMPARE(words().notShown(1200), QStringLiteral("1,200 older notices are not shown."));
    }
};

} // namespace

QTEST_GUILESS_MAIN(NoticeWordsTest)
#include "tst_noticewords.moc"
