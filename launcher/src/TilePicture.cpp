// SPDX-License-Identifier: Apache-2.0
#include "TilePicture.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonValue>

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
