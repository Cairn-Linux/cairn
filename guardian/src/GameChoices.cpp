// SPDX-License-Identifier: Apache-2.0
#include "GameChoices.h"

#include <QChar>

namespace {

bool sameGames(const QStringList& one, const QStringList& other) {
    return QSet<QString>(one.begin(), one.end()) == QSet<QString>(other.begin(), other.end());
}

} // namespace

void GameChoices::setSaved(const QString& login, const QStringList& appIds) {
    m_saved.insert(login, appIds);
}

QStringList GameChoices::wanted(const QString& login) const {
    return m_wanted.contains(login) ? m_wanted.value(login) : m_saved.value(login);
}

bool GameChoices::isWanted(const QString& login, const QString& appId) const {
    return wanted(login).contains(appId);
}

bool GameChoices::isSaved(const QString& login, const QString& appId) const {
    return m_saved.value(login).contains(appId);
}

bool GameChoices::changed(const QString& login) const {
    return m_wanted.contains(login);
}

bool GameChoices::anyChanged() const {
    return !m_wanted.isEmpty();
}

QStringList GameChoices::changedAmong(const QStringList& logins) const {
    QStringList changedOnes;
    for (const QString& login : logins) {
        if (changed(login)) {
            changedOnes.append(login);
        }
    }
    return changedOnes;
}

// A game turned on goes at the end, so the order a parent chose them in is
// kept; the command sorts the list by title.
void GameChoices::turn(const QString& login, const QString& appId) {
    QStringList games = wanted(login);
    if (!games.removeOne(appId)) {
        games.append(appId);
    }
    if (sameGames(games, m_saved.value(login))) {
        m_wanted.remove(login);
    } else {
        m_wanted.insert(login, games);
    }
}

void GameChoices::putBack(const QString& login) {
    m_wanted.remove(login);
}

// cairn-family takes every --log-out before the first list.
QStringList GameChoices::saveArguments(const QStringList& logins, const QStringList& logOut) const {
    QStringList arguments{QStringLiteral("save-games")};
    for (const QString& login : logins) {
        if (m_wanted.contains(login) && logOut.contains(login)) {
            arguments.append(QStringLiteral("--log-out=") + login);
        }
    }
    for (const QString& login : logins) {
        if (m_wanted.contains(login)) {
            arguments.append(login + QLatin1Char('=') +
                             m_wanted.value(login).join(QLatin1Char(',')));
        }
    }
    return arguments;
}

void GameChoices::keepOnly(const QStringList& logins, const QSet<QString>& shared) {
    QHash<QString, QStringList> kept;
    for (const QString& login : logins) {
        if (!m_wanted.contains(login)) {
            continue;
        }
        const QStringList& saved = m_saved.value(login);
        QStringList games;
        for (const QString& appId : m_wanted.value(login)) {
            if (shared.contains(appId) || saved.contains(appId)) {
                games.append(appId);
            }
        }
        if (!sameGames(games, saved)) {
            kept.insert(login, games);
        }
    }
    m_wanted = kept;
}
