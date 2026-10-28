// SPDX-License-Identifier: Apache-2.0
#include "ChildGamesFile.h"

#include "GameCatalogue.h"
#include "ReadableFile.h"
#include "SystemTool.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <sys/stat.h>
#include <utility>

namespace {

// A list of a thousand games is a few hundred kilobytes.
constexpr qint64 largestFile = qint64{4} * 1024 * 1024;

} // namespace

ChildGamesFile::ChildGamesFile(QString folder, unsigned int owner, QString restorecon)
    : m_folder(std::move(folder)), m_owner(owner), m_restorecon(std::move(restorecon)) {}

QString ChildGamesFile::path(const QString& login) const {
    return QDir(m_folder).filePath(login + QStringLiteral(".json"));
}

QList<QJsonObject> ChildGamesFile::read(const QString& login) const {
    QFile file(path(login));
    if (!file.open(QIODevice::ReadOnly) || file.size() > largestFile) {
        return {};
    }
    const std::optional<GameCatalogue> catalogue = GameCatalogue::fromJson(file.readAll());
    if (!catalogue) {
        return {};
    }
    return catalogue->entries();
}

bool ChildGamesFile::write(const QString& login, const QList<QJsonObject>& entries) const {
    if (!folderIsReady()) {
        return false;
    }
    const QString target = path(login);
    const ReadableFile::Written written =
        ReadableFile::write(target, GameCatalogue::toJson(entries));
    if (written == ReadableFile::Written::Failed) {
        return false;
    }
    // A file renamed into place keeps the label of the file it was written
    // as, so it is labelled again for the folder, as the menu entries are.
    return written == ReadableFile::Written::Unchanged || !QFileInfo(m_restorecon).isExecutable() ||
           SystemTool::run(m_restorecon, {target});
}

// lstat, not stat, so a link put in the folder's place is seen as a link.
// mkdir makes only the last folder: /var/lib/cairn is systemd-tmpfiles'.
bool ChildGamesFile::folderIsReady() const {
    const QByteArray folder = QFile::encodeName(m_folder);
    struct stat status{};
    if (lstat(folder.constData(), &status) != 0) {
        if (mkdir(folder.constData(), 0755) != 0 || chmod(folder.constData(), 0755) != 0 ||
            lstat(folder.constData(), &status) != 0) {
            return false;
        }
        if (QFileInfo(m_restorecon).isExecutable()) {
            SystemTool::run(m_restorecon, {m_folder});
        }
    }
    return S_ISDIR(status.st_mode) && status.st_uid == m_owner &&
           (status.st_mode & (S_IWGRP | S_IWOTH)) == 0;
}
