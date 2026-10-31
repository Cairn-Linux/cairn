// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QDeadlineTimer>
#include <QObject>
#include <QString>
#include <QTimer>

// The Guardian's own Steam, which closes while a child's Steam is open and
// starts again after (ADR-0032). Then only one Steam is on the desktop,
// with no window to mix up and no second client in the shared library.
//
// It runs as the Guardian, from the Guardian's window: the root helper never
// starts or stops anything in the Guardian's session.
class OwnSteam : public QObject {
    Q_OBJECT

public:
    struct Programs {
        // Asked to shut down; it saves first.
        QString steam = QStringLiteral("/usr/bin/steam");
        // Started with -silent afterwards, as Bazzite's autostart does. If it
        // is missing, the plain client is.
        QString atLogin = QStringLiteral("/usr/bin/bazzite-steam");
        // Where running programs are listed.
        QString proc = QStringLiteral("/proc");
        // How long Steam gets to save and quit (18 s was seen).
        int shutdownMilliseconds = 30000;
    };

    OwnSteam(uint uid, Programs programs, QObject* parent = nullptr);

    // A Steam client of this account is running.
    bool running() const;

    // Asks Steam to shut down if it is running. paused() says when it has
    // gone, or that it was still there when the time ran out.
    void pause();
    // Starts Steam again, only if pause() asked it to shut down and it has
    // gone. True if it did. A Steam that would not close stays asked: once
    // it is quit some other way, the next resume() starts it (#217).
    bool resume();

signals:
    void paused(bool closed);

private:
    void check();

    uint m_uid;
    Programs m_programs;
    QTimer m_poll;
    QDeadlineTimer m_deadline;
    bool m_askedToShutDown = false;
};
