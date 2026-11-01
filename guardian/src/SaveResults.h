// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "GameShelf.h"
#include "GamesSaving.h"
#include "SaveReport.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QVariantMap>

// How far a Save has got, what it did for each child, or why nothing was
// saved, as the Games page's QML reads them (ADR-0041), from cairn-family's
// own lines, read by SaveReport, and its last word.
namespace SaveResults {

// The stage the command's lines say, as the window's step of a Save.
GamesSaving::Progress progressFor(SaveReport::Stage stage);

// Why nothing was saved, from the run's last word: one of cairn-family's
// refusal words, pkexec's own, or SaveRun's; Unknown for any other.
GamesSaving::Reason reasonFor(const QString& answer);

// Each game's title by its appid, for naming a game left out.
QHash<QString, QString> titlesOf(const QList<GameShelf::Game>& games);

// The child's result as a map: login, name, added, removed, held (a
// GamesSaving::Held), skipped, a list of maps with appId, title and
// reason (a GamesSaving::Skip), and tools (a GamesSaving::Tools), what the
// copy did with Steam's tools. added and removed are -1 when the child's
// list was not written.
QVariantMap forChild(const SaveReport::Child& child, const QString& name,
                     const QHash<QString, QString>& titles);

// Whether every child named got every new game: none held, none left out.
bool allGiven(const QList<SaveReport::Child>& children);

} // namespace SaveResults
