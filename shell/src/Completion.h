// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QString>
#include <QStringList>

// What a half-typed line could become. Before the first space it is one of
// the commands; after it, one of the names where the child is. A child
// typing one letter every five seconds is expected, so hinting is eager.
namespace Completion {

// Sorted candidates, or every name when nothing has been typed yet.
QStringList candidates(const QString& line, const QStringList& commands,
                       const QStringList& namesHere);

} // namespace Completion
