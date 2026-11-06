// SPDX-License-Identifier: Apache-2.0
#include "TryEpisodes.h"

#include <QStringList>

#include <algorithm>

namespace {

constexpr qint64 microseconds = 1000000;

QString keyOf(const TryEpisode& episode) {
    return QStringList{
        episode.account ? episode.account->login : QString(),
        QString::number(static_cast<int>(episode.place)),
        episode.program,
        episode.from ? QString::number(episode.from->uid) : QString(),
    }
        .join(QLatin1Char('\n'));
}

} // namespace

TryEpisodes::TryEpisodes(qint64 failInterval) : m_interval(failInterval * microseconds) {}

TryEpisode& TryEpisodes::at(std::size_t index) {
    return m_episodes[index];
}

const std::vector<TryEpisode>& TryEpisodes::all() const {
    return m_episodes;
}

// A new notice like `like`, from `time`, now the open one for its key.
std::size_t TryEpisodes::open(const TryEpisode& like, qint64 time) {
    TryEpisode episode;
    episode.account = like.account;
    episode.place = like.place;
    episode.program = like.program;
    episode.from = like.from;
    episode.first = time;
    episode.last = time;
    m_episodes.push_back(episode);
    m_open.insert(keyOf(episode), m_episodes.size() - 1);
    return m_episodes.size() - 1;
}

std::size_t TryEpisodes::episodeFor(const PasswordTry& passwordTry, bool wrong) {
    TryEpisode like;
    like.account = passwordTry.account;
    like.place = passwordTry.place;
    like.program = passwordTry.program;
    like.from = passwordTry.from;
    const auto found = m_open.constFind(keyOf(like));
    if (found == m_open.cend() || passwordTry.time - m_episodes[*found].last > m_interval ||
        (wrong && m_episodes[*found].lockTime)) {
        return open(like, passwordTry.time);
    }
    TryEpisode& episode = m_episodes[*found];
    episode.last = std::max(episode.last, passwordTry.time);
    return *found;
}

// One that was in a notice with a lock came after that lock, so it starts
// the next notice from that place.
void TryEpisodes::countAsWrong(std::size_t index, qint64 time) {
    TryEpisode& was = m_episodes[index];
    --was.whileLocked;
    if (!was.lockTime) {
        ++was.wrong;
        return;
    }
    std::size_t next = m_open.value(keyOf(was));
    if (next == index) {
        next = open(was, time);
    }
    TryEpisode& episode = m_episodes[next];
    episode.first = std::min(episode.first, time);
    episode.last = std::max(episode.last, time);
    ++episode.wrong;
}

TryEpisode* TryEpisodes::openWithoutLock(const Account& account, TryPlace place, qint64 time) {
    TryEpisode like;
    like.account = account;
    like.place = place;
    const auto found = m_open.constFind(keyOf(like));
    if (found == m_open.cend()) {
        return nullptr;
    }
    TryEpisode& episode = m_episodes[*found];
    if (episode.wrong == 0 || episode.lockTime || episode.last < time - m_interval) {
        return nullptr;
    }
    return &episode;
}
