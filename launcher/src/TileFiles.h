// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "TileModel.h"

#include <QList>
#include <QString>
#include <QStringList>

// The tiles come from two files (ADR-0033): the manifest, which is the
// Guardian's own choice, and the games file, which provisioning makes from
// the shared Steam library. TileModel and the Steam-at-login check both read
// them here, so the tiles a child sees and the decision to start Steam for
// them never disagree.
namespace TileFiles {

struct Result {
    // The manifest's tiles, or the built-in ones when there is no manifest or
    // it cannot be used, then the games file's. Never the Terminal, which is
    // TileModel's own.
    QList<TileModel::Tile> tiles;
    // Set when the manifest cannot be used, so the built-in tiles show.
    QString manifestError;
    // For the journal, in the order they were found, the manifest's error
    // among them.
    QStringList sentences;
};

// Each path is a filesystem path, a file:// URL or empty. `picturesFolder`
// holds the games file's pictures, <appid>.jpg (ADR-0047); empty means the
// game tiles have none.
Result read(const QString& manifestPath, const QString& gamesPath,
            const QString& picturesFolder = {});

} // namespace TileFiles
