// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "GameCatalogue.h"

#include <QJsonObject>
#include <QList>
#include <QStringList>

// The rule for one child's new list of games (ADR-0041), with nothing read
// or written. The list holds a game only when the parent ticked it, and
// then only when the child already had it or the copy just gave it to them,
// so a tile never names a game the child has no copy of.
namespace ChildGameList {

// What to do for the games a parent ticked.
struct Plan {
    // Ticked, not on the old list, and in the shared folder: to copy.
    QStringList toCopy;
    // Ticked, not on the old list, and not in the shared folder.
    QStringList notShared;
};

Plan plan(const QList<QJsonObject>& old, const QStringList& ticked, const GameCatalogue& catalogue);

struct NewList {
    // Sorted by title, as kidscan sorts the full list, each game once.
    QList<QJsonObject> entries;
    int added = 0;
    int removed = 0;
};

// `old` is the child's list before the Save. `copied` holds the games the
// copy reported as copied or already there. A game still ticked that the
// shared folder no longer has keeps its old entry: the child's copy still
// plays it.
NewList result(const QList<QJsonObject>& old, const QStringList& ticked, const QStringList& copied,
               const GameCatalogue& catalogue);

} // namespace ChildGameList
