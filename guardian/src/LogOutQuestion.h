// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "ChildList.h"

#include <QObject>
#include <QQmlEngine>
#include <QSet>
#include <QString>
#include <QStringList>

#include <cstdint>
#include <functional>
#include <vector>

class GameChoices;
class WaitingGames;

// The question the Games page asks before a Save when a child with new
// games to copy is logged in (ADR-0041): "Log Ada out now" or "Later", one
// child at a time. "Log Ada out now" first warns that anything Ada has not
// saved in a game may be lost, and asks again: "Log Ada out" or "Not now",
// which goes back to the choice. "Later" logs nobody out: the games wait
// for Ada's next logout. Once every child is answered, the page starts one
// Save, with one password, that names the children to log out.
class LogOutQuestion : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The Games page has one.")
    Q_PROPERTY(Step step READ step NOTIFY changed)
    // The child asked about, or "" when no question is open.
    Q_PROPERTY(QString login READ login NOTIFY changed)
    Q_PROPERTY(QString name READ name NOTIFY changed)

public:
    // No question; the choice between now and later; the warning.
    enum class Step : std::uint8_t { NotAsking, Choosing, Warning };
    Q_ENUM(Step)

    using LoggedIn = std::function<QSet<QString>()>;

    explicit LogOutQuestion(QObject* parent = nullptr);

    Step step() const;
    QString login() const;
    QString name() const;
    bool asking() const;

    // The children to ask about, in the children's order: each with a
    // change to save and a game to copy, a game on their new list that
    // their own list does not hold and the shared folder has, who is
    // logged in. A child whose change only takes games off has nothing to
    // copy, so is never asked about. `loggedIn` is asked only when some
    // child has a game to copy.
    static std::vector<ListedChild> whoToAsk(const std::vector<ListedChild>& children,
                                             const GameChoices& choices, const WaitingGames& lists,
                                             const QSet<QString>& shared, const LoggedIn& loggedIn);

    // Asks about each of these children in turn. With none, it is decided
    // at once, with nobody to log out.
    void ask(const std::vector<ListedChild>& children);

    // The choice: "Log Ada out now" shows the warning; "Later" goes on to
    // the next child.
    Q_INVOKABLE void logOutNow();
    Q_INVOKABLE void later();
    // The warning: "Log Ada out" goes on to the next child, and Ada is
    // logged out; "Not now" goes back to the choice.
    Q_INVOKABLE void logOut();
    Q_INVOKABLE void notNow();

signals:
    void changed();
    // Every child is answered: these are to be logged out, in the order
    // asked.
    void decided(const QStringList& logOut);

private:
    void next();

    std::vector<ListedChild> m_toAsk;
    std::size_t m_current = 0;
    Step m_step = Step::NotAsking;
    QStringList m_logOut;
};
