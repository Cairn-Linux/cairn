// SPDX-License-Identifier: Apache-2.0
#include "ReadableFile.h"

#include <QFile>
#include <QSaveFile>

ReadableFile::Written ReadableFile::write(const QString& path, const QByteArray& bytes) {
    QFile existing(path);
    if (existing.open(QIODevice::ReadOnly) && existing.readAll() == bytes) {
        return Written::Unchanged;
    }
    existing.close();

    // The mode is set on the new file before it is renamed into place, so
    // there is no moment when the launcher finds a file it cannot read.
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) ||
        !file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                             QFileDevice::ReadGroup | QFileDevice::ReadOther) ||
        file.write(bytes) != bytes.size() || !file.commit()) {
        return Written::Failed;
    }
    return Written::Changed;
}
