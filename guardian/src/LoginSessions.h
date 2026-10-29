// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QList>
#include <QSet>
#include <QString>

#include <optional>

// Who is logged in now, as systemd-logind sees it, read without root
// (ADR-0041), for the Games page to tell which children are logged in.
//
// It counts a login by the visit helper's own rule (cairn-steam-visit's
// child_logged_in), so a child counts exactly when their copy would be held:
// every session of the account counts, a closing one too, but its service
// manager's and a Steam visit's.
namespace LoginSessions {

// One session, as logind lists it.
struct Session {
    QString login;
    // logind's class: user, greeter, manager, background ...
    QString kind;
    // The PAM service that opened it, such as sddm or cairn-steam-visit.
    QString service;
};

// Whether this session is a login of the account's own.
bool isLogin(const Session& session);

// The accounts with a login among these sessions.
QSet<QString> loggedIn(const QList<Session>& sessions);

// Every session logind has now, closing ones included, from
// org.freedesktop.login1 on the system bus, which any account may read.
// Nothing when logind cannot be asked.
std::optional<QList<Session>> fromLogind();

// The accounts logged in now. When logind cannot be asked, nobody counts,
// and the journal says so; a Save still holds a logged-in child's copy.
QSet<QString> loggedInNow();

} // namespace LoginSessions
