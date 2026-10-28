// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "Account.h"
#include "Refusal.h"

#include <QString>

#include <functional>
#include <optional>

// Who started cairn-family, checked before anything else (ADR-0039). A
// Guardian's window runs it through pkexec, which sets PKEXEC_UID to the
// Guardian's number, and no caller can take that away. The family's setup
// runs it as root with sudo, which leaves PKEXEC_UID unset.
struct Caller {
    using LookUpUid = std::function<std::optional<Account>(unsigned int)>;
    using LookUpHome = std::function<std::optional<QString>(unsigned int)>;

    // Set when the command must not go on: not root, or started through
    // pkexec by an account that is not an administrator.
    std::optional<Refusal> refusal;
    // Through pkexec only: the grown-up's home, from the account database,
    // never from $HOME, which the caller sets. Nothing under sudo, or when
    // the grown-up has no home.
    std::optional<QString> home;

    static Caller check(unsigned int euid, const std::optional<QString>& pkexecUid,
                        const LookUpUid& lookUpUid, const LookUpHome& lookUpHome);

    // What main() passes in: the real effective uid and PKEXEC_UID.
    static unsigned int effectiveUid();
    static std::optional<QString> pkexecUidFromEnvironment();
};
