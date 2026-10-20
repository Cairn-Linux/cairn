// SPDX-License-Identifier: Apache-2.0
#include "TileFiles.h"

#include "GamesFile.h"
#include "Manifest.h"
#include "SteamGameWait.h"

#include <QSet>

#include <utility>

TileFiles::Result TileFiles::read(const QString& manifestPath, const QString& gamesPath) {
    Result result;
    if (manifestPath.isEmpty()) {
        result.tiles = TileModel::builtInTiles();
    } else {
        const Manifest::Result manifest = Manifest::read(manifestPath);
        result.sentences = manifest.skipped + manifest.notes;
        if (manifest.error.isEmpty()) {
            result.tiles = manifest.tiles;
        } else {
            result.manifestError = manifest.error;
            result.sentences.append(manifest.error);
            result.tiles = TileModel::builtInTiles();
        }
    }

    // A game that already has a tile keeps that one. The manifest's is the
    // Guardian's own choice, so the games file's is left out without a word.
    QSet<QString> appIds;
    for (const TileModel::Tile& tile : std::as_const(result.tiles)) {
        const QString appId = SteamGameWait::appIdOf(tile.exec);
        if (!appId.isEmpty()) {
            appIds.insert(appId);
        }
    }
    const GamesFile::Result games = GamesFile::read(gamesPath, appIds);
    result.sentences.append(games.sentences);
    result.tiles.append(games.tiles);
    return result;
}
