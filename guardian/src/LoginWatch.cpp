// SPDX-License-Identifier: Apache-2.0
#include "LoginWatch.h"

#include <utility>

LoginWatch::LoginWatch(Read read, QObject* parent)
    : QObject(parent), m_read(std::move(read)), m_loggedIn(m_read()) {}

// Only from logind itself, by its well-known name.
void LoginWatch::listen(const QDBusConnection& bus) {
    QDBusConnection connection(bus);
    bool listening = true;
    for (const QString& name : {QStringLiteral("SessionNew"), QStringLiteral("SessionRemoved")}) {
        listening = connection.connect(QStringLiteral("org.freedesktop.login1"),
                                       QStringLiteral("/org/freedesktop/login1"),
                                       QStringLiteral("org.freedesktop.login1.Manager"), name,
                                       QStringLiteral("so"), this, SLOT(sessionsChanged())) &&
                    listening;
    }
    if (!listening) {
        qWarning("cairn-guardian: no system bus, so the Games page notices a child logging in "
                 "or out only when it is shown again.");
    }
}

bool LoginWatch::isLoggedIn(const QString& login) const {
    return m_loggedIn.contains(login);
}

QSet<QString> LoginWatch::loggedIn() const {
    return m_loggedIn;
}

void LoginWatch::refresh() {
    QSet<QString> now = m_read();
    if (now != m_loggedIn) {
        m_loggedIn = std::move(now);
        emit changed();
    }
}

void LoginWatch::sessionsChanged() {
    refresh();
}
