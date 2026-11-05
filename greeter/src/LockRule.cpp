// SPDX-License-Identifier: Apache-2.0
#include "LockRule.h"

#include <algorithm>

LockRule::Result LockRule::decide(const QList<qint64>& failureTimes,
                                  const FaillockSettings& settings, qint64 now) {
    // deny = 0 never locks. With fail_interval = 0 no try is ever inside
    // the window, so nothing locks either.
    if (settings.deny == 0 || settings.failInterval == 0) {
        return {.kind = Kind::Open, .secondsLeft = 0};
    }

    // check_tally: the window is counted back from the newest try, not
    // from now.
    qint64 latest = 0;
    for (const qint64 time : failureTimes) {
        latest = std::max(latest, time);
    }
    const auto failures = std::count_if(failureTimes.begin(), failureTimes.end(), [&](qint64 time) {
        return latest - time < settings.failInterval;
    });

    if (failures >= settings.deny) {
        if (settings.unlockTime == 0) {
            return {.kind = Kind::LockedUntilReset, .secondsLeft = 0};
        }
        // faillock lets a try in only once latest + unlock_time < now.
        const qint64 sinceLatest = now - latest;
        if (sinceLatest < 0) {
            return {.kind = Kind::LockedForUnknownTime, .secondsLeft = 0};
        }
        if (sinceLatest <= settings.unlockTime) {
            return {.kind = Kind::Locked, .secondsLeft = settings.unlockTime - sinceLatest};
        }
        // The lock has run out. write_tally forgets every old try at the
        // next wrong one, so that one counts as the first.
        return {.kind = settings.deny == 1 ? Kind::OneTryLeft : Kind::Open, .secondsLeft = 0};
    }

    // write_tally forgets tries older than fail_interval counted back from
    // now, and from a clock set back, before it adds the next one. The next
    // wrong try locks if what is left, plus that one, reaches deny.
    const auto stillCounted =
        std::count_if(failureTimes.begin(), failureTimes.end(), [&](qint64 time) {
            return time <= now && now - time < settings.failInterval;
        });
    return {.kind = stillCounted + 1 >= settings.deny ? Kind::OneTryLeft : Kind::Open,
            .secondsLeft = 0};
}
