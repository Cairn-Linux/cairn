// SPDX-License-Identifier: Apache-2.0
#include "GameShelf.h"

#include "GameCatalogue.h"
#include "GamePicture.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QVariantMap>

#include <algorithm>
#include <optional>

namespace {

// A list of a thousand games is a few hundred kilobytes, as for the
// child's file that cairn-family reads.
constexpr qint64 largestFile = qint64{4} * 1024 * 1024;

// Nothing when the file is too big or not a games file.
std::optional<GameCatalogue> readGamesFile(const QString& path) {
    if (path.isEmpty()) {
        return std::nullopt;
    }
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

QString GameShelf::childListPath(const QString& folder, const QString& login) {
    static const QRegularExpression plain(QStringLiteral("^[a-z_][a-z0-9_-]{0,31}$"));
    return plain.match(login).hasMatch() ? QDir(folder).filePath(login + QStringLiteral(".json"))
                                         : QString();
}

QStringList GameShelf::listedIds(const QString& childList) {
    QStringList appIds;
    for (const Game& game : read(QString(), childList).games()) {
        appIds.append(game.appId);
    }
    return appIds;
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

QSet<QString> GameShelf::sharedIds() const {
    QSet<QString> appIds;
    for (const Game& game : m_games) {
        if (game.shared) {
            appIds.insert(game.appId);
        }
    }
    return appIds;
}

bool GameShelf::shows(const QString& appId) const {
    return std::ranges::any_of(m_games, [&appId](const Game& game) { return game.appId == appId; });
}

QVariantList GameShelf::toVariantList() const {
    QVariantList games;
    for (const Game& game : m_games) {
        games.append(QVariantMap{
            {QStringLiteral("appId"), game.appId},
            {QStringLiteral("title"), game.title},
            {QStringLiteral("picture"), game.picture},
            {QStringLiteral("saved"), game.onList},
            {QStringLiteral("shared"), game.shared},
        });
    }
    return games;
}
