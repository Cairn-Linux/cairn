// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "TileModel.h"

#include <QList>
#include <QString>
#include <QStringList>

// Reads the version-1 manifest that tools/kidscan writes: a JSON object with
// an "entries" array of {title, category, exec}, each with an optional
// "picture" (TilePicture). Errors come back as a plain sentence in
// Result::error, never as an exception.
namespace Manifest {

struct Result {
    QList<TileModel::Tile> tiles;
    // Set when the file cannot be used at all; then there are no tiles.
    QString error;
    // One sentence per entry that was left out. The other entries still load.
    QStringList skipped;
    // One sentence per tile that kept its place but not its picture.
    QStringList notes;
};

// `path` is a filesystem path or a file:// URL.
Result read(const QString& path);

// A file:// URL as a filesystem path; anything else as it is. The QML tests
// name their files by URL.
QString localPath(const QString& pathOrUrl);

} // namespace Manifest
