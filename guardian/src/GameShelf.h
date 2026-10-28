// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QList>
#include <QString>
#include <QUrl>

#include <cstdint>

// What the Games page shows for one child (ADR-0041): every game in the
// shared folder, from the full games list kidscan writes, each marked when
// it is on the child's own list. Both files are the launcher's games file,
// read with its rules, so a game is marked here exactly when the child's
// launcher would give it a tile.
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
        // Empty when the game has no picture; the page shows the title.
        QUrl picture;
        bool onList = false;
    };

    // `catalogue` is the full games list, /var/lib/cairn/games.json, and
    // `childList` the child's own, /var/lib/cairn/games/<login>.json. An
    // empty `childList` reads as a child with no file.
    static GameShelf read(const QString& catalogue, const QString& childList);

    // Every game in the shared folder, in the full list's order, each once.
    // None when that list is missing or cannot be read.
    QList<Game> games() const;
    List list() const;
    // How many of games() are on the child's list.
    int onListCount() const;

private:
    QList<Game> m_games;
    List m_list = List::None;
};
