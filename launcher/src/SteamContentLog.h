// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "LogTail.h"

#include <QByteArray>
#include <QDateTime>
#include <QSet>
#include <QString>

#include <optional>

// What Steam's downloads for a tapped game are doing, read from its own
// content log (#206).
//
// Steam writes what it downloads to `content_log.txt`. When a launch needs
// something first, Steam puts the game, and each tool the game runs in, at
// the front of its schedule (`AppID 1070560 scheduler update : Priority
// First`), downloads what each needs (`update started`, then `finished
// update`), and takes each off the schedule again: `AppID 292780 scheduler
// finished : removed from schedule (result Suspended, state 0xc)`. Every run
// ends with that scheduler line, a shader cache's too, which never writes
// `finished update`. When the launch goes on, Steam writes its next step in
// the same second as the game's removal.
//
// On the test VM (2026-10-30) a new child's Steam fetched one of the two
// tools a Linux game needs, took the game off its schedule, and did nothing
// more. Other games' downloads at a lower priority carry on after a launch's
// apps leave the schedule, so they do not count. Only lines written since
// the tap, or since Steam was started again, count.
class SteamContentLog {
public:
    explicit SteamContentLog(QString path = defaultPath());

    // Only what the log gains from now counts: at a tap, and again when Steam
    // is started again. Empty for anything but a Steam game.
    void startLaunch(const QString& appId);
    // Reads what the log has gained since last time, at `now`.
    void read(const QDateTime& now);
    // When Steam took the game off its schedule, if that is the last thing it
    // did for the launch and none of the launch's downloads still runs, as of
    // the last read(); otherwise nothing.
    std::optional<QDateTime> offScheduleAt() const;

    // `content_log.txt` in the child's own Steam folder.
    static QString defaultPath();

private:
    LogTail m_tail;
    QByteArray m_appId;
    std::optional<QDateTime> m_offSchedule;
    // The game, and the apps Steam put at the front of its schedule since the
    // tap: the tools the launch needs.
    QSet<QByteArray> m_launchApps;
    // Those of them with a download started and not yet ended.
    QSet<QByteArray> m_downloading;
};
