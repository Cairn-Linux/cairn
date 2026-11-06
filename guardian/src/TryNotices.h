// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "FaillockSettings.h"
#include "FaillockTally.h"
#include "HeldLocks.h"
#include "PasswordTry.h"
#include "TryEpisodes.h"

#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Groups tries and locks into notices as journalctl gives them, oldest
// first, so a flood of tries takes no more room than its count (ADR-0054).
// Each account's tries go through faillock's own rule (FaillockTally), so a
// try faillock refused because the account was locked is never called
// wrong, and a lock faillock made without a word is still a lock. Locks
// must come before the tries they follow: journalctl gives faillock's
// audit record before the record of the try that caused it, and the lock
// screen's locks are read before any try.
class TryNotices {
public:
    // Without faillock's settings, or with unlock_time "never", tries are
    // grouped by faillock's defaults and no lock is worked out from them:
    // only the journal's locks show, and no try counts as made while
    // locked.
    explicit TryNotices(std::optional<FaillockSettings> settings, int maxNotices = 200);

    // The lock screen's locks for this account are in a journal file that
    // could not be read. A lock the count works out there is then a lock,
    // where otherwise a missing line would mean the count was emptied out
    // of sight.
    void lockScreenUnread(unsigned int uid);
    void addLock(const LockEvent& lock);
    void addTry(const PasswordTry& passwordTry);
    void addGood(const GoodPassword& good);

    struct Notices {
        // Newest first, at most maxNotices.
        std::vector<TryEpisode> shown;
        // How many older notices there were beyond those.
        int notShown = 0;
    };

    // The notices so far, after the locks no try came after.
    Notices notices();

private:
    struct Refusal {
        std::size_t episode = 0;
        qint64 time = 0;
    };

    // What the journal says of one account's count.
    struct AccountCount {
        FaillockTally tally;
        HeldLocks locks;
        // Each try counted as refused since the last lock.
        std::vector<Refusal> refused;
        // The last right password, until the account part follows it.
        std::optional<GoodPassword> goodPassword;
    };

    bool follows(const Account& account) const;
    AccountCount& countOf(const Account& account);
    // What faillock made of a try, and the journal's lock it caused.
    struct Counted {
        FaillockTally::Outcome outcome = FaillockTally::Outcome::Wrong;
        HeldLocks::Held* lock = nullptr;
    };

    Counted countTry(AccountCount& count, const PasswordTry& passwordTry, HeldLocks::Held* own);
    void settleBefore(AccountCount& count, qint64 time);
    void settle(AccountCount& count, HeldLocks::Held& held);
    void undoRefusals(AccountCount& count, qint64 time, int most);
    void attachOrAdd(const LockEvent& lock, std::vector<TryEpisode>& all) const;

    FaillockSettings m_rule;
    bool m_follow;
    int m_maxNotices;
    TryEpisodes m_episodes;
    // Each account's count, by uid.
    std::unordered_map<unsigned int, AccountCount> m_counts;
    // Accounts whose lock screen's locks could not be read, by uid.
    std::unordered_set<unsigned int> m_unreadLockScreens;
};
