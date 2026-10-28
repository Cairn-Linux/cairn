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

} // namespace

GameShelf GameShelf::read(const QString& catalogue, const QString& childList) {
    GameShelf shelf;
    QSet<QString> listed;
    if (!childList.isEmpty() && QFileInfo::exists(childList)) {
        const std::optional<GameCatalogue> own = readGamesFile(childList);
        shelf.m_list = own ? List::Read : List::Unreadable;
        if (own) {
            for (const QJsonObject& entry : own->entries()) {
                listed.insert(GameCatalogue::appIdOf(entry).value_or(QString()));
            }
        }
    }

    const std::optional<GameCatalogue> shared = readGamesFile(catalogue);
    if (!shared) {
        return shelf;
    }
    for (const QJsonObject& entry : shared->entries()) {
        // Every entry fromJson keeps has an appid.
        const QString appId = GameCatalogue::appIdOf(entry).value_or(QString());
        shelf.m_games.append({.appId = appId,
                              .title = entry.value(QStringLiteral("title")).toString(),
                              .picture = GamePicture::fromEntry(entry),
                              .onList = listed.contains(appId)});
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
