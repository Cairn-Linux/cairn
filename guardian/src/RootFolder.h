// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QString>

#include <optional>

// A folder under /var/lib/cairn that only root may change and every account
// may read, and the small files in it (ADR-0041): each child's games list,
// the games waiting for a child to log out, and how the last copy at a
// logout ended. Every check is made on the folder itself, never through a
// link, so a file is only ever trusted in a folder its owner alone can
// change.
namespace RootFolder {

// Whether `folder` is a plain folder, not a link, that belongs to `owner`
// and that no one else may write in. `owner` is root, or the test's own
// account in a test.
bool isSafe(const QString& folder, unsigned int owner);

// Makes the folder, mode 0755, when it is missing, and labels it with
// `restorecon` when that program is there; then isSafe. mkdir makes only
// the last folder: /var/lib/cairn is systemd-tmpfiles'.
bool make(const QString& folder, unsigned int owner, const QString& restorecon);

// Writes the file every account may read, mode 0644, renamed into place,
// and labels a changed one again for the folder. False, with nothing
// written, when the folder is not safe or the file cannot be put in place.
// True once the file is in place, even when labelling it fails.
bool write(const QString& folder, unsigned int owner, const QString& restorecon,
           const QString& name, const QByteArray& bytes);

struct File {
    QByteArray bytes;
    QDateTime modified;
};

// The file's bytes, when the folder is safe and the file, never followed
// through a link, is a regular file that belongs to `owner`, that no one
// else may write, and holds at most `largest` bytes. Nothing otherwise,
// or when there is no file.
std::optional<File> read(const QString& folder, unsigned int owner, const QString& name,
                         qint64 largest);

// Whether the folder has anything of that name, a link included.
bool holds(const QString& folder, const QString& name);

// Removes the file from a safe folder. True when it is gone, or was never
// there, folder and all.
bool remove(const QString& folder, unsigned int owner, const QString& name);

} // namespace RootFolder
