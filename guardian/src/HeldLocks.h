// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "PasswordTry.h"

#include <QtTypes>

#include <cstddef>
#include <vector>

// One account's locks from the journal, oldest first, as the Notices page
// takes them up among the tries (ADR-0054): the try each one goes with,
// and the ones no try came after.
class HeldLocks {
public:
    // faillock's record of the try that locked, and the try's own record,
    // come from one process within moments of each other; so do the lock
    // screen's try and its line. Microseconds.
    static constexpr qint64 sameMoment = 5000000;

    struct Held {
        qint64 time = 0;
        LockEvent lock;
        // Its place in the account's count is taken.
        bool settled = false;
        // A notice made from tries took it as its lock.
        bool attached = false;
    };

    // In order of time, wherever it falls: the lock screen's locks come
    // from a run of their own.
    void add(const LockEvent& lock);
    // The lock the try caused, written by its own process just before it.
    Held* ownLock(const PasswordTry& passwordTry);
    // The lock screen's lock just after the try.
    Held* lockScreenLock(const PasswordTry& passwordTry);
    // The next lock before `time` not yet settled, or nothing.
    Held* nextBefore(qint64 time);
    const std::vector<Held>& all() const;

private:
    std::vector<Held> m_locks;
    // The first that may not be settled yet.
    std::size_t m_unsettled = 0;
};
