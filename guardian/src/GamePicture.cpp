// SPDX-License-Identifier: Apache-2.0
#include "GamePicture.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

// The app id is checked here too, though GameCatalogue has checked it,
// because this builds a path from it, and cairn-family runs it as root.
// QDir counts a path inside the program's own resources (":/…") as absolute
// too; a picture is a file on this computer, so that is refused.
QUrl GamePicture::forAppId(const QString& folder, const QString& appId) {
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
