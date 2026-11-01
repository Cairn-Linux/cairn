// SPDX-License-Identifier: Apache-2.0
#include "GamesFile.h"

#include "Manifest.h"
#include "TilePicture.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <optional>

namespace {

constexpr int supportedVersion = 1;

// The app id of a command that is exactly three words: `steam`, `-applaunch`
// and an app id, which Steam numbers from 1 with no leading zero. Not a path
// to a program, and no other argument, so the file can only ever start a
// Steam game. Any other command has no app id.
std::optional<QString> steamAppId(const QJsonValue& exec) {
    static const QRegularExpression appId(
        QRegularExpression::anchoredPattern(QStringLiteral("[1-9][0-9]{0,9}")));
    const QJsonArray words = exec.toArray();
    if (words.size() == 3 && words.at(0).toString() == QStringLiteral("steam") &&
        words.at(1).toString() == QStringLiteral("-applaunch") && words.at(2).isString() &&
        appId.match(words.at(2).toString()).hasMatch()) {
        return words.at(2).toString();
    }
    return std::nullopt;
}

// A file that is there but cannot be used costs only the game tiles; the
// manifest's tiles still show. Provisioning makes the file again on every run.
QString unusable(const QString& file, const QString& problem) {
    return QStringLiteral("The games file %1 %2, so no game tiles show. Provisioning makes it "
                          "again on its next run.")
        .arg(file, problem);
}

} // namespace

GamesFile::Result GamesFile::read(const QString& path, const QSet<QString>& appIdsToLeaveOut,
                                  const QString& picturesFolder) {
    if (path.isEmpty()) {
        return {};
    }
    const QString file = Manifest::localPath(path);
    if (!QFileInfo::exists(file)) {
        return {};
    }
    QFile source(file);
    if (!source.open(QIODevice::ReadOnly)) {
        return {.tiles = {}, .sentences = {unusable(file, QStringLiteral("could not be opened"))}};
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(source.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return {.tiles = {}, .sentences = {unusable(file, QStringLiteral("is not valid JSON"))}};
    }
    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("version")).toInt() != supportedVersion) {
        return {.tiles = {},
                .sentences = {
                    unusable(file, QStringLiteral("is not version %1").arg(supportedVersion))}};
    }

    Result result;
    QSet<QString> kept;
    for (const auto& value : root.value(QStringLiteral("entries")).toArray()) {
        const QJsonObject entry = value.toObject();
        const std::optional<QString> appId = steamAppId(entry.value(QStringLiteral("exec")));
        if (appId && (appIdsToLeaveOut.contains(*appId) || kept.contains(*appId))) {
            continue;
        }
        const QString title = entry.value(QStringLiteral("title")).toString();
        if (title.isEmpty() ||
            entry.value(QStringLiteral("category")).toString() != QStringLiteral("games") ||
            !appId) {
            result.sentences.append(
                QStringLiteral("The games file %1 has an entry without a title, the category "
                               "games, or the command steam -applaunch and an app id: %2. It "
                               "was left out.")
                    .arg(file, title.isEmpty() ? QStringLiteral("(untitled)") : title));
            continue;
        }
        kept.insert(*appId);
        result.tiles.append(
            {.title = title,
             .kind = TileModel::Kind::Games,
             .exec = {QStringLiteral("steam"), QStringLiteral("-applaunch"), *appId},
             .opensTerminal = false,
             .picture = TilePicture::forAppId(picturesFolder, *appId)});
    }
    return result;
}
