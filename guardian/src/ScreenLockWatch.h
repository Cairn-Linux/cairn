// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QDBusConnection>
#include <QObject>

// Says when the Guardian's screen locks, so a child's Steam does not stay
// open behind the lock screen, keeping the child from logging in
// (ADR-0032).
//
// Plasma's KWin sends the freedesktop screen saver's ActiveChanged signal on
// the session bus when the lock screen comes up (seen in the VM with
// loginctl lock-session, 2026-10-12).
class ScreenLockWatch : public QObject {
    Q_OBJECT

public:
    // Without a session bus it says so in the journal, and never says the
    // screen locked.
    explicit ScreenLockWatch(const QDBusConnection& bus, QObject* parent = nullptr);

signals:
    void locked();

private slots:
    void activeChanged(bool active);
};
