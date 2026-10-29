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

    // No child named is a look for new games, which changes no list.
    void noChildIsALook() {
        const GamesRequest request = GamesRequest::fromArguments({"save-games"});
        QVERIFY(!request.refusal.has_value());
        QVERIFY(request.children.isEmpty());
        QVERIFY(request.logins().isEmpty());
    }

    // The children to log out come first, each once, and each has a list.
    void readsTheChildrenToLogOut() {
        const GamesRequest request = GamesRequest::fromArguments(
            {"save-games", "--log-out=ben", "--log-out=ada", "ada=620", "ben=12", "cy=7"});
        QVERIFY(!request.refusal.has_value());
        QCOMPARE(request.logOut, QStringList({"ben", "ada"}));
        QCOMPARE(request.logins(), QStringList({"ada", "ben", "cy"}));
    }

    // Without --log-out, nobody is logged out.
    void nobodyToLogOut() {
        const GamesRequest request = GamesRequest::fromArguments({"save-games", "ada=620"});
        QVERIFY(!request.refusal.has_value());
        QVERIFY(request.logOut.isEmpty());
    }

    void refuses_data() {
        QTest::addColumn<QStringList>("arguments");
        QTest::addColumn<QString>("word");
        QTest::newRow("nothing") << QStringList{} << "usage";
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
        QTest::newRow("log-out after a list")
            << QStringList{"save-games", "ada=620", "--log-out=ada"} << "usage";
        QTest::newRow("log-out between lists")
            << QStringList{"save-games", "ben=12", "--log-out=ada", "ada=620"} << "usage";
        QTest::newRow("log-out twice")
            << QStringList{"save-games", "--log-out=ada", "--log-out=ada", "ada=620"} << "usage";
        QTest::newRow("log-out of a child with no list")
            << QStringList{"save-games", "--log-out=ben", "ada=620"} << "usage";
        QTest::newRow("log-out alone") << QStringList{"save-games", "--log-out=ada"} << "usage";
        QTest::newRow("log-out with no name")
            << QStringList{"save-games", "--log-out=", "ada=620"} << "bad-name";
        QTest::newRow("log-out of a path")
            << QStringList{"save-games", "--log-out=../ada", "ada=620"} << "bad-name";
        QTest::newRow("log-out of capitals")
            << QStringList{"save-games", "--log-out=Ada", "Ada=620"} << "bad-name";
        QTest::newRow("log-out as its own word")
            << QStringList{"save-games", "--log-out", "ada", "ada=620"} << "usage";
        QTest::newRow("log-out with a space")
            << QStringList{"save-games", "--log-out", "ada=620"} << "usage";
        QTest::newRow("a log-out spelled another way")
            << QStringList{"save-games", "--logout=ada", "ada=620"} << "bad-name";
        QTest::newRow("33 log-outs")
            << QStringList{"save-games"} + many("--log-out=c%1", 33) + many("c%1=", 33) << "usage";
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
        QVERIFY(request.logOut.isEmpty());
    }

    // The limits themselves are allowed.
    void takesTheMost() {
        QVERIFY(!GamesRequest::fromArguments(QStringList{"save-games"} + many("c%1=", 32))
                     .refusal.has_value());
        const GamesRequest everyone = GamesRequest::fromArguments(
            QStringList{"save-games"} + many("--log-out=c%1", 32) + many("c%1=", 32));
        QVERIFY(!everyone.refusal.has_value());
        QCOMPARE(everyone.logOut.size(), 32);
        const GamesRequest request =
            GamesRequest::fromArguments({"save-games", "ada=" + many("%1", 1000).join(',')});
        QVERIFY(!request.refusal.has_value());
        QCOMPARE(request.children.at(0).appIds.size(), 1000);
    }
};

} // namespace

QTEST_GUILESS_MAIN(GamesRequestTest)
#include "tst_gamesrequest.moc"
