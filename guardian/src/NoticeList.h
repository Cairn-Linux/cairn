// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "PasswordTry.h"
#include "TryEpisodes.h"

#include <cstddef>
#include <vector>

// The Notices page's list (ADR-0054): the notices made from tries, and the
// locks no try took, newest first and no more than the page shows.
class NoticeList {
public:
    struct Notices {
        // Newest first, at most the number take() was given.
        std::vector<TryEpisode> shown;
        // How many older notices there were beyond those.
        int notShown = 0;
    };

    // The notices made from tries; faillock's fail_interval in seconds.
    NoticeList(std::vector<TryEpisode> fromTries, qint64 failInterval);

    // A lock no try took. A lock of the place of a notice of wrong tries,
    // in that notice's time, goes with it; any other is a notice alone.
    // Only notices made from tries are looked through.
    void addLock(const LockEvent& lock);

    // The list, newest first, at most `maxNotices` long.
    Notices take(int maxNotices);

private:
    std::vector<TryEpisode> m_all;
    // How many of m_all, from the front, were made from tries.
    std::size_t m_fromTries;
    qint64 m_interval;
};
