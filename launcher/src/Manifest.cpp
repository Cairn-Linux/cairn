// SPDX-License-Identifier: Apache-2.0
#include "Manifest.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

#include <optional>

namespace {

constexpr int supportedVersion = 1;

QString localPath(const QString& pathOrUrl) {
    const QUrl url(pathOrUrl);
    return url.isLocalFile() ? url.toLocalFile() : pathOrUrl;
}

// The tile kinds the brand defines (DESIGN §6.1, ADR-0024). Anything else is
// refused rather than guessed.
std::optional<TileModel::Kind> kindFromCategory(const QString& category) {
    if (category == QStringLiteral("make")) {
        return TileModel::Kind::Make;
    }
    if (category == QStringLiteral("practice")) {
        return TileModel::Kind::Practice;
    }
    if (category == QStringLiteral("games")) {
        return TileModel::Kind::Games;
    }
    if (category == QStringLiteral("machine")) {
        return TileModel::Kind::Machine;
    }
    return std::nullopt;
}

std::optional<QStringList> execFromJson(const QJsonValue& value) {
    if (!value.isArray()) {
        return std::nullopt;
    }
    QStringList exec;
    for (const auto& part : value.toArray()) {
        if (!part.isString()) {
            return std::nullopt;
        }
        exec.append(part.toString());
    }
    return exec;
}

} // namespace

Manifest::Result Manifest::read(const QString& path) {
    const QString file = localPath(path);
    QFile source(file);
    if (!source.open(QIODevice::ReadOnly)) {
        return {.tiles = {},
                .error = QStringLiteral("Could not open the manifest %1.").arg(file),
                .skipped = {}};
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(source.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return {.tiles = {},
                .error = QStringLiteral("The manifest %1 is not valid JSON.").arg(file),
                .skipped = {}};
    }

    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("version")).toInt() != supportedVersion) {
        return {.tiles = {},
                .error = QStringLiteral("The manifest %1 is not version %2.")
                             .arg(file)
                             .arg(supportedVersion),
                .skipped = {}};
    }

    Result result;
    for (const auto& value : root.value(QStringLiteral("entries")).toArray()) {
        const QJsonObject entry = value.toObject();
        const QString title = entry.value(QStringLiteral("title")).toString();
        const QString category = entry.value(QStringLiteral("category")).toString();
        const std::optional<TileModel::Kind> kind = kindFromCategory(category);
        const std::optional<QStringList> exec = execFromJson(entry.value(QStringLiteral("exec")));

        // One bad entry costs only its own tile (#97).
        if (title.isEmpty() || !kind || !exec) {
            result.skipped.append(
                QStringLiteral("The manifest %1 has an entry without a title, a category of "
                               "make, practice, games or machine, or an exec list: %2. It "
                               "was left out.")
                    .arg(file, title.isEmpty() ? QStringLiteral("(untitled)") : title));
            continue;
        }
        result.tiles.append({.title = title, .kind = *kind, .exec = *exec, .opensTerminal = false});
    }
    if (result.tiles.isEmpty()) {
        result.error =
            result.skipped.isEmpty()
                ? QStringLiteral("The manifest %1 has no entries.").arg(file)
                : QStringLiteral("The manifest %1 has no entries the launcher can use.").arg(file);
    }
    return result;
}
