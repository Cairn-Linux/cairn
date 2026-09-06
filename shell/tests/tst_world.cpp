// SPDX-License-Identifier: Apache-2.0
#include "Fixture.h"
#include "World.h"

#include <QTest>

namespace {

QStringList names(const QList<World::Node>& nodes) {
    QStringList out;
    for (const World::Node& node : nodes) {
        out.append(node.name);
    }
    return out;
}

class WorldTest : public QObject {
    Q_OBJECT

private slots:
    void slugsAreLowercaseAsciiWithDashes_data() {
        QTest::addColumn<QString>("title");
        QTest::addColumn<QString>("expected");
        QTest::newRow("word") << "Draw" << "draw";
        QTest::newRow("two words") << "Tux Paint" << "tux-paint";
        QTest::newRow("punctuation") << "Putt-Putt: Moon!" << "putt-putt-moon";
        QTest::newRow("accents folded") << "Café Ünïcode" << "cafe-unicode";
        QTest::newRow("spaces around") << "  Story  " << "story";
        QTest::newRow("digits") << "Freddi Fish 2" << "freddi-fish-2";
    }

    void slugsAreLowercaseAsciiWithDashes() {
        QFETCH(const QString, title);
        QFETCH(const QString, expected);
        QCOMPARE(World::slug(title), expected);
    }

    void rootHoldsKindFoldersInOrderThenHome() {
        QCOMPARE(names(fixtureWorld().children(QString())),
                 (QStringList{QStringLiteral("make"), QStringLiteral("practice"),
                              QStringLiteral("home")}));
        const World onlyMake(
            {{.title = QStringLiteral("Draw"), .kind = World::Kind::Make, .exec = {}}}, {});
        QCOMPARE(names(onlyMake.children(QString())), QStringList{QStringLiteral("make")});
        const World empty({}, {});
        QVERIFY(empty.children(QString()).isEmpty());
    }

    void doorsKeepTheirOrderWithinAFolder() {
        const QList<World::Node> make = fixtureWorld().children(QStringLiteral("make"));
        QCOMPARE(names(make), (QStringList{QStringLiteral("draw"), QStringLiteral("music"),
                                           QStringLiteral("tux-paint")}));
        QCOMPARE(make[2].title, QStringLiteral("Tux Paint"));
        QCOMPARE(make[2].path, QStringLiteral("make/tux-paint"));
        QCOMPARE(make[0].exec, QStringList{QStringLiteral("tuxpaint")});
    }

    void repeatedNamesAreNumbered() {
        const World world({{.title = QStringLiteral("Draw"), .kind = World::Kind::Make, .exec = {}},
                           {.title = QStringLiteral("draw"), .kind = World::Kind::Make, .exec = {}},
                           {.title = QStringLiteral("!!!"), .kind = World::Kind::Make, .exec = {}}},
                          {});
        QCOMPARE(names(world.children(QStringLiteral("make"))),
                 (QStringList{QStringLiteral("draw"), QStringLiteral("draw-2"),
                              QStringLiteral("thing")}));
    }

    void homeIsNestedByPath() {
        const World world = fixtureWorld();
        QCOMPARE(names(world.children(QStringLiteral("home"))), QStringList{QStringLiteral("sam")});
        QCOMPARE(names(world.children(QStringLiteral("home/sam"))),
                 (QStringList{QStringLiteral("hello"), QStringLiteral("words"),
                              QStringLiteral("notes"), QStringLiteral("pictures")}));
        const World::Node cairn =
            world.find(QStringLiteral("home/sam/notes/cairn")).value_or(World::Node{});
        QCOMPARE(cairn.kind, World::Kind::Note);
        QVERIFY(!cairn.text.isEmpty());
        QVERIFY(world.children(QStringLiteral("home/sam/pictures")).isEmpty());
    }

    void pathHelpers() {
        QCOMPARE(World::parentOf(QStringLiteral("home/sam/notes")), QStringLiteral("home/sam"));
        QCOMPARE(World::parentOf(QStringLiteral("make")), QString());
        QCOMPARE(World::join(QString(), QStringLiteral("make")), QStringLiteral("make"));
        QCOMPARE(World::join(QStringLiteral("make"), QStringLiteral("draw")),
                 QStringLiteral("make/draw"));
    }

    void findByNameLooksEverywhere() {
        const World world = fixtureWorld();
        QCOMPARE(world.findByName(QStringLiteral("draw")).value_or(World::Node{}).path,
                 QStringLiteral("make/draw"));
        QCOMPARE(world.findByName(QStringLiteral("cairn")).value_or(World::Node{}).path,
                 QStringLiteral("home/sam/notes/cairn"));
        QVERIFY(!world.findByName(QStringLiteral("nothing")));
        QVERIFY(!world.find(QStringLiteral("make/nothing")));
    }
};

} // namespace

QTEST_GUILESS_MAIN(WorldTest)
#include "tst_world.moc"
