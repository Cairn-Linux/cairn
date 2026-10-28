// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "Account.h"
#include "ChildGamesFile.h"
#include "CopyJob.h"
#include "GameCatalogue.h"

#include <QString>
#include <QStringList>

// One child's part of a Save (ADR-0041): the games newly ticked are copied,
// then the child's games file is written with the games they may play and
// have. A child whose copy is held still loses the games taken off, at once.
namespace ChildSave {

// What the Save needs for every child.
struct Shared {
    CopyJob::Setup copy;
    ChildGamesFile file;
    GameCatalogue catalogue;
};

// Says each line for the child as it goes, and returns the last,
// `child LOGIN added N removed N [held WORD]`, for the journal too.
QString run(const Shared& shared, const Account& child, const QStringList& ticked,
            const CopyJob::Say& say);

} // namespace ChildSave
