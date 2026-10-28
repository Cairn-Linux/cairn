// SPDX-License-Identifier: Apache-2.0
#include "GameShelf.h"

#include "GameCatalogue.h"
#include "GamePicture.h"

#include <QFile>
#include <QFileInfo>
#include <QSet>

#include <algorithm>
#include <optional>

namespace {

// A list of a thousand games is a few hundred kilobytes, as for the
// child's file that cairn-family reads.
constexpr qint64 largestFile = qint64{4} * 1024 * 1024;

// Nothing when the file is too big or not a games file.
std::optional<GameCatalogue> readGamesFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > largestFile) {
        return std::nullopt;
    }
    return GameCatalogue::fromJson(file.readAll());
}

GameShelf::Game gameOf(const QJsonObject& entry, bool onList, bool shared) {
    // Every entry fromJson keeps has an appid.
    return {.appId = GameCatalogue::appIdOf(entry).value_or(QString()),
            .title = entry.value(QStringLiteral("title")).toString(),
            .picture = GamePicture::fromEntry(entry),
            .onList = onList,
            .shared = shared};
}

} // namespace

GameShelf GameShelf::read(const QString& catalogue, const QString& childList) {
    GameShelf shelf;
    QList<QJsonObject> listed;
    if (!childList.isEmpty() && QFileInfo::exists(childList)) {
        const std::optional<GameCatalogue> own = readGamesFile(childList);
        shelf.m_list = own ? List::Read : List::Unreadable;
        if (own) {
            listed = own->entries();
        }
    }
    QSet<QString> listedIds;
    for (const QJsonObject& entry : listed) {
        listedIds.insert(GameCatalogue::appIdOf(entry).value_or(QString()));
    }

    QSet<QString> sharedIds;
    const std::optional<GameCatalogue> shared = readGamesFile(catalogue);
    for (const QJsonObject& entry : shared ? shared->entries() : QList<QJsonObject>()) {
        Game game = gameOf(entry, false, true);
        game.onList = listedIds.contains(game.appId);
        sharedIds.insert(game.appId);
        shelf.m_games.append(game);
    }
    // The child's launcher still gives a tile for a game on their list
    // that the shared folder no longer has, so the page shows it too.
    for (const QJsonObject& entry : listed) {
        const Game game = gameOf(entry, true, false);
        if (!sharedIds.contains(game.appId)) {
            shelf.m_games.append(game);
        }
    }
    return shelf;
}

QList<GameShelf::Game> GameShelf::games() const {
    return m_games;
}

GameShelf::List GameShelf::list() const {
    return m_list;
}

int GameShelf::onListCount() const {
    return static_cast<int>(
        std::ranges::count_if(m_games, [](const Game& game) { return game.onList; }));
}

int GameShelf::sharedCount() const {
    return static_cast<int>(
        std::ranges::count_if(m_games, [](const Game& game) { return game.shared; }));
}
