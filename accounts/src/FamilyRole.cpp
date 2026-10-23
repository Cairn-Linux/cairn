// SPDX-License-Identifier: Apache-2.0
#include "FamilyRole.h"

FamilyRole FamilyRole::fromGroups(const QStringList& groups) {
    if (groups.contains(QStringLiteral("cairn-guardian"))) {
        return {.kind = Kind::Guardian, .level = std::nullopt};
    }
    if (groups.contains(QStringLiteral("wheel"))) {
        return {.kind = Kind::Administrator, .level = std::nullopt};
    }
    // Lowest first, so two level groups give the more restricted one.
    for (int level = lowestLevel; level <= highestLevel; ++level) {
        if (groups.contains(levelGroup(level))) {
            return {.kind = Kind::Child, .level = level};
        }
    }
    return {};
}

QString FamilyRole::levelGroup(int level) {
    return QStringLiteral("cairn-l%1").arg(level);
}

bool FamilyRole::isAdministrator() const {
    return kind == Kind::Guardian || kind == Kind::Administrator;
}
