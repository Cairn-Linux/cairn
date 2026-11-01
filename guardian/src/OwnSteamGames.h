// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "SteamManifest.h"

#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

// The games a Guardian installed in their own Steam libraries rather than
// the shared folder, which children cannot have until they are moved
// there (#167). Steam's default library is in the Guardian's home, so a
// game installed the usual way lands here. The Guardian tool runs as the
// Guardian, so it reads its own home; it never changes anything.
namespace OwnSteamGames {

// Where Steam keeps its files in a home, as kidscan looks for them.
QStringList steamRoots(const QString& home);

// Every library of the Steam in `root`, as each one's steamapps folder,
// each once: the root's own, then each one its libraryfolders.vdf lists
// that exists. A libraryfolders.vdf that cannot be read adds none.
QStringList libraries(const QString& root, const QString& home);

// The games in every library of every Steam in `home`, but for the
// library in `sharedLibrary` and anything inside it, and the appids in
// `shared`, which the family already has. Each game once, by appid, in
// order of name. Files that cannot be read are passed over. A home that is
// not a full path, such as none, finds none.
QList<SteamManifest::Game> find(const QString& home, const QString& sharedLibrary,
                                const QSet<QString>& shared);

// The games' names, in the same order.
QStringList names(const QList<SteamManifest::Game>& games);

} // namespace OwnSteamGames
