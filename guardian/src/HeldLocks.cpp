// SPDX-License-Identifier: Apache-2.0
#include "HeldLocks.h"

#include <algorithm>

void HeldLocks::add(const LockEvent& lock) {
    const auto at = std::ranges::upper_bound(m_locks, lock.time, {}, &Held::time);
    const auto index = static_cast<std::size_t>(at - m_locks.begin());
    m_locks.insert(at, Held{.time = lock.time, .lock = lock, .settled = false, .attached = false});
    m_unsettled = std::min(m_unsettled, index);
}

// sudo, console login and polkit's helper make several tries in one
// process, so only the first try after the lock from that process is the
// one that caused it.
HeldLocks::Held* HeldLocks::ownLock(const PasswordTry& passwordTry) {
    if (passwordTry.pid == 0) {
        return nullptr;
    }
    auto near = std::ranges::lower_bound(m_locks, passwordTry.time - sameMoment, {}, &Held::time);
    for (; near != m_locks.end() && near->time <= passwordTry.time + sameMoment; ++near) {
        if (!near->settled && near->lock.pid == passwordTry.pid) {
            return &*near;
        }
    }
    return nullptr;
}

// The lock screen's line comes from its own program just after unix_chkpwd
// wrote the record of the try that locked.
HeldLocks::Held* HeldLocks::lockScreenLock(const PasswordTry& passwordTry) {
    auto near = std::ranges::lower_bound(m_locks, passwordTry.time, {}, &Held::time);
    for (; near != m_locks.end() && near->time <= passwordTry.time + sameMoment; ++near) {
        if (!near->settled && near->lock.place == TryPlace::OwnLogin) {
            return &*near;
        }
    }
    return nullptr;
}

HeldLocks::Held* HeldLocks::nextBefore(qint64 time) {
    while (m_unsettled < m_locks.size() && m_locks[m_unsettled].time < time) {
        Held& held = m_locks[m_unsettled++];
        if (!held.settled) {
            return &held;
        }
    }
    return nullptr;
}

const std::vector<HeldLocks::Held>& HeldLocks::all() const {
    return m_locks;
}
