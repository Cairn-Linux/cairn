// SPDX-License-Identifier: Apache-2.0
#include "LoginSessions.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QDBusVariant>

namespace {

QString logind() {
    return QStringLiteral("org.freedesktop.login1");
}

// One property of a session object, or "" when it cannot be read.
QString property(const QDBusConnection& bus, const QString& path, const QString& name) {
    QDBusMessage call = QDBusMessage::createMethodCall(
        logind(), path, QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("Get"));
    call << QStringLiteral("org.freedesktop.login1.Session") << name;
    const QDBusReply<QDBusVariant> reply = bus.call(call);
    return reply.isValid() ? reply.value().variant().toString() : QString();
}

} // namespace

// A session whose class could not be read has gone since it was listed.
bool LoginSessions::isLogin(const Session& session) {
    return !session.kind.isEmpty() && !session.kind.startsWith(QStringLiteral("manager")) &&
           session.service != QStringLiteral("cairn-steam-visit");
}

QSet<QString> LoginSessions::loggedIn(const QList<Session>& sessions) {
    QSet<QString> logins;
    for (const Session& session : sessions) {
        if (isLogin(session)) {
            logins.insert(session.login);
        }
    }
    return logins;
}

// ListSessions answers a(susso): the session's id, the account's uid and
// name, the seat and the session's object path.
std::optional<QList<LoginSessions::Session>> LoginSessions::fromLogind() {
    const QDBusConnection bus = QDBusConnection::systemBus();
    if (!bus.isConnected()) {
        return std::nullopt;
    }
    const QDBusMessage reply = bus.call(QDBusMessage::createMethodCall(
        logind(), QStringLiteral("/org/freedesktop/login1"),
        QStringLiteral("org.freedesktop.login1.Manager"), QStringLiteral("ListSessions")));
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().size() != 1 ||
        reply.signature() != QStringLiteral("a(susso)")) {
        return std::nullopt;
    }

    QList<Session> sessions;
    const auto list = reply.arguments().constFirst().value<QDBusArgument>();
    list.beginArray();
    while (!list.atEnd()) {
        QString id;
        uint uid = 0;
        QString login;
        QString seat;
        QDBusObjectPath path;
        list.beginStructure();
        list >> id >> uid >> login >> seat >> path;
        list.endStructure();
        sessions.append({.login = login,
                         .kind = property(bus, path.path(), QStringLiteral("Class")),
                         .service = property(bus, path.path(), QStringLiteral("Service"))});
    }
    list.endArray();
    return sessions;
}

QSet<QString> LoginSessions::loggedInNow() {
    const std::optional<QList<Session>> sessions = fromLogind();
    if (!sessions) {
        qWarning("cairn-guardian: logind could not be asked who is logged in, so every child "
                 "counts as logged out; a Save still holds a logged-in child's copy.");
        return {};
    }
    return loggedIn(*sessions);
}
