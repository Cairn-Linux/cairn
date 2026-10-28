// SPDX-License-Identifier: Apache-2.0
#include "Caller.h"

#include <QTest>

namespace {

std::optional<Account> account(unsigned int uid) {
    switch (uid) {
    case 1001:
        return Account{.uid = 1001, .groups = {"gina", "wheel"}, .login = "gina", .fullName = {}};
    case 1005:
        return Account{
            .uid = 1005, .groups = {"dad", "cairn-guardian"}, .login = "dad", .fullName = {}};
    case 1002:
        return Account{.uid = 1002, .groups = {"ada", "cairn-l1"}, .login = "ada", .fullName = {}};
    case 1006:
        return Account{
            .uid = 1006, .groups = {"lodger", "users"}, .login = "lodger", .fullName = {}};
    default:
        return std::nullopt;
    }
}

std::optional<QString> home(unsigned int uid) {
    return QStringLiteral("/var/home/user%1").arg(uid);
}

Caller check(unsigned int euid, const std::optional<QString>& pkexecUid) {
    return Caller::check(euid, pkexecUid, account, home);
}

class CallerTest : public QObject {
    Q_OBJECT

private slots:
    void notRoot() {
        QCOMPARE(check(1001, std::nullopt).refusal, std::optional(Refusal::NotRoot));
        QCOMPARE(check(1001, QStringLiteral("1001")).refusal, std::optional(Refusal::NotRoot));
    }

    // Under sudo there is no grown-up to read pictures from.
    void sudo() {
        const Caller caller = check(0, std::nullopt);
        QVERIFY(!caller.refusal.has_value());
        QVERIFY(!caller.home.has_value());
    }

    // An administrator, by either group, with the home the database gives.
    void aGrownUp() {
        for (const unsigned int uid : {1001U, 1005U}) {
            const Caller caller = check(0, QString::number(uid));
            QVERIFY(!caller.refusal.has_value());
            QCOMPARE(caller.home, home(uid));
        }
    }

    void notAGrownUp_data() {
        QTest::addColumn<QString>("pkexecUid");
        QTest::newRow("a child") << "1002";
        QTest::newRow("another account") << "1006";
        QTest::newRow("no such account") << "4000";
        QTest::newRow("empty") << "";
        QTest::newRow("a name") << "gina";
        QTest::newRow("a sign") << "+1001";
        QTest::newRow("a space") << " 1001";
        QTest::newRow("too long") << "00000000001001";
        QTest::newRow("too big") << "9999999999";
    }

    void notAGrownUp() {
        QFETCH(const QString, pkexecUid);
        const Caller caller = check(0, pkexecUid);
        QCOMPARE(caller.refusal, std::optional(Refusal::NotAGrownUp));
        QVERIFY(!caller.home.has_value());
    }

    // A grown-up with no home in the database has no pictures to give.
    void aGrownUpWithNoHome() {
        const Caller caller = Caller::check(0, QStringLiteral("1001"), account,
                                            [](unsigned int) { return std::nullopt; });
        QVERIFY(!caller.refusal.has_value());
        QVERIFY(!caller.home.has_value());
    }
};

} // namespace

QTEST_GUILESS_MAIN(CallerTest)
#include "tst_caller.moc"
