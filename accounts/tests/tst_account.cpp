// SPDX-License-Identifier: Apache-2.0
#include "Account.h"

#include <QTest>

#include <algorithm>
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

    // A home comes from the account database, whatever $HOME says.
    void theSystemLookUpOfAHome() {
        QCOMPARE(lookUpHome(0), std::optional<QString>(QStringLiteral("/root")));
        const QByteArray saved = qgetenv("HOME");
        qputenv("HOME", "/somewhere/else");
        const std::optional<QString> own = lookUpHome(getuid());
        qputenv("HOME", saved);
        QVERIFY(own.has_value());
        QVERIFY(own.value_or(QString()).startsWith('/'));
        QVERIFY(own != std::optional<QString>(QStringLiteral("/somewhere/else")));
        QVERIFY(!lookUpHome(4000000000U).has_value());
    }

    // The whole list holds root and the account running the test, each
    // once, with the same values a lookup by name gives.
    void theSystemList() {
        const std::vector<Account> accounts = listAccounts();
        const auto count = [&accounts](unsigned int uid) {
            return std::ranges::count_if(
                accounts, [uid](const Account& account) { return account.uid == uid; });
        };
        QCOMPARE(count(0), 1);
        // The test's account may come from a source NSS cannot list, such as
        // systemd's dynamic users; if it can be listed it is there once.
        const std::optional<Account> self = lookUpAccountByUid(getuid());
        QVERIFY(self.has_value());
        QVERIFY(count(getuid()) <= 1);
        const auto root =
            std::ranges::find_if(accounts, [](const Account& account) { return account.uid == 0; });
        QVERIFY(root != accounts.end());
        QCOMPARE(root->login, lookUpAccount("root").value_or(Account{}).login);
        QCOMPARE(root->groups, lookUpAccount("root").value_or(Account{}).groups);
    }

    // A name is in use when an account or a group has it.
    void namesInUse() {
        QVERIFY(nameInUse("root"));
        // Each of root's groups, whether or not an account has its name.
        for (const QString& group : lookUpAccount("root").value_or(Account{}).groups) {
            QVERIFY(nameInUse(group));
        }
        QVERIFY(!nameInUse("no-such-account-cairn-test"));
        QVERIFY(!nameInUse(QString()));
    }

    // Only a person's account in a level group, and no administrator.
    void whoIsACairnChild() {
        const auto account = [](unsigned int uid, const QStringList& groups) {
            return Account{.uid = uid, .groups = groups, .login = "ada", .fullName = {}};
        };
        QVERIFY(account(1001, {"ada", "cairn-l1"}).isCairnChild());
        QVERIFY(account(1000, {"ada", "cairn-l3"}).isCairnChild());
        QVERIFY(account(1001, {"ada", "cairn-l1", "cairn-l2"}).isCairnChild());
        QVERIFY(!account(999, {"ada", "cairn-l1"}).isCairnChild());
        QVERIFY(!account(1001, {"ada"}).isCairnChild());
        QVERIFY(!account(1001, {"ada", "cairn-l1", "wheel"}).isCairnChild());
        QVERIFY(!account(1001, {"ada", "cairn-l2", "cairn-guardian"}).isCairnChild());
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
