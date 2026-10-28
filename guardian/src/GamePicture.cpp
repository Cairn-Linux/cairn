// SPDX-License-Identifier: Apache-2.0
#include "GamePicture.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonValue>

QUrl GamePicture::fromEntry(const QJsonObject& entry) {
    const QJsonValue value = entry.value(QStringLiteral("picture"));
    if (!value.isString()) {
        return {};
    }
    const QString path = value.toString();
    // QDir counts a path inside the program's own resources (":/…") as
    // absolute too. A picture is a file on this computer, so that is refused.
    if (!QDir::isAbsolutePath(path) || path.startsWith(QLatin1Char(':')) ||
        !QFileInfo(path).isFile()) {
        return {};
    }
    return QUrl::fromLocalFile(path);
}
