// SPDX-License-Identifier: Apache-2.0
#include "LanguageFile.h"

#include "RootFolder.h"

#include <QDir>
#include <QFile>

namespace {

QString childFolder(const QString& parent, const QString& login) {
    return QDir(parent).filePath(login);
}

} // namespace

// The parent first: RootFolder makes only the last folder of a path.
bool LanguageFile::write(const QString& parent, unsigned int owner, const QString& restorecon,
                         const QString& login, const ChildLanguage::Language& language) {
    return RootFolder::make(parent, owner, restorecon) &&
           RootFolder::write(childFolder(parent, login), owner, restorecon,
                             QString::fromLatin1(ChildLanguage::fileName),
                             ChildLanguage::fileText(language));
}

// The child's folder may stay, empty: cairn-session adds it to
// XDG_CONFIG_DIRS, where a folder with nothing in it changes nothing.
bool LanguageFile::remove(const QString& parent, unsigned int owner, const QString& login) {
    if (!QDir(parent).exists()) {
        return true;
    }
    return RootFolder::isSafe(parent, owner) &&
           RootFolder::remove(childFolder(parent, login), owner,
                              QString::fromLatin1(ChildLanguage::fileName));
}

// A file far larger than any Family writes is not a language file.
std::optional<QString> LanguageFile::read(const QString& parent, const QString& login) {
    constexpr qint64 largest = 4096;
    QFile file(
        QDir(childFolder(parent, login)).filePath(QString::fromLatin1(ChildLanguage::fileName)));
    if (!file.open(QIODevice::ReadOnly) || file.size() > largest) {
        return std::nullopt;
    }
    return ChildLanguage::localeIn(file.read(largest));
}
