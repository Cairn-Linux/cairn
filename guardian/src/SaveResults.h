// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "GameShelf.h"
#include "SaveReport.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QVariantMap>

// What a Save, or the copy at a child's logout, did for one child, as the
// Games page's QML reads it (ADR-0041). Both come from cairn-family's own
// lines, read by SaveReport, so the page says the same sentences for both.
namespace SaveResults {

// Each game's title by its appid, for naming a game left out.
QHash<QString, QString> titlesOf(const QList<GameShelf::Game>& games);

// The child's result as a map: login, name, added, removed, held (a
// GamesSaving::Held), later, and skipped, a list of maps with appId, title
// and reason (a GamesSaving::Skip). added and removed are -1 when the
// child's list was not written.
QVariantMap forChild(const SaveReport::Child& child, const QString& name,
                     const QHash<QString, QString>& titles);

// Whether every child named got every new game: none held, none left out.
bool allGiven(const QList<SaveReport::Child>& children);

} // namespace SaveResults
