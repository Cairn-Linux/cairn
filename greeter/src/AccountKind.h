// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QStringList>

#include <cstdint>

// What kind of family member an account is, from its groups (ADR-0011), and
// so whether its tile on the login screen asks for a password (ADR-0020).
//
// This only decides what the login screen draws. The gate is PAM: the
// greeter's own PAM service lets cairn-l1 and cairn-l2 in with no password,
// unless they are also cairn-guardian or wheel, and asks everyone else for one
// (ADR-0016, ADR-0023). So the rule here must match that one,
// and if it ever did not, the worst case is a password field a child does not
// need, or a login that PAM refuses.
class AccountKind : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Only its Kind enum is used from QML.")

public:
    // In the order the login screen shows them.
    enum class Kind : std::uint8_t { YoungChild, OlderChild, Guardian, NotFamily };
    Q_ENUM(Kind)

    static Kind fromGroups(const QStringList& groups);
    static bool needsPassword(Kind kind);
};
