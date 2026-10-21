// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "SteamLaunchLog.h"

#include <QDateTime>
#include <QString>

#include <cstdint>

// Whether the child's Steam client is still starting, or installing an update
// of itself, read from its own two logs (ADR-0037).
//
// Steam's updater writes `bootstrap_log.txt`: `Startup - ...` each time the
// client is started, then `Found pending update`, `Installing update...` and
// `Update complete, launching Steam...` when it updates itself first. Once
// the client has started it runs its own start command, and writes
// `ExecCommandLine: "'.../steam' '-srt-logger-opened' '-silent'"` to
// `console_log.txt`; a tap it held runs in the same second. On the kids' laptop (2026-10-20) an
// update at login took 39 s and the restarted client 18 s more; every tap made during an update was
// lost, and every tap made while Steam only started was kept.
class SteamStartup {
public:
    enum class Now : std::uint8_t {
        // Started, or no start to wait for.
        Ready,
        // Started by the updater, not yet running its own start command.
        Starting,
        // Downloading or installing an update of itself before it starts.
        Updating,
    };

    explicit SteamStartup(QString bootstrapLog = defaultPath(),
                          QString consoleLog = SteamLaunchLog::defaultPath());

    // Where the client is at `now`, from the last part of both logs.
    Now read(const QDateTime& now) const;

    // `bootstrap_log.txt` in the child's own Steam folder.
    static QString defaultPath();

private:
    QString m_bootstrapLog;
    QString m_consoleLog;
};
