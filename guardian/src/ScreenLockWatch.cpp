// SPDX-License-Identifier: Apache-2.0
#include "ScreenLockWatch.h"

ScreenLockWatch::ScreenLockWatch(const QDBusConnection& bus, QObject* parent) : QObject(parent) {
    // Only from the program that owns the screen saver's name. KWin sends
    // the signal on two paths; this is the one the freedesktop name uses.
    QDBusConnection connection(bus);
    const bool listening =
        connection.connect(QStringLiteral("org.freedesktop.ScreenSaver"),
                           QStringLiteral("/org/freedesktop/ScreenSaver"),
                           QStringLiteral("org.freedesktop.ScreenSaver"),
                           QStringLiteral("ActiveChanged"), this, SLOT(activeChanged(bool)));
    if (!listening) {
        qWarning("No session bus: locking the screen will not close a child's Steam.");
    }
}

void ScreenLockWatch::activeChanged(bool active) {
    if (active) {
        emit locked();
    }
}
