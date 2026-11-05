// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "FaillockSettings.h"

#include <QList>

#include <cstdint>

// pam_faillock's own rule for when an account is locked, copied from
// check_tally and write_tally in linux-pam 1.7.2 (pam_faillock.c), so the
// login screen can say what faillock will do (ADR-0052). No files and no
// clock: everything comes in as values, so the tests can try every case.
//
// faillock stays the gate. If this copy were ever wrong, the worst case is
// the wrong words on the screen; who gets in never changes.
class LockRule {
public:
    enum class Kind : std::uint8_t {
        Open,                 // two or more wrong tries left, or faillock never locks
        OneTryLeft,           // the next wrong try locks the account
        Locked,               // locked, for secondsLeft more
        LockedUntilReset,     // locked until someone resets the count (unlock_time = never)
        LockedForUnknownTime, // locked, but the newest try is later than now
    };

    struct Result {
        Kind kind = Kind::Open;
        // Only for Locked. faillock still refuses at 0; it lets a try in
        // the second after.
        qint64 secondsLeft = 0;
    };

    // failureTimes are the counted tries from TallyFile; now is seconds
    // since 1970.
    static Result decide(const QList<qint64>& failureTimes, const FaillockSettings& settings,
                         qint64 now);
};
