// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantList>

#include <cstdint>

// What the Games page shows for one child (ADR-0041): every game in the
// shared folder, from the full games list kidscan writes, each marked when
// it is on the child's own list. Both files are the launcher's games file,
// read with the launcher's rules, so a game is marked here exactly when the
// child's launcher gives it a tile.
class GameShelf {
public:
    // The child's own list. A child with no file has no games: that is a
    // new child, not a fault. A file that is there but cannot be read gives
    // the child no game tiles either, and the page says so.
    enum class List : std::uint8_t { None, Read, Unreadable };

    struct Game {
        QString appId;
        // From a Steam game's own name, so the page shows it as plain text.
        QString title;
        // The picture step's copy for the game's app id, as the child's tile
        // shows it (GamePicture); empty when there is none, and the page
        // shows the title.
        QUrl picture;
        bool onList = false;
        // False for a game on the child's list that the shared folder no
        // longer has. The child's own copy may still play it, so it shows.
        bool shared = true;
    };

    // `catalogue` is the full games list, /var/lib/cairn/games.json, and
    // `childList` the child's own, /var/lib/cairn/games/<login>.json. An
    // empty `childList` reads as a child with no file. `pictures` is the
    // picture step's folder, /var/lib/cairn/game-pictures; "" means no game
    // has a picture.
    static GameShelf read(const QString& catalogue, const QString& childList,
                          const QString& pictures = {});

    // The child's own list in `folder`, or "" when the login does not
    // follow cairn-family's rule for one: it goes into a file name, so any
    // other reads as a child with no list.
    static QString childListPath(const QString& folder, const QString& login);

    // The appids on a child's own list, in its order: none when the file is
    // missing or cannot be read.
    static QStringList listedIds(const QString& childList);

    // Every game in the shared folder, in the full list's order, each once,
    // then the games on the child's list that the shared folder no longer
    // has, in the child's list's order. Only those last ones when the full
    // list is missing or cannot be read.
    QList<Game> games() const;
    List list() const;
    // How many of games() are on the child's list.
    int onListCount() const;
    // How many of games() are among these appids.
    int countAmong(const QStringList& appIds) const;
    // How many of games() are in the shared folder.
    int sharedCount() const;
    // The appids of those.
    QSet<QString> sharedIds() const;
    // Whether games() has this game.
    bool shows(const QString& appId) const;
    // games() for the page, each as a map: appId, title, picture, saved (on
    // the child's list) and shared.
    QVariantList toVariantList() const;

private:
    QList<Game> m_games;
    List m_list = List::None;
};
