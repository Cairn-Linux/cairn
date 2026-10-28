// SPDX-License-Identifier: Apache-2.0
#include "GamesRequest.h"

#include <QTest>

namespace {

QStringList many(const QString& format, int count) {
    QStringList items;
    for (int i = 1; i <= count; ++i) {
        items.append(format.arg(i));
    }
    return items;
}

class GamesRequestTest : public QObject {
    Q_OBJECT

private slots:
    // Each child's whole list, in the order given; an empty one is no games.
    void readsEachChildsList() {
        const GamesRequest request =
            GamesRequest::fromArguments({"save-games", "ada=620,294690", "ben=", "cy=12"});
        QVERIFY(!request.refusal.has_value());
        QCOMPARE(request.children.size(), 3);
        QCOMPARE(request.children.at(0).login, QStringLiteral("ada"));
        QCOMPARE(request.children.at(0).appIds, QStringList({"620", "294690"}));
        QCOMPARE(request.children.at(1).login, QStringLiteral("ben"));
        QVERIFY(request.children.at(1).appIds.isEmpty());
        QCOMPARE(request.children.at(2).appIds, QStringList({"12"}));
        QCOMPARE(request.logins(), QStringList({"ada", "ben", "cy"}));
    }

    void refuses_data() {
        QTest::addColumn<QStringList>("arguments");
        QTest::addColumn<QString>("word");
        QTest::newRow("nothing") << QStringList{} << "usage";
        QTest::newRow("no child") << QStringList{"save-games"} << "usage";
        QTest::newRow("another subcommand") << QStringList{"save-game", "ada=620"} << "usage";
        QTest::newRow("no equals sign") << QStringList{"save-games", "ada"} << "usage";
        QTest::newRow("a flag") << QStringList{"save-games", "--all"} << "usage";
        QTest::newRow("a child twice") << QStringList{"save-games", "ada=620", "ada="} << "usage";
        QTest::newRow("a game twice") << QStringList{"save-games", "ada=620,620"} << "usage";
        QTest::newRow("no login") << QStringList{"save-games", "=620"} << "bad-name";
        QTest::newRow("capitals") << QStringList{"save-games", "Ada=620"} << "bad-name";
        QTest::newRow("a path") << QStringList{"save-games", "../ada=620"} << "bad-name";
        QTest::newRow("a space") << QStringList{"save-games", "ada =620"} << "bad-name";
        QTest::newRow("trailing comma") << QStringList{"save-games", "ada=620,"} << "bad-appid";
        QTest::newRow("leading comma") << QStringList{"save-games", "ada=,620"} << "bad-appid";
        QTest::newRow("two commas") << QStringList{"save-games", "ada=620,,12"} << "bad-appid";
        QTest::newRow("all") << QStringList{"save-games", "ada=--all"} << "bad-appid";
        QTest::newRow("leading zero") << QStringList{"save-games", "ada=0620"} << "bad-appid";
        QTest::newRow("zero") << QStringList{"save-games", "ada=0"} << "bad-appid";
        QTest::newRow("eleven digits")
            << QStringList{"save-games", "ada=12345678901"} << "bad-appid";
        QTest::newRow("a sign") << QStringList{"save-games", "ada=+620"} << "bad-appid";
        QTest::newRow("a space in the list")
            << QStringList{"save-games", "ada=620, 12"} << "bad-appid";
        QTest::newRow("arabic-indic digits")
            << QStringList{"save-games", QStringLiteral("ada=١٢")} << "bad-appid";
        QTest::newRow("fullwidth digits")
            << QStringList{"save-games", QStringLiteral("ada=６２０")} << "bad-appid";
        QTest::newRow("a second equals sign")
            << QStringList{"save-games", "ada=620=12"} << "bad-appid";
        QTest::newRow("33 children") << QStringList{"save-games"} + many("c%1=", 33) << "usage";
        QTest::newRow("1001 games")
            << QStringList{"save-games", "ada=" + many("%1", 1001).join(',')} << "usage";
    }

    // Nothing is taken from arguments that break a rule.
    void refuses() {
        QFETCH(const QStringList, arguments);
        QFETCH(const QString, word);
        const GamesRequest request = GamesRequest::fromArguments(arguments);
        QVERIFY(request.refusal.has_value());
        QCOMPARE(refusalWord(request.refusal.value_or(Refusal::Failed)), word);
        QVERIFY(request.children.isEmpty());
    }

    // The limits themselves are allowed.
    void takesTheMost() {
        QVERIFY(!GamesRequest::fromArguments(QStringList{"save-games"} + many("c%1=", 32))
                     .refusal.has_value());
        const GamesRequest request =
            GamesRequest::fromArguments({"save-games", "ada=" + many("%1", 1000).join(',')});
        QVERIFY(!request.refusal.has_value());
        QCOMPARE(request.children.at(0).appIds.size(), 1000);
    }
};

} // namespace

QTEST_GUILESS_MAIN(GamesRequestTest)
#include "tst_gamesrequest.moc"
