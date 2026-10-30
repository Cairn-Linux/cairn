// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QDBusConnection>
#include <QObject>
#include <QSet>
#include <QString>

#include <functional>

// Which children are logged in, kept up to date for the Games page
// (ADR-0041). It reads who is logged in with LoginSessions' rule, and reads
// it again whenever systemd-logind says a session came or went, so the page
// greys a child's games while they are logged in and shows them again once
// their last session has gone.
class LoginWatch : public QObject {
    Q_OBJECT

public:
    using Read = std::function<QSet<QString>()>;

    explicit LoginWatch(Read read, QObject* parent = nullptr);

    // Listens for logind's SessionNew and SessionRemoved on this bus: the
    // system bus in the window, a bus of the test's own in a test. Without
    // one it says so in the journal, and the page still reads again each
    // time it is shown and after a logout.
    void listen(const QDBusConnection& bus);

    bool isLoggedIn(const QString& login) const;
    QSet<QString> loggedIn() const;

    // Reads who is logged in now.
    void refresh();

signals:
    // Only when someone logged in or out.
    void changed();

private slots:
    void sessionsChanged();

    // NOLINTNEXTLINE(readability-redundant-access-specifiers): moc needs the slot section
private:
    Read m_read;
    QSet<QString> m_loggedIn;
};
