// SPDX-License-Identifier: Apache-2.0
#include "GameShelf.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

namespace {

QJsonObject game(const QString& appId, const QString& title) {
    return {{"id", "steam-" + appId},
            {"title", title},
            {"category", "games"},
            {"engine", "steam"},
            {"exec", QJsonArray{"steam", "-applaunch", appId}}};
}

QByteArray gamesFile(const QJsonArray& entries) {
    return QJsonDocument(QJsonObject{{"version", 1}, {"entries", entries}}).toJson();
}

QStringList titles(const GameShelf& shelf) {
    QStringList titles;
    for (const GameShelf::Game& game : shelf.games()) {
        titles.append(game.title + (game.onList ? " (listed)" : ""));
    }
    return titles;
}

class GameShelfTest : public QObject {
    Q_OBJECT

    QString path(const QString& name) const {
        return QDir(m_temporary.path()).filePath(name);
    }

    static void write(const QString& path, const QByteArray& bytes) {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(bytes), bytes.size());
    }

    QTemporaryDir m_temporary;
    QString m_catalogue;

private slots:
    void init() {
        QVERIFY(m_temporary.isValid());
        m_catalogue = path("games.json");
        write(m_catalogue, gamesFile({game("1001", "Game one"), game("1002", "Game two"),
                                      game("1003", "Game three")}));
    }

    void cleanup() {
        QDir(m_temporary.path()).removeRecursively();
        QDir().mkpath(m_temporary.path());
    }

    // Every shared game in the full list's order; the child's are marked.
    void theChildsGamesAreMarked() {
        write(path("ada.json"), gamesFile({game("1003", "Game three"), game("1001", "Game one")}));
        const GameShelf shelf = GameShelf::read(m_catalogue, path("ada.json"));
        QCOMPARE(titles(shelf),
                 QStringList({"Game one (listed)", "Game two", "Game three (listed)"}));
        QCOMPARE(shelf.list(), GameShelf::List::Read);
        QCOMPARE(shelf.onListCount(), 2);
        QCOMPARE(shelf.games().at(1).appId, "1002");
    }

    // A new child has no file, and so no games: not a fault.
    void noFileIsNoGames() {
        const GameShelf shelf = GameShelf::read(m_catalogue, path("ben.json"));
        QCOMPARE(titles(shelf), QStringList({"Game one", "Game two", "Game three"}));
        QCOMPARE(shelf.list(), GameShelf::List::None);
        QCOMPARE(shelf.onListCount(), 0);
        QCOMPARE(GameShelf::read(m_catalogue, {}).list(), GameShelf::List::None);
    }

    void anEmptyListIsRead() {
        write(path("ben.json"), gamesFile({}));
        const GameShelf shelf = GameShelf::read(m_catalogue, path("ben.json"));
        QCOMPARE(shelf.list(), GameShelf::List::Read);
        QCOMPARE(shelf.onListCount(), 0);
    }

    // The launcher gives no game tiles from a file it cannot use.
    void aListThatCannotBeUsedIsNoGames_data() {
        QTest::addColumn<QByteArray>("text");
        QTest::newRow("not JSON") << QByteArray("{");
        QTest::newRow("another version")
            << QJsonDocument(QJsonObject{{"version", 2}, {"entries", QJsonArray{}}}).toJson();
        QTest::newRow("no entries") << QJsonDocument(QJsonObject{{"version", 1}}).toJson();
        QTest::newRow("too big") << QByteArray((4 * 1024 * 1024) + 1, ' ');
    }

    void aListThatCannotBeUsedIsNoGames() {
        QFETCH(const QByteArray, text);
        write(path("cy.json"), text);
        const GameShelf shelf = GameShelf::read(m_catalogue, path("cy.json"));
        QCOMPARE(shelf.list(), GameShelf::List::Unreadable);
        QCOMPARE(shelf.onListCount(), 0);
        QCOMPARE(shelf.games().size(), 3);
    }

    void aListThatCannotBeOpenedIsNoGames() {
        write(path("cy.json"), gamesFile({game("1001", "Game one")}));
        QVERIFY(QFile::setPermissions(path("cy.json"), QFileDevice::Permissions()));
        const GameShelf shelf = GameShelf::read(m_catalogue, path("cy.json"));
        QCOMPARE(shelf.list(), GameShelf::List::Unreadable);
        QCOMPARE(shelf.onListCount(), 0);
    }

