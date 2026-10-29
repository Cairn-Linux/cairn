// SPDX-License-Identifier: Apache-2.0
#include "ReadableFile.h"

#include <QFile>
#include <QSaveFile>

#include <sys/stat.h>
#include <unistd.h>

ReadableFile::Written ReadableFile::write(const QString& path, const QByteArray& bytes) {
    const QByteArray encoded = QFile::encodeName(path);
    struct stat status{};
    if (lstat(encoded.constData(), &status) == 0 && S_ISREG(status.st_mode)) {
        QFile existing(path);
        if (existing.open(QIODevice::ReadOnly) && existing.readAll() == bytes) {
            const bool ownedByWriter = status.st_uid == geteuid();
            if (ownedByWriter && (status.st_mode & 07777) == 0644) {
                return Written::Unchanged;
            }
            if (ownedByWriter) {
                // Same bytes, unsafe mode: repair it in place.
                if (chmod(encoded.constData(), 0644) != 0) {
                    return Written::Failed;
                }
                return Written::Changed;
            }
            // Same bytes, wrong owner: fall through and replace the file,
            // so the new file belongs to the writer.
        }
        existing.close();
    }

    // The mode is set on the new file before it is renamed into place, so
    // there is no moment when the launcher finds a file it cannot read.
    // A link or other non-regular file in the way is replaced, not followed.
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) ||
        !file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                             QFileDevice::ReadGroup | QFileDevice::ReadOther) ||
        file.write(bytes) != bytes.size() || !file.commit()) {
        return Written::Failed;
    }
    return Written::Changed;
}
