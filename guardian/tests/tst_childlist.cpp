// SPDX-License-Identifier: Apache-2.0
#include "ChildList.h"

#include <QTest>

namespace {

Account account(unsigned int uid, const QString& login, const QStringList& groups,
                const QString& fullName = {}) {
    return {.uid = uid, .groups = groups, .login = login, .fullName = fullName};
}

class ChildListTest : public QObject {
    Q_OBJECT

private slots:
    // In the order the accounts were made, whatever order NSS gives.
    void inTheOrderTheyWereMade() {
        const std::vector<ListedChild> children = childrenAmong({
            account(1003, "ben", {"ben", "cairn-l2"}, "Ben"),
            account(1001, "ada", {"ada", "cairn-l1"}, "Ada"),
            account(1004, "cy", {"cy", "cairn-l1"}),
        });
        QCOMPARE(children.size(), std::size_t{3});
        QCOMPARE(children.at(0).login, QStringLiteral("ada"));
        QCOMPARE(children.at(0).name, QStringLiteral("Ada"));
        QCOMPARE(children.at(0).level, 1);
        QCOMPARE(children.at(1).login, QStringLiteral("ben"));
        QCOMPARE(children.at(1).level, 2);
        // No full name: the login name, as on the login screen.
        QCOMPARE(children.at(2).name, QStringLiteral("cy"));
    }

    // Only accounts cairn-family would change.
    void everyoneElseIsLeftOut() {
        const std::vector<ListedChild> children = childrenAmong({
            account(1000, "guardian", {"guardian", "wheel", "cairn-guardian"}),
            account(1005, "helper", {"helper", "wheel"}),
            account(1006, "guest", {"guest"}),
            account(1007, "parent", {"parent", "cairn-guardian", "cairn-l1"}),
            account(1008, "admin", {"admin", "wheel", "cairn-l2"}),
            account(990, "service", {"service", "cairn-l1"}),
            account(65534, "nobody", {"nobody"}),
            account(1002, "ada", {"ada", "cairn-l1"}),
        });
        QCOMPARE(children.size(), std::size_t{1});
        QCOMPARE(children.at(0).login, QStringLiteral("ada"));
    }

    // A level change cut off part-way: the lower, more restricted level.
    void twoLevelGroupsShowTheLowerLevel() {
        const std::vector<ListedChild> children =
            childrenAmong({account(1001, "ada", {"cairn-l2", "ada", "cairn-l1"})});
        QCOMPARE(children.size(), std::size_t{1});
        QCOMPARE(children.at(0).level, 1);
    }

    // Shown with their level, though the page offers only 1 and 2.
    void olderLevelsAreListed() {
        const std::vector<ListedChild> children = childrenAmong(
            {account(1001, "ada", {"ada", "cairn-l3"}), account(1002, "ben", {"ben", "cairn-l4"})});
        QCOMPARE(children.size(), std::size_t{2});
        QCOMPARE(children.at(0).level, 3);
        QCOMPARE(children.at(1).level, 4);
    }

    void noChildren() {
        QVERIFY(childrenAmong({}).empty());
    }
};

} // namespace

QTEST_APPLESS_MAIN(ChildListTest)
#include "tst_childlist.moc"
