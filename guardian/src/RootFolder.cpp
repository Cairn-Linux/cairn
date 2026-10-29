// SPDX-License-Identifier: Apache-2.0
#include "RootFolder.h"

#include "ReadableFile.h"
#include "SystemTool.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <cerrno>
#include <sys/stat.h>
#include <unistd.h>

namespace {

QString pathIn(const QString& folder, const QString& name) {
    return QDir(folder).filePath(name);
}

void label(const QString& restorecon, const QString& path) {
    if (QFileInfo(restorecon).isExecutable()) {
        SystemTool::run(restorecon, {path});
    }
}

} // namespace

// lstat, not stat, so a link put in the folder's place is seen as a link.
bool RootFolder::isSafe(const QString& folder, unsigned int owner) {
    struct stat status{};
    if (lstat(QFile::encodeName(folder).constData(), &status) != 0) {
        return false;
    }
    return S_ISDIR(status.st_mode) && status.st_uid == owner &&
           (status.st_mode & (S_IWGRP | S_IWOTH)) == 0;
}

bool RootFolder::make(const QString& folder, unsigned int owner, const QString& restorecon) {
    const QByteArray path = QFile::encodeName(folder);
    struct stat status{};
    if (lstat(path.constData(), &status) != 0) {
        if (mkdir(path.constData(), 0755) != 0 || chmod(path.constData(), 0755) != 0) {
            return false;
        }
        label(restorecon, folder);
    }
    return isSafe(folder, owner);
}

// A file renamed into place keeps the label of the file it was written as,
// so it is labelled again for the folder, as the menu entries are. The new
// file is in place by then, so a relabel that fails still counts as
// written.
bool RootFolder::write(const QString& folder, unsigned int owner, const QString& restorecon,
                       const QString& name, const QByteArray& bytes) {
    if (!make(folder, owner, restorecon)) {
        return false;
    }
    const QString target = pathIn(folder, name);
    const ReadableFile::Written written = ReadableFile::write(target, bytes);
    if (written == ReadableFile::Written::Failed) {
        return false;
    }
    if (written == ReadableFile::Written::Changed) {
        label(restorecon, target);
    }
    return true;
}

// A folder that is not there holds no file either.
bool RootFolder::remove(const QString& folder, unsigned int owner, const QString& name) {
    struct stat status{};
    if (lstat(QFile::encodeName(folder).constData(), &status) != 0) {
        return errno == ENOENT;
    }
    if (!isSafe(folder, owner)) {
        return false;
    }
    const QByteArray path = QFile::encodeName(pathIn(folder, name));
    return unlink(path.constData()) == 0 || errno == ENOENT;
}
