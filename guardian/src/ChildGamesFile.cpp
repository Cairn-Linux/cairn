// SPDX-License-Identifier: Apache-2.0
#include "ChildGamesFile.h"

#include "GameCatalogue.h"
#include "RootFolder.h"

#include <QDir>
#include <QFile>

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
    const std::optional<RootFolder::File> file =
        RootFolder::read(m_folder, m_owner, login + QStringLiteral(".json"), largestFile);
    if (!file) {
        return {};
    }
    const std::optional<GameCatalogue> catalogue = GameCatalogue::fromJson(file->bytes);
    if (!catalogue) {
        return {};
    }
    return catalogue->entries();
}

bool ChildGamesFile::write(const QString& login, const QList<QJsonObject>& entries) const {
    return RootFolder::write(m_folder, m_owner, m_restorecon, login + QStringLiteral(".json"),
                             GameCatalogue::toJson(entries));
}
