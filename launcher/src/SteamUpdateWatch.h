// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "SteamStartup.h"

#include <QDateTime>
#include <QObject>
#include <QQmlEngine>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <optional>

// Whether the child's Steam is updating itself, read while the tiles are in
// front, so the tiles of games that start in Steam can wait (ADR-0061).
//
// Steam is updating itself while its logs read Updating or Restarting
// (SteamStartup) and a process of this user called exactly `steam` runs. A
// log left part-way through an update by a Steam that was stopped reads
// Updating for up to 20 minutes, so the process list is read too, but only
// while the logs say Steam is updating. A plain start does not count: it is
// over within about 20 s at most logins.
//
// While active it reads at once and then every 5 s. While not active it reads
// nothing and keeps its last answer. The journal hears when the answer
// changes, with Steam's own times, so a pause never repeats or stretches a
// line.
class SteamUpdateWatch : public QObject {
    Q_OBJECT
    QML_ELEMENT
    // The tiles are in front. Main.qml binds it.
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    // Steam is updating itself, as of the last read.
    Q_PROPERTY(bool updating READ updating NOTIFY updatingChanged)
    // Where the process list is read from. Tests set a folder of their own;
    // while active, a new one is read at once.
    Q_PROPERTY(QString processRoot READ processRoot WRITE setProcessRoot NOTIFY processRootChanged)

public:
    explicit SteamUpdateWatch(QObject* parent = nullptr);

    bool active() const;
    void setActive(bool active);
    bool updating() const;
    QString processRoot() const;
    void setProcessRoot(const QString& processRoot);

    // Reads Steam's logs, and the process list while they say Steam is
    // updating, as at `now`. The timer reads at the clock's time; tests give
    // their own.
    void readAt(const QDateTime& now);
    // A tile with the command `exec` was pressed. True when it starts a game
    // in Steam and Steam is updating itself by the last read and by a read
    // made now: the game waits, and the journal says so the first time in
    // each update. False otherwise, also when that read says Steam is done.
    bool holdsBack(const QString& title, const QStringList& exec);

signals:
    void activeChanged();
    void updatingChanged();
    void processRootChanged();

private:
    // A process of this user called exactly `steam` runs.
    bool steamRunning() const;
    // Writes the journal's line for the answer turning false.
    void sayWhyItEnded(const SteamStartup::Reading& reading, bool steamRuns) const;

    SteamStartup m_startup;
    QString m_processRoot = QStringLiteral("/proc");
    QTimer m_timer;
    bool m_active = false;
    bool m_updating = false;
    // The updater's last start when this update was first seen.
    std::optional<QDateTime> m_startedAt;
    // The games pressed in this update, so each is in the journal once.
    QSet<QString> m_heldBack;
};
