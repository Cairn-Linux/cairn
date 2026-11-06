// SPDX-License-Identifier: Apache-2.0
#include "NoticeList.h"

#include "HeldLocks.h"

#include <algorithm>
#include <functional>
#include <utility>

namespace {

constexpr qint64 microseconds = 1000000;

} // namespace

NoticeList::NoticeList(std::vector<TryEpisode> fromTries, qint64 failInterval)
    : m_all(std::move(fromTries)), m_fromTries(m_all.size()),
      m_interval(failInterval * microseconds) {}

void NoticeList::addLock(const LockEvent& lock) {
    const auto fromTries = m_all.begin() + static_cast<std::ptrdiff_t>(m_fromTries);
    const auto explained = std::find_if(m_all.begin(), fromTries, [&](const TryEpisode& episode) {
        return episode.wrong > 0 && !episode.lockTime && episode.account &&
               episode.account->uid == lock.account.uid && episode.place == lock.place &&
               episode.first <= lock.time + HeldLocks::sameMoment &&
               episode.last >= lock.time - m_interval;
    });
    if (explained != fromTries) {
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
    m_all.push_back(alone);
}

NoticeList::Notices NoticeList::take(int maxNotices) {
    Notices result;
    result.shown = std::move(m_all);
    std::vector<TryEpisode>& all = result.shown;
    std::ranges::stable_sort(all, std::ranges::greater{}, &TryEpisode::last);
    if (std::cmp_greater(all.size(), maxNotices)) {
        result.notShown = static_cast<int>(all.size()) - maxNotices;
        all.resize(maxNotices);
    }
    return result;
}
