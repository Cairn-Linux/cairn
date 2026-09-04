// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "TileModel.h"

#include <QList>
#include <QString>

// Reads the version-1 manifest that tools/kidscan writes: a JSON object with
// an "entries" array of {title, category, exec}. Errors come back as a plain
// sentence in Result::error, never as an exception.
namespace Manifest {

struct Result {
    QList<TileModel::Tile> tiles;
    QString error;
};

// `path` is a filesystem path or a file:// URL.
Result read(const QString& path);

} // namespace Manifest
