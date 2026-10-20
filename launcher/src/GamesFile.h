// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "TileModel.h"

#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

// Reads the games file provisioning makes with kidscan from the shared Steam
// library, /var/lib/cairn/games.json (ADR-0033). It has the manifest's form,
// but the launcher trusts it less: it is made from Steam's files, so nothing
// in it may choose a program. The one command an entry may hold is the Steam
// client starting a game by its app id.
namespace GamesFile {

struct Result {
    // Games tiles, each running exactly `steam -applaunch <appid>`.
    QList<TileModel::Tile> tiles;
    // For the journal: the file could not be used, or an entry was left out
    // or kept without its picture.
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
Result read(const QString& path, const QSet<QString>& appIdsToLeaveOut);

} // namespace GamesFile
