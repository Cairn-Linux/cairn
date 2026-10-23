// SPDX-License-Identifier: Apache-2.0
#include "FamilyRole.h"

#include <QTest>

namespace {

using Kind = FamilyRole::Kind;

class FamilyRoleTest : public QObject {
    Q_OBJECT

private slots:
    void roleComesFromTheGroups_data() {
        QTest::addColumn<QStringList>("groups");
        QTest::addColumn<Kind>("kind");
        QTest::addColumn<int>("level");
        QTest::newRow("L1") << QStringList{"ada", "cairn-l1"} << Kind::Child << 1;
        QTest::newRow("L2") << QStringList{"ben", "cairn-l2"} << Kind::Child << 2;
        QTest::newRow("L3") << QStringList{"ada", "cairn-l3"} << Kind::Child << 3;
        QTest::newRow("L4") << QStringList{"ben", "cairn-l4"} << Kind::Child << 4;
        QTest::newRow("Guardian") << QStringList{"mum", "wheel", "cairn-guardian"} << Kind::Guardian
                                  << 0;
        QTest::newRow("Guardian without wheel")
            << QStringList{"dad", "cairn-guardian"} << Kind::Guardian << 0;
        QTest::newRow("Guardian in a level group too")
            << QStringList{"dad", "cairn-l1", "cairn-guardian"} << Kind::Guardian << 0;
        QTest::newRow("installer's admin")
            << QStringList{"bazzite", "wheel"} << Kind::Administrator << 0;
        QTest::newRow("admin in a level group too")
            << QStringList{"bazzite", "wheel", "cairn-l1"} << Kind::Administrator << 0;
        QTest::newRow("no groups") << QStringList{} << Kind::NotFamily << 0;
        QTest::newRow("a plain account") << QStringList{"cy", "users"} << Kind::NotFamily << 0;
        QTest::newRow("names that only look like a level")
            << QStringList{"cairn-l1x", "xcairn-l2", "Cairn-L1", "cairn-l5", "cairn-l0", "cairn-l"}
            << Kind::NotFamily << 0;
        QTest::newRow("names that only look like a grown-up")
            << QStringList{"cairn-guardians", "Wheel", "wheel2", "cairn-l2"} << Kind::Child << 2;
        QTest::newRow("two levels give the lower")
            << QStringList{"ada", "cairn-l2", "cairn-l1"} << Kind::Child << 1;
        QTest::newRow("a kiosk and a desktop level give the kiosk")
            << QStringList{"ben", "cairn-l4", "cairn-l2"} << Kind::Child << 2;
    }

    void roleComesFromTheGroups() {
        QFETCH(const QStringList, groups);
        QFETCH(const Kind, kind);
        QFETCH(const int, level);
        const FamilyRole role = FamilyRole::fromGroups(groups);
        QCOMPARE(role.kind, kind);
        QCOMPARE(role.level.value_or(0), level);
        QCOMPARE(role.level.has_value(), kind == Kind::Child);
    }

    void onlyGrownUpsAdministerTheMachine() {
        QVERIFY(FamilyRole::fromGroups({"cairn-guardian"}).isAdministrator());
        QVERIFY(FamilyRole::fromGroups({"wheel"}).isAdministrator());
        QVERIFY(!FamilyRole::fromGroups({"cairn-l1"}).isAdministrator());
        QVERIFY(!FamilyRole::fromGroups({}).isAdministrator());
    }

    void eachLevelHasItsGroup() {
        QCOMPARE(FamilyRole::levelGroup(1), QStringLiteral("cairn-l1"));
        QCOMPARE(FamilyRole::levelGroup(4), QStringLiteral("cairn-l4"));
    }
};

} // namespace

QTEST_APPLESS_MAIN(FamilyRoleTest)
#include "tst_familyrole.moc"
