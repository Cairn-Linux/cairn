// SPDX-License-Identifier: Apache-2.0
#include "TryNotices.h"

#include <iterator>
#include <limits>

namespace {

constexpr qint64 microseconds = 1000000;
constexpr qint64 sameMoment = HeldLocks::sameMoment;
// A right password, then the account part, in one process.
constexpr qint64 accountPartWithin = 60 * microseconds;

} // namespace

TryNotices::TryNotices(std::optional<FaillockSettings> settings, int maxNotices)
    : m_rule(settings.value_or(FaillockSettings())),
      m_follow(settings && settings->unlockTime != 0 && settings->deny > 0),
      m_maxNotices(maxNotices), m_episodes(m_rule.failInterval) {}

// faillock never locks root with the settings FaillockSettings accepts.
bool TryNotices::follows(const Account& account) const {
    return m_follow && account.uid != 0;
}

TryNotices::AccountCount& TryNotices::countOf(const Account& account) {
    const auto [found, added] = m_counts.try_emplace(account.uid);
    if (added) {
        found->second.tally = FaillockTally(m_rule);
    }
    return found->second;
}

void TryNotices::lockScreenUnread(unsigned int uid) {
    m_unreadLockScreens.insert(uid);
}

void TryNotices::addLock(const LockEvent& lock) {
    countOf(lock.account).locks.add(lock);
}

void TryNotices::settleBefore(AccountCount& count, qint64 time) {
    while (HeldLocks::Held* held = count.locks.nextBefore(time)) {
        settle(count, *held);
    }
}

// A lock no try's process explains: tries the count did not see, such as
// wrong fingers, made it. faillock locks only an account it does not count
// as locked, so a lock inside one it counts means the count was emptied
// out of sight, by faillock --reset. No password try made this lock, so
// the tries refused before it stay refused: wrong fingers may have been
// all of it. The lock screen's lock goes with the tries in that login that
// ended no longer than fail_interval before it; any other waits for
// notices(). A child can fake the lock screen's line about their own
// account by the thousand, so each is settled without looking through the
// others.
void TryNotices::settle(AccountCount& count, HeldLocks::Held& held) {
    held.settled = true;
    const qint64 at = held.time;
    if (follows(held.lock.account)) {
        if (count.tally.refuses(at)) {
            if (at - count.tally.lockStart() <= sameMoment) {
                // The lock the tries made, said twice.
                held.attached = true;
                return;
            }
            count.tally.reset();
        }
        count.tally.lockAt(at);
        count.refused.clear();
    }
    if (held.lock.place != TryPlace::OwnLogin) {
        return;
    }
    if (TryEpisode* episode = m_episodes.openWithoutLock(held.lock.account, held.lock.place, at)) {
        episode->lockTime = at;
        held.attached = true;
    }
}

// The count was emptied out of sight, at a time the journal does not say,
// so tries counted as refused since the last lock may have been wrong
// passwords after all. Only as many as the lock at `time` needs, at most
// `most`, the newest within fail_interval of it, are counted wrong:
// a try refused is never called wrong without cause.
void TryNotices::undoRefusals(AccountCount& count, qint64 time, int most) {
    const qint64 interval = m_rule.failInterval * microseconds;
    auto from = count.refused.end();
    for (int taken = 0; taken < most && from != count.refused.begin(); ++taken) {
        if (std::prev(from)->time < time - interval) {
            break;
        }
        --from;
    }
    for (; from != count.refused.end(); ++from) {
        m_episodes.countAsWrong(from->episode, from->time);
    }
    count.refused.clear();
}

