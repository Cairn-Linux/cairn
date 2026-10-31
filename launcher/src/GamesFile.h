// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "TileModel.h"

#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

// Reads a games file: on the image, the child's own list,
// /var/lib/cairn/games/<login>.json, which cairn-family save-games writes
// from entries of the full list (ADR-0041); on a computer set up by
// provisioning, the full list it makes with kidscan from the shared Steam
// library, /var/lib/cairn/games.json (ADR-0033). It has the manifest's form,
// but the launcher trusts it less: it is made from Steam's files, so nothing
// in it may choose a program. The one command an entry may hold is the Steam
// client starting a game by its app id. Nothing in it chooses a picture
// either: a game's picture is the picture step's copy for its app id, so a
// picture copied after the file was written still shows (ADR-0047).
namespace GamesFile {

struct Result {
    // Games tiles, each running exactly `steam -applaunch <appid>`.
    QList<TileModel::Tile> tiles;
    // For the journal: the file could not be used, or an entry was left out.
    QStringList sentences;
};

// `path` is a filesystem path or a file:// URL. An empty path, a file that is
// not there or one with no entries gives no tiles and says nothing: that is
// a computer without shared games, not a fault.
//
// A game whose app id is in `appIdsToLeaveOut` already has a tile, and so
// does a game an earlier entry in the file gave one. Its entry is left out
// before it is checked, so it gets no tile and no sentence: a parent is never
// told about a tile that is not there.
//
// Each game's picture is <picturesFolder>/<appid>.jpg (TilePicture). A game
// without one shows its name, which is not a fault either, so nothing is
// said: the Guardian's Family app names it.
Result read(const QString& path, const QSet<QString>& appIdsToLeaveOut,
            const QString& picturesFolder);

} // namespace GamesFile
