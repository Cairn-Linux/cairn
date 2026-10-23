// SPDX-License-Identifier: Apache-2.0
#include "AccountKind.h"

#include "FamilyRole.h"

AccountKind::Kind AccountKind::fromGroups(const QStringList& groups) {
    const FamilyRole role = FamilyRole::fromGroups(groups);
    switch (role.kind) {
    // A Guardian who is also in a level group is still a Guardian: the tile
    // asks for a password either way, and a parent looks for their own tile
    // among the grown-ups.
    case FamilyRole::Kind::Guardian:
        return Kind::Guardian;
    case FamilyRole::Kind::Child:
        return role.level.value_or(FamilyRole::highestLevel) <= FamilyRole::highestKioskLevel
                   ? Kind::YoungChild
                   : Kind::OlderChild;
    // Any other administrator is never a child either, whatever else they
    // are in: PAM asks them for a password (ADR-0023).
    case FamilyRole::Kind::Administrator:
    case FamilyRole::Kind::NotFamily:
        return Kind::NotFamily;
    }
    return Kind::NotFamily;
}

bool AccountKind::needsPassword(Kind kind) {
    return kind != Kind::YoungChild;
}
