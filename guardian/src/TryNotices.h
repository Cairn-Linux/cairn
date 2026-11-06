// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "FaillockSettings.h"
#include "PasswordTry.h"

#include <QHash>
#include <QString>

#include <optional>
#include <vector>

// One notice on the Notices page (ADR-0054): the tries for one account from
// one place, each within faillock's interval of the last, with the lock
// they caused, if any. A lock no try explains, such as one from wrong
// fingers at the lock screen, is a notice of its own with no tries.
struct TryEpisode {
    std::optional<Account> account;
    TryPlace place = TryPlace::Other;
    QString program;
    std::optional<Account> from;
    qint64 first = 0;
    qint64 last = 0;
    // Wrong passwords, and tries refused because the account was locked,
    // which may have been the right password.
    int wrong = 0;
    int whileLocked = 0;
    std::optional<qint64> lockTime;
};

// Groups tries and locks into notices as journalctl gives them, oldest
// first, so a flood of tries takes no more room than its count. Locks must
// come before the tries they follow: journalctl gives faillock's audit
// record before the record of the try that caused it, and the lock screen's
// locks are read before any try.
class TryNotices {
public:
    // Without faillock's settings, faillock's defaults group the tries, but
    // no try counts as made while locked.
    explicit TryNotices(std::optional<FaillockSettings> settings, int maxNotices = 200);

    void addLock(const LockEvent& lock);
    void addTry(const PasswordTry& passwordTry);

    struct Notices {
        // Newest first, at most maxNotices.
        std::vector<TryEpisode> shown;
        // How many older notices there were beyond those.
        int notShown = 0;
    };

    Notices notices() const;

private:
    struct HeldLock {
        qint64 time = 0;
        LockEvent lock;
        // The try that caused it was seen: the first try after it from
        // its process.
        bool claimed = false;
        // That try's notice took it as its lock.
        bool attached = false;
    };

    HeldLock* claimOwnLock(const PasswordTry& passwordTry);
    bool whileLocked(const PasswordTry& passwordTry, const HeldLock* own) const;
    void attachOrAdd(const LockEvent& lock, std::vector<TryEpisode>& all) const;

    FaillockSettings m_rule;
    bool m_known;
    int m_maxNotices;
    std::vector<TryEpisode> m_episodes;
    // Each account's locks, oldest first.
    QHash<unsigned int, std::vector<HeldLock>> m_locks;
    // The episode still open for each account, place and login.
    QHash<QString, std::size_t> m_open;
};
