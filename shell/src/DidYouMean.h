// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QString>
#include <QStringList>

#include <optional>

// Picks the one candidate a mistyped word was probably meant to be, so an
// error can suggest instead of scold: "opn" is open, "hlep" is help.
// Deterministic: ties go to the earlier candidate.
namespace DidYouMean {

// Insertions, deletions and substitutions needed to turn one word into the other.
int editDistance(const QString& from, const QString& to);

// A candidate at most one edit away for short words, two for longer ones.
std::optional<QString> closest(const QString& word, const QStringList& candidates);

} // namespace DidYouMean