// The try through faillock's rule, with the journal's own lock, if any,
// as the last word. `own` is the lock its process wrote, or nothing.
TryNotices::Counted TryNotices::countTry(AccountCount& count, const PasswordTry& passwordTry,
                                         HeldLocks::Held* own) {
    using Outcome = FaillockTally::Outcome;
    if (own != nullptr && count.tally.refuses(passwordTry.time)) {
        // This try was the last of deny, so deny less one before it.
        undoRefusals(count, passwordTry.time, m_rule.deny - 1);
        count.tally.reset();
    }
    Outcome outcome = count.tally.fail(passwordTry.time);
    const bool announced = outcome == Outcome::Locked || outcome == Outcome::AnnouncedOnly;
    const bool ownLogin = passwordTry.place == TryPlace::OwnLogin;
    if (own == nullptr && announced && ownLogin) {
        own = count.locks.lockScreenLock(passwordTry);
        if (own != nullptr) {
            own->settled = true;
        }
    }
    if (own != nullptr && outcome == Outcome::AnnouncedOnly) {
        // faillock wrote that it locked, and by its own rule it did not.
        own->attached = true;
        own = nullptr;
    } else if (own != nullptr) {
        count.tally.lockAt(own->time);
    } else if (announced && ownLogin && m_unreadLockScreens.contains(passwordTry.account->uid)) {
        // The lock screen's line may be in the file that could not be
        // read, so the count's word stands: it locked without a word seen.
        outcome = outcome == Outcome::Locked ? Outcome::SilentlyLocked : Outcome::Wrong;
    } else if (announced) {
        // faillock writes it each time, so the count was emptied out of
        // sight before this try.
        count.tally.reset();
        outcome = count.tally.fail(passwordTry.time);
    }
    return Counted{.outcome = outcome, .lock = own};
}

void TryNotices::addTry(const PasswordTry& passwordTry) {
    if (!passwordTry.account) {
        ++m_episodes.at(m_episodes.episodeFor(passwordTry, true)).wrong;
        return;
    }
    using Outcome = FaillockTally::Outcome;
    AccountCount& count = countOf(*passwordTry.account);
    HeldLocks::Held* own = count.locks.ownLock(passwordTry);
    if (own != nullptr) {
        own->settled = true;
    }
    settleBefore(count, passwordTry.time);
    const Counted counted = follows(*passwordTry.account)
                                ? countTry(count, passwordTry, own)
                                : Counted{.outcome = Outcome::Wrong, .lock = own};
    const Outcome outcome = counted.outcome;
    own = counted.lock;
    const bool refused = outcome == Outcome::Refused;
    const std::size_t index =
        refused ? m_episodes.episodeFor(passwordTry, false, count.tally.lockStart())
                : m_episodes.episodeFor(passwordTry, true);
    TryEpisode& episode = m_episodes.at(index);
    if (refused) {
        ++episode.whileLocked;
        count.refused.push_back(Refusal{.episode = index, .time = passwordTry.time});
        return;
    }
    ++episode.wrong;
    if (own != nullptr || outcome == Outcome::SilentlyLocked) {
        episode.lockTime = own != nullptr ? own->time : passwordTry.time;
        if (own != nullptr) {
            own->attached = true;
        }
        count.refused.clear();
    }
}

// A right password is refused while faillock counts the account as locked,
// so one then means the count was emptied out of sight. It says nothing of
// the tries before it, which stay refused.
void TryNotices::addGood(const GoodPassword& good) {
    if (!follows(good.account)) {
        return;
    }
    AccountCount& count = countOf(good.account);
    settleBefore(count, good.time);
    if (!good.accountPart) {
        if (count.tally.refuses(good.time)) {
            count.tally.reset();
            count.refused.clear();
        }
        count.goodPassword = good;
        return;
    }
    const std::optional<GoodPassword>& checked = count.goodPassword;
    if (checked && checked->pid == good.pid && good.time - checked->time <= accountPartWithin) {
        count.tally.reset();
        count.refused.clear();
    }
    count.goodPassword.reset();
}

TryNotices::Notices TryNotices::notices() {
    for (auto& entry : m_counts) {
        settleBefore(entry.second, std::numeric_limits<qint64>::max());
    }
    NoticeList list(m_episodes.all(), m_rule.failInterval);
    for (const auto& entry : m_counts) {
        for (const HeldLocks::Held& held : entry.second.locks.all()) {
            if (!held.attached) {
                list.addLock(held.lock);
            }
        }
    }
    return list.take(m_maxNotices);
}
