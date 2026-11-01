// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "LogTail.h"

#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <QStringList>

#include <cstdint>
#include <optional>

// What the Steam client says it is doing with the game a tile asked for,
// read from its own log (ADR-0036).
//
// Steam writes each step of a launch to `console_log.txt` as it goes:
// `GameAction [AppID 2471140, ActionID 1] : LaunchApp changed task to
// DownloadingDepots with ""`. Before a game's first start in a while it may
// download a game update, the Steam Linux Runtime or Proton, and run the
// game's install script, which on the kids' laptop (2026-10-20) took 50 s
// before the game's own process. Only lines written after the tap count, so
// a launch from before says nothing about this one.
//
// The same lines say whether Steam took the tap at all (ADR-0037). Steam
// writes `ExecCommandLine: "'.../steam' '-applaunch' '2471160'"` the moment
// it does. A tap made while Steam installs an update of itself is lost when
// it restarts, and the new client then writes only its own start command.
//
// A launch whose last step is still DownloadingDepots after Steam took the
// game off its download schedule has stopped part-way (#206, SteamStall).
class SteamLaunchLog {
public:
    enum class Step : std::uint8_t {
        // Nothing about this game since the tap: Steam has not taken it up.
        Nothing,
        // A step that ends by itself: downloading, an install script.
        Working,
        // Steam asks something on a window the kiosk hides, such as the
        // cloud-sync question when offline. Nothing moves until it is answered.
        WaitingForAnswer,
        // Steam refused the launch, such as when another game runs.
        Refused,
        // Steam has started the game's own process; its window comes next.
        GameStarted,
    };

    explicit SteamLaunchLog(QString path = defaultPath());

    // A Steam tile was pressed: only what the log gains from now counts.
    void startLaunch(const QString& appId);
    // Reads what the log has gained since last time, at `now`, and says
    // where the launch has got to. The last line about the game decides.
    Step read(const QDateTime& now);
    // Since the tap, Steam's log has named `-applaunch <appid>`: Steam took
    // the tap. As of the last read().
    bool reachedSteam() const;
    // When a Steam client that started after the tap ran its own start
    // command, as of the last read(), or nothing. Steam takes any tap it held
    // in the same second.
    std::optional<QDateTime> steamStartedAt() const;
    // Since the tap, a new Steam client has started, as of the last read():
    // the log names a client's own start command, with the tap or without
    // it, or Steam began a new log. A new client holds nothing an earlier one
    // was asking about (#211).
    bool clientStarted() const;
    // Where the launch had got to at the last read().
    Step step() const;
    // The last step about the game since the tap was DownloadingDepots, as
    // of the last read().
    bool downloading() const;
    // The last step about the game since the tap was the question Steam
    // asks when it could not check the game's Steam Cloud saves,
    // `SynchronizingCloud "syncfailed"`, as of the last read() (#211).
    bool cloudSyncFailed() const;

    // The app id in `steam -applaunch <appid>`, or empty for anything else.
    static QString appIdOf(const QStringList& exec);
    // What one line of the log says about the game, or Nothing when the
    // line is not about it.
    static Step stepOf(const QByteArray& line, const QString& appId);
    // `console_log.txt` in the child's own Steam folder.
    static QString defaultPath();
    // The moment a line of Steam's logs starts with, such as
    // `[2026-10-20 21:32:29]`, for a line read at `now`, or nothing. Steam
    // writes the local time with no zone, so in the hour that comes twice
    // when the clocks go back, each time means two moments an hour apart.
    // A line is written before it is read, so it is the later of them that
    // is not after `now`, or the earlier when both are, as after the clock
    // was set back (#248).
    static std::optional<QDateTime> timeOf(const QByteArray& line, const QDateTime& now);

private:
    LogTail m_tail;
    QString m_appId;
    Step m_step = Step::Nothing;
    bool m_downloading = false;
    bool m_cloudSyncFailed = false;
    bool m_reached = false;
    bool m_clientStarted = false;
    std::optional<QDateTime> m_steamStartedAt;
};
