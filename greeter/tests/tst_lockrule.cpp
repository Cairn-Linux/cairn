// SPDX-License-Identifier: Apache-2.0
#include "LockRule.h"

#include <QTest>

Q_DECLARE_METATYPE(FaillockSettings)
Q_DECLARE_METATYPE(LockRule::Kind)

namespace {

using Kind = LockRule::Kind;

// Cairn's settings, which are faillock's defaults: three wrong tries in 15
// minutes lock the account for 10.
FaillockSettings cairn() {
    return FaillockSettings{};
}

FaillockSettings with(int deny, qint64 failInterval, qint64 unlockTime) {
    FaillockSettings settings;
    settings.deny = deny;
    settings.failInterval = failInterval;
    settings.unlockTime = unlockTime;
    return settings;
}

// An hour into some day; the tries are counted from here.
constexpr qint64 t = 1'800'000'000;

class LockRuleTest : public QObject {
    Q_OBJECT

private slots:
    void theRule_data() {
        QTest::addColumn<QList<qint64>>("failures");
        QTest::addColumn<FaillockSettings>("settings");
        QTest::addColumn<qint64>("now");
        QTest::addColumn<Kind>("kind");
        QTest::addColumn<qint64>("secondsLeft");

        QTest::newRow("no wrong tries") << QList<qint64>{} << cairn() << t << Kind::Open << 0LL;
        QTest::newRow("one wrong try") << QList<qint64>{t} << cairn() << t + 5 << Kind::Open << 0LL;
        QTest::newRow("two of three: one try left")
            << QList<qint64>{t, t + 10} << cairn() << t + 20 << Kind::OneTryLeft << 0LL;
        QTest::newRow("three at once: locked for 10 minutes")
            << QList<qint64>{t, t + 1, t + 2} << cairn() << t + 2 << Kind::Locked << 600LL;
        QTest::newRow("the order in the file does not matter")
            << QList<qint64>{t + 2, t, t + 1} << cairn() << t + 2 << Kind::Locked << 600LL;
        QTest::newRow("a little later")
            << QList<qint64>{t, t + 1, t + 2} << cairn() << t + 302 << Kind::Locked << 300LL;
        // faillock still refuses at latest + unlock_time ...
        QTest::newRow("exactly at the unlock time: still locked")
            << QList<qint64>{t, t + 1, t + 2} << cairn() << t + 602 << Kind::Locked << 0LL;
        // ... and lets a try in the second after, when every old try is
        // forgotten at the next wrong one.
        QTest::newRow("a second after: open, all tries back")
            << QList<qint64>{t, t + 1, t + 2} << cairn() << t + 603 << Kind::Open << 0LL;
        QTest::newRow("long after")
            << QList<qint64>{t, t + 1, t + 2} << cairn() << t + 86400 << Kind::Open << 0LL;
        // The window is counted back from the newest try, not from now.
        QTest::newRow("three within 15 minutes of the newest")
            << QList<qint64>{t, t + 800, t + 899} << cairn() << t + 1000 << Kind::Locked << 499LL;
        QTest::newRow("three spread over more than 15 minutes")
            << QList<qint64>{t, t + 800, t + 900} << cairn() << t + 901 << Kind::OneTryLeft << 0LL;
        QTest::newRow("an old lock and one new try: only the new one counts")
            << QList<qint64>{t, t + 1, t + 2, t + 5000} << cairn() << t + 5001 << Kind::Open << 0LL;
        QTest::newRow("tries older than the window do not count")
            << QList<qint64>{t - 5000, t - 4000, t} << cairn() << t + 1 << Kind::Open << 0LL;
        // write_tally forgets tries older than the window counted back from
        // now before it adds the next, so the oldest is about to go.
        QTest::newRow("the oldest try is about to be forgotten")
            << QList<qint64>{t, t + 100} << cairn() << t + 950 << Kind::Open << 0LL;
        QTest::newRow("one try just inside the window from now")
            << QList<qint64>{t, t + 100} << cairn() << t + 899 << Kind::OneTryLeft << 0LL;
        QTest::newRow("deny 0 never locks") << QList<qint64>{t, t + 1, t + 2, t + 3}
                                            << with(0, 900, 600) << t + 4 << Kind::Open << 0LL;
        QTest::newRow("fail_interval 0 never locks")
            << QList<qint64>{t, t, t} << with(3, 0, 600) << t << Kind::Open << 0LL;
        QTest::newRow("deny 1: the first try is the last")
            << QList<qint64>{} << with(1, 900, 600) << t << Kind::OneTryLeft << 0LL;
        QTest::newRow("deny 1: one try locks")
            << QList<qint64>{t} << with(1, 900, 600) << t + 60 << Kind::Locked << 540LL;
        QTest::newRow("deny 1: after the lock, still one try")
            << QList<qint64>{t} << with(1, 900, 600) << t + 601 << Kind::OneTryLeft << 0LL;
        QTest::newRow("deny 5: three tries, two left")
            << QList<qint64>{t, t + 1, t + 2} << with(5, 900, 600) << t + 3 << Kind::Open << 0LL;
        QTest::newRow("deny 5: four tries, one left")
            << QList<qint64>{t, t + 1, t + 2, t + 3} << with(5, 900, 600) << t + 4
            << Kind::OneTryLeft << 0LL;
        QTest::newRow("unlock_time never") << QList<qint64>{t, t + 1, t + 2} << with(3, 900, 0)
                                           << t + 999999 << Kind::LockedUntilReset << 0LL;
        // A clock set back after the lock: the record is later than now.
        QTest::newRow("the newest try is later than now")
            << QList<qint64>{t, t + 1, t + 2} << cairn() << t - 3600 << Kind::LockedForUnknownTime
            << 0LL;
        QTest::newRow("a try later than now is about to be forgotten")
            << QList<qint64>{t - 10, t + 3600} << cairn() << t << Kind::Open << 0LL;
        QTest::newRow("more tries than deny, all in the window")
            << QList<qint64>{t, t + 1, t + 2, t + 3, t + 4} << cairn() << t + 4 << Kind::Locked
            << 600LL;
    }
    void theRule() {
        QFETCH(const QList<qint64>, failures);
        QFETCH(const FaillockSettings, settings);
        QFETCH(const qint64, now);
        QFETCH(const Kind, kind);
        QFETCH(const qint64, secondsLeft);
        const LockRule::Result result = LockRule::decide(failures, settings, now);
        QCOMPARE(result.kind, kind);
        QCOMPARE(result.secondsLeft, secondsLeft);
    }
};

} // namespace

QTEST_GUILESS_MAIN(LockRuleTest)
#include "tst_lockrule.moc"
