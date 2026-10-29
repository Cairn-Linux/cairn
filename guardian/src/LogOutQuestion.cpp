// SPDX-License-Identifier: Apache-2.0
#include "LogOutQuestion.h"

#include "GameChoices.h"
#include "WaitingGames.h"

#include <algorithm>

LogOutQuestion::LogOutQuestion(QObject* parent) : QObject(parent) {}

LogOutQuestion::Step LogOutQuestion::step() const {
    return m_step;
}

QString LogOutQuestion::login() const {
    return asking() ? m_toAsk.at(m_current).login : QString();
}

QString LogOutQuestion::name() const {
    return asking() ? m_toAsk.at(m_current).name : QString();
}

bool LogOutQuestion::asking() const {
    return m_step != Step::NotAsking;
}

// The copy compares the new list with the child's own file, not with a
// waiting list, so a game that only waits is still a game to copy.
std::vector<ListedChild> LogOutQuestion::whoToAsk(const std::vector<ListedChild>& children,
                                                  const GameChoices& choices,
                                                  const WaitingGames& lists,
                                                  const QSet<QString>& shared,
                                                  const LoggedIn& loggedIn) {
    std::vector<ListedChild> copying;
    for (const ListedChild& child : children) {
        const QStringList onFile = lists.saved(child.login);
        const QStringList wanted = choices.wanted(child.login);
        const bool hasGameToCopy = std::ranges::any_of(wanted, [&](const QString& appId) {
            return shared.contains(appId) && !onFile.contains(appId);
        });
        if (choices.changed(child.login) && hasGameToCopy) {
            copying.push_back(child);
        }
    }
    if (copying.empty()) {
        return {};
    }
    const QSet<QString> logins = loggedIn();
    std::erase_if(copying,
                  [&logins](const ListedChild& child) { return !logins.contains(child.login); });
    return copying;
}

void LogOutQuestion::ask(const std::vector<ListedChild>& children) {
    if (asking()) {
        return;
    }
    m_toAsk = children;
    m_current = 0;
    m_logOut.clear();
    if (m_toAsk.empty()) {
        emit decided({});
        return;
    }
    m_step = Step::Choosing;
    emit changed();
}

void LogOutQuestion::logOutNow() {
    if (m_step == Step::Choosing) {
        m_step = Step::Warning;
        emit changed();
    }
}

void LogOutQuestion::later() {
    if (m_step == Step::Choosing) {
        next();
    }
}

void LogOutQuestion::logOut() {
    if (m_step == Step::Warning) {
        m_logOut.append(login());
        next();
    }
}

void LogOutQuestion::notNow() {
    if (m_step == Step::Warning) {
        m_step = Step::Choosing;
        emit changed();
    }
}

// The question closes before the Save starts, so the page shows the Save
// and not a question that has been answered.
void LogOutQuestion::next() {
    ++m_current;
    if (m_current < m_toAsk.size()) {
        m_step = Step::Choosing;
        emit changed();
        return;
    }
    m_step = Step::NotAsking;
    m_toAsk.clear();
    m_current = 0;
    emit changed();
    emit decided(m_logOut);
}
