// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QString>
#include <QStringList>

#include <cstdint>
#include <optional>

// Who an account is in the family, from its groups alone (ADR-0011). The
// order is ADR-0023's, the same as PAM's, polkit's and the session's:
// an administrator first, whatever else they are in, then a child by level,
// then everyone else.
struct FamilyRole {
    enum class Kind : std::uint8_t {
        // In cairn-guardian, even from a level group.
        Guardian,
        // In wheel but not cairn-guardian, such as the installer's account.
        // Never a child, and not a Guardian either.
        Administrator,
        // In one of cairn-l1 to cairn-l4, and neither of the above.
        Child,
        // Anyone else: an account Cairn did not make.
        NotFamily,
    };

    // The levels a child can be at, and the two the kiosk serves.
    static constexpr int lowestLevel = 1;
    static constexpr int highestKioskLevel = 2;
    static constexpr int highestLevel = 4;

    Kind kind = Kind::NotFamily;
    // A child's level. An account in two level groups is at the lower one,
    // the more restricted: a level change passes through that (#100).
    std::optional<int> level;

    static FamilyRole fromGroups(const QStringList& groups);
    // The group that puts a child at a level, such as cairn-l1.
    static QString levelGroup(int level);

    bool isAdministrator() const;
};
