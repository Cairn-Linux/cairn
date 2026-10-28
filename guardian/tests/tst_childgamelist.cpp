// SPDX-License-Identifier: Apache-2.0
#include "ChildGameList.h"

#include <QJsonArray>
#include <QTest>

namespace {

QJsonObject game(const QString& appId, const QString& title, const QString& picture = {}) {
    QJsonObject entry{{"id", "steam-" + appId},
                      {"title", title},
                      {"category", "games"},
                      {"engine", "steam"},
                      {"exec", QJsonArray{"steam", "-applaunch", appId}},
                      {"source", QJsonObject{{"store", "steam"}, {"appid", appId}}},
                      {"needs_steam_running", true}};
    if (!picture.isEmpty()) {
        entry.insert("picture", picture);
    }
    return entry;
}

// The shared folder: Portal, Celeste and Zoo Tycoon.
GameCatalogue catalogue() {
    const QList<QJsonObject> entries{game("620", "Portal 2", "/p/620.jpg"),
                                     game("504230", "Celeste"), game("12", "zoo tycoon")};
    return GameCatalogue::fromJson(GameCatalogue::toJson(entries)).value_or(GameCatalogue());
}

QStringList appIds(const QList<QJsonObject>& entries) {
    QStringList found;
    for (const QJsonObject& entry : entries) {
        found.append(GameCatalogue::appIdOf(entry).value_or(QStringLiteral("none")));
    }
    return found;
}

class ChildGameListTest : public QObject {
    Q_OBJECT

private slots:
    // Only games not on the old list are copied, and only from the shared
    // folder.
    void plansTheCopy() {
        const ChildGameList::Plan plan =
            ChildGameList::plan({game("620", "Portal 2")}, {"620", "12", "999"}, catalogue());
        QCOMPARE(plan.toCopy, QStringList({"12"}));
        QCOMPARE(plan.notShared, QStringList({"999"}));
    }

    void result_data() {
        QTest::addColumn<QList<QJsonObject>>("old");
        QTest::addColumn<QStringList>("ticked");
        QTest::addColumn<QStringList>("copied");
        QTest::addColumn<QStringList>("listed");
        QTest::addColumn<int>("added");
        QTest::addColumn<int>("removed");
        const QList<QJsonObject> none;
        const QList<QJsonObject> portal{game("620", "Portal 2")};
        QTest::newRow("a first Save")
            << none << QStringList{"620", "12"} << QStringList{"620", "12"}
            << QStringList{"620", "12"} << 2 << 0;
        QTest::newRow("kept without a copy")
            << portal << QStringList{"620"} << QStringList{} << QStringList{"620"} << 0 << 0;
        QTest::newRow("only copies are added")
            << none << QStringList{"620", "12"} << QStringList{"12"} << QStringList{"12"} << 1 << 0;
        QTest::newRow("unticked while held")
            << portal << QStringList{} << QStringList{} << QStringList{} << 0 << 1;
        QTest::newRow("a copy of a game not ticked")
            << none << QStringList{"620"} << QStringList{"620", "504230"} << QStringList{"620"} << 1
            << 0;
        QTest::newRow("a copy of a game not shared")
            << none << QStringList{"999"} << QStringList{"999"} << QStringList{} << 0 << 0;
        QTest::newRow("sorted by title")
            << none << QStringList{"12", "620", "504230"} << QStringList{"12", "620", "504230"}
            << QStringList{"504230", "620", "12"} << 3 << 0;
        QTest::newRow("an old list twice")
            << QList<QJsonObject>{game("620", "Portal 2"), game("620", "Portal 2")}
            << QStringList{"620"} << QStringList{} << QStringList{"620"} << 0 << 0;
        QTest::newRow("an empty list")
            << QList<QJsonObject>{game("620", "Portal 2"), game("12", "zoo tycoon")}
            << QStringList{} << QStringList{} << QStringList{} << 0 << 2;
    }

    // The list holds a ticked game the child already had or was just given,
    // sorted by title, each once, and nothing that was not ticked.
    void result() {
        QFETCH(const QList<QJsonObject>, old);
        QFETCH(const QStringList, ticked);
        QFETCH(const QStringList, copied);
        QFETCH(const QStringList, listed);
        QFETCH(const int, added);
        QFETCH(const int, removed);
        const ChildGameList::NewList list = ChildGameList::result(old, ticked, copied, catalogue());
        QCOMPARE(appIds(list.entries), listed);
        QCOMPARE(list.added, added);
        QCOMPARE(list.removed, removed);
        for (const QString& appId : appIds(list.entries)) {
            QVERIFY(ticked.contains(appId));
        }
    }

    // A kept game takes the shared folder's entry, with its new picture; one
    // the folder no longer has keeps its old entry, as the child still has
    // their copy.
    void keptEntries() {
        const QList<QJsonObject> old{game("620", "Portal 2"), game("400", "Portal")};
        const ChildGameList::NewList list =
            ChildGameList::result(old, {"620", "400"}, {}, catalogue());
        QCOMPARE(list.entries.size(), 2);
        QCOMPARE(list.entries.at(0), game("400", "Portal"));
        QCOMPARE(list.entries.at(1), game("620", "Portal 2", "/p/620.jpg"));
    }
};

} // namespace

QTEST_GUILESS_MAIN(ChildGameListTest)
#include "tst_childgamelist.moc"
