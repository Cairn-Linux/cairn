// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "GameCatalogue.h"

#include <QString>
#include <QStringList>

#include <optional>

// Makes the pictures and the full games list again from the shared folder,
// as provisioning's make_game_tiles does (ADR-0035, ADR-0041), so a game a
// Guardian has just installed can be saved for a child at once. Each step
// keeps what was there when it cannot finish, and says why in its own words,
// for the journal. Then it names each game in the full list whose tile shows
// its name for want of a picture, so the Save's window can say so (ADR-0047).
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
    // The picture step ran to its end: it said `finished`.
    bool picturesMade = false;
    // The picture step said why it did not: `failed`, `refused folder` or
    // `refused lists`.
    bool picturesEndedEarly = false;
    // The appids the picture step left the Guardian's picture out for.
    QStringList picturesRefused;
    // For the journal: the picture step's lines but `finished`, then, when
    // it did not finish and did not say why, a word of cairn-family's own:
    // `no-home` when there was no Guardian's home to read, as under sudo,
    // `no-answer` when it said no line at all (it could not start, or was
    // stopped before its first line), `stopped` when it said some first.
    QStringList pictureNotes;
    // kidscan's list is in the games file now.
    bool catalogueMade = false;
    // For the journal, when it is not: `failed` (kidscan failed, was
    // stopped or said too much), `not-a-games-list` or `not-written`.
    QString catalogueNote;
    // The games file as it is now, new or old; nothing when neither can be
    // read, and then no game can be added.
    std::optional<GameCatalogue> catalogue;
    // Only after a picture step that went to its end, so each says what
    // helps: for each game in the games file without a picture, by the
    // launcher's rule (GamePicture), in the file's order, `picture-refused
    // APPID` when the step left the Guardian's picture out, and
    // `picture-missing APPID` otherwise.
    QStringList withoutPicture;
};

// `home` is the Guardian's whose Steam has the pictures. Without one, under
// sudo, the pictures stay as they are. `lists` is the folder of each child's
// own list: a game on one keeps its picture (ADR-0047).
Result run(const Setup& setup, const std::optional<QString>& home, const QString& lists);

// Takes one of the picture step's lines into `result`, by the step's own
// grammar. Any other line is left out.
void takePictureLine(Result& result, const QString& line);

// The games file as it is, with nothing made again; nothing when it cannot
// be read.
std::optional<GameCatalogue> read(const QString& gamesFile);

} // namespace CatalogueRefresh