    // The launcher's rule decides what is a game, in both files.
    void onlyGamesTheLauncherWouldShow() {
        QJsonObject notSteam = game("1004", "Not Steam");
        notSteam["exec"] = QJsonArray{"sh", "-c", "true"};
        const QJsonObject untitled = game("1005", "");
        QJsonObject notAGame = game("1006", "Not a game");
        notAGame["category"] = "make";
        write(m_catalogue, gamesFile({game("1001", "Game one"), notSteam, untitled, notAGame,
                                      game("1001", "Game one again")}));
        QJsonObject listedBadly = game("1001", "Game one");
        listedBadly["exec"] = QJsonArray{"steam", "-applaunch", "1001", "-x"};
        write(path("ada.json"), gamesFile({listedBadly}));
        const GameShelf shelf = GameShelf::read(m_catalogue, path("ada.json"));
        QCOMPARE(titles(shelf), QStringList({"Game one"}));
    }

    // A game on the list that the shared folder no longer has still gets
    // a tile from the child's copy, so it shows, last, in the list's order.
    void aListedGameNoLongerSharedStillShows() {
        write(path("ada.json"), gamesFile({game("2002", "Gone two"), game("1002", "Game two"),
                                           game("2001", "Gone one")}));
        const GameShelf shelf = GameShelf::read(m_catalogue, path("ada.json"));
        QCOMPARE(titles(shelf), QStringList({"Game one", "Game two (listed)", "Game three",
                                             "Gone two (listed)", "Gone one (listed)"}));
        QCOMPARE(shelf.onListCount(), 3);
        QCOMPARE(shelf.sharedCount(), 3);
        QVERIFY(shelf.games().at(2).shared);
        QVERIFY(!shelf.games().at(3).shared);
        QCOMPARE(shelf.games().at(3).appId, "2002");
    }

    // A title is carried as written; the page shows it as plain text.
    void aTitleIsCarriedAsWritten() {
        const QString markup = "<b>Putt</b><img src=\"http://127.0.0.1:9/x.png\">";
        write(m_catalogue, gamesFile({game("1001", markup)}));
        QCOMPARE(GameShelf::read(m_catalogue, {}).games().at(0).title, markup);
    }

    // Each game shows the picture step's copy for its app id, as the
    // child's tile does, and never one an entry names (ADR-0047).
    void aPictureIsFoundByItsAppId() {
        QVERIFY(QDir().mkpath(path("pictures")));
        const QString picture = path("pictures/1001.jpg");
        write(picture, "jpeg");
        write(path("named.jpg"), "jpeg");
        QJsonObject named = game("1002", "Game two");
        named["picture"] = path("named.jpg");
        write(m_catalogue, gamesFile({game("1001", "Game one"), named}));
        const GameShelf shelf = GameShelf::read(m_catalogue, {}, path("pictures"));
        QCOMPARE(shelf.games().at(0).picture, QUrl::fromLocalFile(picture));
        QVERIFY(shelf.games().at(1).picture.isEmpty());
        // With no folder, no game has a picture.
        QVERIFY(GameShelf::read(m_catalogue, {}).games().at(0).picture.isEmpty());
    }

    // A game on the child's list that the shared folder no longer has keeps
    // the picture the picture step keeps for it, as their tile does.
    void aGameNoLongerSharedKeepsItsPicture() {
        QVERIFY(QDir().mkpath(path("pictures")));
        write(path("pictures/2001.jpg"), "jpeg");
        write(path("ada.json"), gamesFile({game("2001", "Gone one")}));
        const GameShelf shelf = GameShelf::read(m_catalogue, path("ada.json"), path("pictures"));
        QCOMPARE(shelf.games().size(), 4);
        QVERIFY(!shelf.games().at(3).shared);
        QCOMPARE(shelf.games().at(3).picture, QUrl::fromLocalFile(path("pictures/2001.jpg")));
    }

    // No shared folder list yet: no shared games, and the child's list is
    // still read and shown.
    void noFullListIsNoGames_data() {
        QTest::addColumn<QByteArray>("text");
        QTest::addColumn<bool>("present");
        QTest::newRow("missing") << QByteArray() << false;
        QTest::newRow("not JSON") << QByteArray("[") << true;
    }

    void noFullListIsNoGames() {
        QFETCH(const QByteArray, text);
        QFETCH(const bool, present);
        QVERIFY(QFile::remove(m_catalogue));
        if (present) {
            write(m_catalogue, text);
        }
        write(path("ada.json"), gamesFile({game("1001", "Game one")}));
        const GameShelf shelf = GameShelf::read(m_catalogue, path("ada.json"));
        QCOMPARE(titles(shelf), QStringList({"Game one (listed)"}));
        QCOMPARE(shelf.sharedCount(), 0);
        QVERIFY(!shelf.games().at(0).shared);
        QCOMPARE(shelf.list(), GameShelf::List::Read);
    }
};

} // namespace

QTEST_GUILESS_MAIN(GameShelfTest)
#include "tst_gameshelf.moc"
