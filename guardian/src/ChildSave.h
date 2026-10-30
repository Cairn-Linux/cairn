// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "Account.h"
#include "ChildGamesFile.h"
#include "CopyJob.h"
#include "GameCatalogue.h"

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

// One child's part of a Save (ADR-0041), in two steps. Before any child's
// copy starts, their games file is written with the games taken off gone,
// so those go at once, even when a copy is held or cut off. Then the games
// newly ticked are copied, and the file is written again with them.
namespace ChildSave {

// What the Save needs for every child.
struct Shared {
    CopyJob::Setup copy;
    ChildGamesFile file;
    // The full games list; nothing when none could be read, and then no
    // game can be added.
    std::optional<GameCatalogue> catalogue;
};

// A child's list as it was before the Save.
struct Before {
    QList<QJsonObject> old;
    // How many games were taken off, once the list without them is written.
    std::optional<int> removed;
};

// Writes the child's list with the games taken off gone and nothing added.
Before takeOff(const Shared& shared, const Account& child, const QStringList& ticked);

// Says each line for the child as it goes, and returns the last,
// `child LOGIN added N removed N [held WORD]`, for the journal too. The
// counts say what the child's file holds now.
QString run(const Shared& shared, const Account& child, const QStringList& ticked,
            const Before& before, const CopyJob::Say& say);

} // namespace ChildSave
