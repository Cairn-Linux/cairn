// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "GameCatalogue.h"

#include <QString>

#include <optional>

// Makes the pictures and the full games list again from the shared folder,
// as provisioning's make_game_tiles does (ADR-0035, ADR-0041), so a game a
// Guardian has just installed can be saved for a child at once. Each step
// keeps what was there when it cannot finish.
namespace CatalogueRefresh {

// Where each part is. main() gives the real places; a test gives stand-ins.
struct Setup {
    QString python = QStringLiteral("/usr/bin/python3");
    // provision/cairn-game-pictures.py and tools/kidscan/kidscan.py.
    QString pictureStep;
    QString kidscan;
    QString library = QStringLiteral("/var/lib/cairn/steam-library");
    QString pictures = QStringLiteral("/var/lib/cairn/game-pictures");
    QString gamesFile = QStringLiteral("/var/lib/cairn/games.json");
    QString restorecon = QStringLiteral("/usr/sbin/restorecon");
};

struct Result {
    // The picture step ran to its end.
    bool picturesMade = false;
    // kidscan's list is in the games file now.
    bool catalogueMade = false;
    // The games file as it is now, new or old; nothing when neither can be
    // read, and then no game can be added.
    std::optional<GameCatalogue> catalogue;
};

// `home` is the Guardian's whose Steam has the pictures. Without one, under
// sudo, the pictures stay as they are.
Result run(const Setup& setup, const std::optional<QString>& home);

} // namespace CatalogueRefresh
