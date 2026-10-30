// SPDX-License-Identifier: Apache-2.0
#include "Caller.h"

#include "FamilyRole.h"

#include <QRegularExpression>

#include <unistd.h>

// polkit has already asked for an administrator's password, but the command
// checks again, as the visit helper does (ADR-0032), so a mistake in a
// polkit rule cannot make it a child's. Root without pkexec is the family's
// setup, run with sudo.
Caller Caller::check(unsigned int euid, const std::optional<QString>& pkexecUid,
                     const LookUpUid& lookUpUid, const LookUpHome& lookUpHome) {
    if (euid != 0) {
        return {.refusal = Refusal::NotRoot, .home = std::nullopt, .login = std::nullopt};
    }
    if (!pkexecUid) {
        return {.refusal = std::nullopt, .home = std::nullopt, .login = std::nullopt};
    }
    static const QRegularExpression number(QStringLiteral("^[0-9]{1,10}$"));
    bool converted = false;
    const unsigned int uid = pkexecUid->toUInt(&converted);
    if (!number.match(*pkexecUid).hasMatch() || !converted) {
        return {.refusal = Refusal::NotAGrownUp, .home = std::nullopt, .login = std::nullopt};
    }
    const std::optional<Account> caller = lookUpUid(uid);
    if (!caller || !FamilyRole::fromGroups(caller->groups).isAdministrator()) {
        return {.refusal = Refusal::NotAGrownUp, .home = std::nullopt, .login = std::nullopt};
    }
    return {.refusal = std::nullopt, .home = lookUpHome(uid), .login = caller->login};
}

unsigned int Caller::effectiveUid() {
    return geteuid();
}

std::optional<QString> Caller::pkexecUidFromEnvironment() {
    if (!qEnvironmentVariableIsSet("PKEXEC_UID")) {
        return std::nullopt;
    }
    return qEnvironmentVariable("PKEXEC_UID");
}
