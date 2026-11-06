// SPDX-License-Identifier: Apache-2.0
#include "TryNotices.h"

#include <QStringList>

#include <algorithm>
#include <cstdlib>
#include <utility>

namespace {

constexpr qint64 microseconds = 1000000;
// faillock's record of the try that locked, and the try's own record, come
// from one process within moments of each other.
constexpr qint64 sameMoment = 5 * microseconds;

QString keyOf(const PasswordTry& passwordTry) {
    return QStringList{
        passwordTry.account ? passwordTry.account->login : QString(),
        QString::number(static_cast<int>(passwordTry.place)),
        passwordTry.program,
        passwordTry.from ? QString::number(passwordTry.from->uid) : QString(),
    }
        .join(QLatin1Char('\n'));
}

} // namespace

TryNotices::TryNotices(std::optional<FaillockSettings> settings, int maxNotices)
    : m_rule(settings.value_or(FaillockSettings())), m_known(settings.has_value()),
      m_maxNotices(maxNotices) {}

void TryNotices::addLock(const LockEvent& lock) {
    // The lock screen's locks come from a run of their own, so they may sit
    // among the others out of order.
    std::vector<HeldLock>& locks = m_locks[lock.account.uid];
    const auto at = std::ranges::upper_bound(locks, lock.time, {}, &HeldLock::time);
    locks.insert(at, HeldLock{.time = lock.time, .lock = lock, .attached = false});
}

// A try in the minutes a lock lasted, other than the one that locked, was
// refused whatever was typed. faillock locks an account only while it is
// not locked, so only the last lock before the try, or the one before it
// when that last lock was this try's own, can still hold. With unlock_time
// "never", how long a lock lasted depends on when someone reset it, which
// the journal does not say, so no try counts as made while locked.
bool TryNotices::whileLocked(const PasswordTry& passwordTry) const {
    if (!m_known || !passwordTry.account || m_rule.unlockTime == 0) {
        return false;
    }
    const auto found = m_locks.constFind(passwordTry.account->uid);
    if (found == m_locks.cend()) {
        return false;
    }
    const std::vector<HeldLock>& locks = *found;
    auto before = std::ranges::lower_bound(locks, passwordTry.time, {}, &HeldLock::time);
    const qint64 lasted = m_rule.unlockTime * microseconds;
    for (int checked = 0; checked < 2 && before != locks.begin(); ++checked) {
        --before;
        if (before->lock.pid != passwordTry.pid) {
            return passwordTry.time <= before->time + lasted;
        }
    }
    return false;
}

// faillock's record of the lock and the record of the try that caused it
// come from one process, within moments of each other.
void TryNotices::attachByProcess(const PasswordTry& passwordTry, TryEpisode& episode) {
    if (!passwordTry.account || passwordTry.pid == 0 || episode.lockTime) {
        return;
    }
    const auto found = m_locks.find(passwordTry.account->uid);
    if (found == m_locks.end()) {
        return;
    }
    std::vector<HeldLock>& locks = *found;
    auto near = std::ranges::lower_bound(locks, passwordTry.time - sameMoment, {}, &HeldLock::time);
    for (; near != locks.end() && near->time <= passwordTry.time + sameMoment; ++near) {
        if (!near->attached && near->lock.pid == passwordTry.pid) {
            near->attached = true;
            episode.lockTime = near->time;
            return;
        }
    }
}

void TryNotices::addTry(const PasswordTry& passwordTry) {
    const QString key = keyOf(passwordTry);
    const auto open = m_open.constFind(key);
    const qint64 interval = m_rule.failInterval * microseconds;
    if (open == m_open.cend() || passwordTry.time - m_episodes[*open].last > interval) {
        TryEpisode episode;
        episode.account = passwordTry.account;
        episode.place = passwordTry.place;
        episode.program = passwordTry.program;
        episode.from = passwordTry.from;
        episode.first = passwordTry.time;
        episode.last = passwordTry.time;
        m_episodes.push_back(episode);
        m_open.insert(key, m_episodes.size() - 1);
    }
    TryEpisode& episode = m_episodes[m_open.value(key)];
    episode.last = std::max(episode.last, passwordTry.time);
    if (whileLocked(passwordTry)) {
        ++episode.whileLocked;
    } else {
        ++episode.wrong;
    }
    attachByProcess(passwordTry, episode);
}

// A lock no try's process explains. The lock screen's lock follows tries
// in the person's own login, made by a program of its own, so it goes with
// the tries there that ended just before it. Any other is a notice alone.
void TryNotices::attachOrAdd(const LockEvent& lock, std::vector<TryEpisode>& all) const {
    const qint64 interval = m_rule.failInterval * microseconds;
    const auto explained = std::ranges::find_if(all, [&](const TryEpisode& episode) {
        return !episode.lockTime && episode.account && episode.account->uid == lock.account.uid &&
               episode.place == lock.place && episode.last <= lock.time + sameMoment &&
               episode.last >= lock.time - interval;
    });
    if (explained != all.end()) {
        explained->lockTime = lock.time;
        return;
    }
    TryEpisode alone;
    alone.account = lock.account;
    alone.place = lock.place;
    alone.program = lock.program;
    alone.first = lock.time;
    alone.last = lock.time;
    alone.lockTime = lock.time;
    all.push_back(alone);
}

TryNotices::Notices TryNotices::notices() const {
    Notices result;
    std::vector<TryEpisode>& all = result.shown;
    all = m_episodes;
    for (const std::vector<HeldLock>& locks : m_locks) {
        for (const HeldLock& held : locks) {
            if (!held.attached) {
                attachOrAdd(held.lock, all);
            }
        }
    }
    std::ranges::stable_sort(all, std::ranges::greater{}, &TryEpisode::last);
    if (std::cmp_greater(all.size(), m_maxNotices)) {
        result.notShown = static_cast<int>(all.size()) - m_maxNotices;
        all.resize(m_maxNotices);
    }
    return result;
}
