// SPDX-License-Identifier: Apache-2.0
#include "Completion.h"

namespace {

QStringList startingWith(const QStringList& names, const QString& prefix) {
    QStringList matches;
    for (const QString& name : names) {
        if (name.startsWith(prefix)) {
            matches.append(name);
        }
    }
    return matches;
}

} // namespace

namespace Completion {

QStringList candidates(const QString& line, const QStringList& commands,
                       const QStringList& namesHere) {
    const QString typed = line.toLower();
    if (!typed.contains(QLatin1Char(' '))) {
        return startingWith(commands, typed.trimmed());
    }
    const QStringList words = typed.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    const QString prefix = words.size() > 1 ? words.last() : QString();
    QStringList names = startingWith(namesHere, prefix);
    names.sort();
    return names;
}

} // namespace Completion
