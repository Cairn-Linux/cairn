// SPDX-License-Identifier: Apache-2.0
#include "DidYouMean.h"

#include <QList>

#include <algorithm>

namespace DidYouMean {

int editDistance(const QString& from, const QString& to) {
    // The classic row-by-row table; each row holds the distance from a
    // prefix of `from` to every prefix of `to`.
    QList<int> previous(to.size() + 1);
    QList<int> current(to.size() + 1);
    for (int column = 0; column <= to.size(); ++column) {
        previous[column] = column;
    }
    for (int row = 1; row <= from.size(); ++row) {
        current[0] = row;
        for (int column = 1; column <= to.size(); ++column) {
            const int substitution = from[row - 1] == to[column - 1] ? 0 : 1;
            current[column] = std::min({previous[column] + 1, current[column - 1] + 1,
                                        previous[column - 1] + substitution});
        }
        std::swap(previous, current);
    }
    return previous[to.size()];
}

std::optional<QString> closest(const QString& word, const QStringList& candidates) {
    constexpr int shortWord = 3;
    const int allowed = word.size() <= shortWord ? 1 : 2;
    std::optional<QString> best;
    int bestDistance = allowed + 1;
    for (const QString& candidate : candidates) {
        const int distance = editDistance(word, candidate);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = candidate;
        }
    }
    return best;
}

} // namespace DidYouMean
