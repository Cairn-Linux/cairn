// SPDX-License-Identifier: Apache-2.0
#include "AccountKind.h"

#include <QTest>

namespace {

using Kind = AccountKind::Kind;

class AccountKindTest : public QObject {
    Q_OBJECT

private slots:
    void kindComesFromTheGroups_data() {
        QTest::addColumn<QStringList>("groups");
        QTest::addColumn<Kind>("expected");
        QTest::newRow("L1") << QStringList{"ada", "cairn-l1"} << Kind::YoungChild;
        QTest::newRow("L2") << QStringList{"ben", "cairn-l2"} << Kind::YoungChild;
        QTest::newRow("L3") << QStringList{"cy", "cairn-l3"} << Kind::OlderChild;
        QTest::newRow("L4") << QStringList{"dee", "cairn-l4"} << Kind::OlderChild;
        QTest::newRow("Guardian") << QStringList{"mum", "wheel", "cairn-guardian"}
                                  << Kind::Guardian;
        QTest::newRow("Guardian in a level group too")
            << QStringList{"dad", "cairn-l1", "cairn-guardian"} << Kind::Guardian;
        QTest::newRow("installer's admin") << QStringList{"bazzite", "wheel"} << Kind::NotFamily;
        QTest::newRow("no groups") << QStringList{} << Kind::NotFamily;
        QTest::newRow("a name that only looks like a level")
            << QStringList{"cairn-l1x", "xcairn-l2", "Cairn-L1"} << Kind::NotFamily;
    }

    void kindComesFromTheGroups() {
        QFETCH(const QStringList, groups);
        QFETCH(const Kind, expected);
        QCOMPARE(AccountKind::fromGroups(groups), expected);
    }

    // The same rule as the greeter's PAM service (session/sddm/pam.d/sddm):
    // only L1 and L2 come in without a password.
    void onlyYoungChildrenSkipThePassword() {
        QVERIFY(!AccountKind::needsPassword(Kind::YoungChild));
        QVERIFY(AccountKind::needsPassword(Kind::OlderChild));
        QVERIFY(AccountKind::needsPassword(Kind::Guardian));
        QVERIFY(AccountKind::needsPassword(Kind::NotFamily));
    }
};

} // namespace

QTEST_APPLESS_MAIN(AccountKindTest)
#include "tst_accountkind.moc"
