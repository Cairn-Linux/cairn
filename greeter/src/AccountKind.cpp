// SPDX-License-Identifier: Apache-2.0
#include "AccountKind.h"

AccountKind::Kind AccountKind::fromGroups(const QStringList& groups) {
    // A Guardian who is also in a level group is still a Guardian: the tile
    // asks for a password either way, and a parent looks for their own tile
    // among the grown-ups.
    if (groups.contains(QStringLiteral("cairn-guardian"))) {
        return Kind::Guardian;
    }
    // Any other administrator is never a child either, whatever else they
    // are in: PAM asks them for a password (ADR-0023).
    if (groups.contains(QStringLiteral("wheel"))) {
        return Kind::NotFamily;
    }
    if (groups.contains(QStringLiteral("cairn-l1")) ||
        groups.contains(QStringLiteral("cairn-l2"))) {
        return Kind::YoungChild;
    }
    if (groups.contains(QStringLiteral("cairn-l3")) ||
        groups.contains(QStringLiteral("cairn-l4"))) {
        return Kind::OlderChild;
    }
    return Kind::NotFamily;
}

bool AccountKind::needsPassword(Kind kind) {
    return kind != Kind::YoungChild;
}
