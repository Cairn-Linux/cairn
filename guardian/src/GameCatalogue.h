// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QString>

#include <optional>

// The games in the shared folder, as kidscan lists them in
// /var/lib/cairn/games.json: the launcher's games file, version 1. Each
// entry is found by its appid, read with the launcher's own rule
// (launcher/src/GamesFile.cpp): a command of exactly `steam -applaunch
// APPID`. An entry without one, a title, or the category games is not a
// game here, as it gets no tile there.
class GameCatalogue {
public:
    // The only version of the file the launcher reads.
    static constexpr int version = 1;

    // Nothing when the text is not version 1 of the file.
    static std::optional<GameCatalogue> fromJson(const QByteArray& text);

    // The appid of a games file entry, or nothing.
    static std::optional<QString> appIdOf(const QJsonObject& entry);

    // The games file text for these entries, as the launcher reads it.
    static QByteArray toJson(const QList<QJsonObject>& entries);

    // Every game, in the file's order, each once.
    QList<QJsonObject> entries() const;

    bool has(const QString& appId) const;
    // The entry for a game the catalogue has; an empty object otherwise.
    QJsonObject entry(const QString& appId) const;

private:
    QList<QJsonObject> m_entries;
    QHash<QString, QJsonObject> m_byAppId;
};
