// SPDX-License-Identifier: Apache-2.0
#include "GameCatalogue.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>

std::optional<GameCatalogue> GameCatalogue::fromJson(const QByteArray& text) {
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(text, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return std::nullopt;
    }
    const QJsonObject root = document.object();
    const QJsonValue entries = root.value(QStringLiteral("entries"));
    if (root.value(QStringLiteral("version")) != QJsonValue(version) || !entries.isArray()) {
        return std::nullopt;
    }
    GameCatalogue catalogue;
    for (const auto& value : entries.toArray()) {
        const QJsonObject entry = value.toObject();
        const std::optional<QString> appId = appIdOf(entry);
        // The first entry for a game wins, as in the launcher.
        if (appId && !catalogue.m_byAppId.contains(*appId)) {
            catalogue.m_byAppId.insert(*appId, entry);
            catalogue.m_entries.append(entry);
        }
    }
    return catalogue;
}

std::optional<QString> GameCatalogue::appIdOf(const QJsonObject& entry) {
    static const QRegularExpression appId(QStringLiteral("^[1-9][0-9]{0,9}$"));
    const QJsonArray words = entry.value(QStringLiteral("exec")).toArray();
    if (entry.value(QStringLiteral("title")).toString().isEmpty() ||
        entry.value(QStringLiteral("category")).toString() != QStringLiteral("games") ||
        words.size() != 3 || words.at(0).toString() != QStringLiteral("steam") ||
        words.at(1).toString() != QStringLiteral("-applaunch") || !words.at(2).isString() ||
        !appId.match(words.at(2).toString()).hasMatch()) {
        return std::nullopt;
    }
    return words.at(2).toString();
}

QByteArray GameCatalogue::toJson(const QList<QJsonObject>& entries) {
    QJsonArray array;
    for (const QJsonObject& entry : entries) {
        array.append(entry);
    }
    const QJsonObject root{{QStringLiteral("version"), version},
                           {QStringLiteral("entries"), array}};
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

QList<QJsonObject> GameCatalogue::entries() const {
    return m_entries;
}

bool GameCatalogue::has(const QString& appId) const {
    return m_byAppId.contains(appId);
}

QJsonObject GameCatalogue::entry(const QString& appId) const {
    return m_byAppId.value(appId);
}
