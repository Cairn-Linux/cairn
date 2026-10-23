// SPDX-License-Identifier: Apache-2.0
#include "Account.h"

#include <QTest>

#include <unistd.h>

namespace {

class AccountTest : public QObject {
    Q_OBJECT

private slots:
    // The system's own lookup finds root and gives it groups; an account
    // that cannot exist is nothing.
    void theSystemLookUp() {
        QVERIFY(!lookUpAccount("no-such-account-cairn-test").has_value());
        QVERIFY(!lookUpAccount(QString()).has_value());
        const std::optional<Account> found = lookUpAccount("root");
        QVERIFY(found.has_value());
        const Account root = found.value_or(Account{});
        QCOMPARE(root.uid, 0U);
        QVERIFY(root.groups.contains("root"));
        QCOMPARE(root.login, QStringLiteral("root"));
    }

    // By number, the account running the test is found under its own name.
    void theSystemLookUpByNumber() {
        const std::optional<Account> self = lookUpAccountByUid(getuid());
        QVERIFY(self.has_value());
        QVERIFY(!self.value_or(Account{}).login.isEmpty());
        QCOMPARE(lookUpAccount(self.value_or(Account{}).login).value_or(Account{}).uid, getuid());
        QVERIFY(!lookUpAccountByUid(4000000000U).has_value());
    }

    void theFullNameComesFirst() {
        const Account named{.uid = 1002, .groups = {}, .login = "ada", .fullName = "Ada"};
        QCOMPARE(named.displayName(), QStringLiteral("Ada"));
        const Account unnamed{.uid = 1003, .groups = {}, .login = "ben", .fullName = {}};
        QCOMPARE(unnamed.displayName(), QStringLiteral("ben"));
    }
};

} // namespace

QTEST_APPLESS_MAIN(AccountTest)
#include "tst_account.moc"
