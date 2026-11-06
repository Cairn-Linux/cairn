// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "PasswordTry.h"

#include <QHash>
#include <QString>

#include <cstddef>
#include <optional>
#include <vector>

// One notice on the Notices page (ADR-0054): the tries for one account from
// one place, each within faillock's interval of the last, with the lock
// they caused, if any, and the tries refused while it held. A wrong try
// after that lock ended starts another notice. A lock no try explains,
// such as one from wrong fingers at the lock screen, is a notice of its
// own with no tries.
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
    // Only ever set with a wrong password, which caused it, or on a notice
    // of a lock alone.
    std::optional<qint64> lockTime;
};

// The notices made from tries so far, and the one still open for each
// account, place and login.
class TryEpisodes {
public:
    // faillock's fail_interval, in seconds.
    explicit TryEpisodes(qint64 failInterval);

    // The notice a try goes in: the open one for its account, place and
    // login, unless the last try there was more than fail_interval before,
    // or the try is wrong and the notice's lock ended before it, or the
    // try was refused under a lock that began at `lockedSince` and the
    // notice's lock is another.
    std::size_t episodeFor(const PasswordTry& passwordTry, bool wrong,
                           std::optional<qint64> lockedSince = std::nullopt);
    TryEpisode& at(std::size_t index);
    const std::vector<TryEpisode>& all() const;
    // A try counted as refused that was wrong after all.
    void countAsWrong(std::size_t index, qint64 time);
    // The open notice of wrong tries without a lock for that account and
    // place, in no other login, whose last try is no more than
    // fail_interval before `time`.
    TryEpisode* openWithoutLock(const Account& account, TryPlace place, qint64 time);

private:
    std::size_t open(const TryEpisode& like, qint64 time);

    qint64 m_interval;
    std::vector<TryEpisode> m_episodes;
    QHash<QString, std::size_t> m_open;
};
