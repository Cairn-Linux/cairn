// SPDX-License-Identifier: Apache-2.0
#include "TilePicture.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonValue>
#include <QRegularExpression>

TilePicture::Result TilePicture::fromEntry(const QJsonObject& entry) {
    const QString key = QStringLiteral("picture");
    if (!entry.contains(key)) {
        return {};
    }
    const QJsonValue value = entry.value(key);
    if (!value.isString()) {
        return {.url = {}, .refused = QStringLiteral("(not text)")};
    }
    const QString path = value.toString();
    if (path.isEmpty()) {
        return {.url = {}, .refused = QStringLiteral("(empty)")};
    }
    // QDir counts a path inside the program's own resources (":/…") as
    // absolute too. A picture is a file on this computer, so that is refused.
    const bool usable = QDir::isAbsolutePath(path) && !path.startsWith(QLatin1Char(':')) &&
                        QFileInfo(path).isFile();
    if (!usable) {
        return {.url = {}, .refused = path};
    }
    return {.url = QUrl::fromLocalFile(path), .refused = std::nullopt};
}

// The app id is checked here too, though GamesFile has checked it, because
// this builds a path from it. A link is not followed, as kidscan's
// --pictures and the picture step's own rule take only a plain file.
QUrl TilePicture::forAppId(const QString& folder, const QString& appId) {
    static const QRegularExpression steamAppId(
        QRegularExpression::anchoredPattern(QStringLiteral("[1-9][0-9]{0,9}")));
    if (folder.isEmpty() || !steamAppId.match(appId).hasMatch()) {
        return {};
    }
    const QString path = QDir(folder).filePath(appId + QStringLiteral(".jpg"));
    if (!QDir::isAbsolutePath(path) || path.startsWith(QLatin1Char(':'))) {
        return {};
    }
    const QFileInfo file(path);
    if (!file.isFile() || file.isSymLink()) {
        return {};
    }
    return QUrl::fromLocalFile(path);
}
