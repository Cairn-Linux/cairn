// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "SteamLaunchLog.h"
#include "SteamStartup.h"

#include <QElapsedTimer>
#include <QString>
#include <QStringList>

// Whether a Steam game a tile asked for is still on its way, so the launcher
// keeps waiting past its usual grace (ADR-0029, #132).
//
// `steam -applaunch` only hands the game to the Steam client. Right after
// login the client is still starting and takes the game up about 17 s after
// the tap; a game run through Proton can take a minute more before its first
// window. Two facts say Steam is still working on it: the client was started
// at login a short while ago, or Steam's `reaper` for that game is running.
// Either keeps the launch waiting, never past an outer limit, so a game that
// never comes still ends in the tiles.
//
// Steam's own log is a third fact (ADR-0036): while it says Steam is working
// on the game, downloading an update or running its install script, the
// launch waits up to a longer limit, and the starting screen says the game is
// getting ready. The other two facts count only until two minutes after
// Steam last showed work, so a game whose download ends still gets the time a
// Proton game needs for its first window, even one that ends just before the
// longer limit.
//
// Steam starting, or updating itself, counts as work too (ADR-0037). A tap
// made while it updates itself is lost when it restarts, so once the new
// client has started without taking the tap, the launcher sends it again.
class SteamGameWait {
public:
    explicit SteamGameWait(QString processRoot = QStringLiteral("/proc"),
                           QString steamLog = SteamLaunchLog::defaultPath(),
                           QString steamStartupLog = SteamStartup::defaultPath());

    // The kiosk started the Steam client at login (ADR-0027); its startup
    // time is counted from now.
    void steamStartedAtLogin();
    // A tile was pressed. Only `steam -applaunch <appid>` is a Steam game. A
    // reaper for it already running is from an earlier start and never counts.
    void startLaunch(const QStringList& exec);
    // The tile pressed last is a Steam game.
    bool steamGame() const;
    // True while the game is worth waiting for: a Steam game, and either
    // Steam's logs say it is working on it, or starting or updating itself,
    // inside the work limit, or, within the limit since Steam last showed
    // work, Steam is still starting or a new reaper for it is running. Asked
    // when a grace ends, so the wait can run one grace past a limit.
    bool stillComing();
    // At the last stillComing(), Steam's logs said it was working on the
    // game, or starting or updating itself, or a client started since the
    // tap had not taken it yet.
    bool steamWorking() const;
    // True once per tap, at the stillComing() after a Steam client that
    // started since the tap has had a moment to take it and has not: the tap
    // was lost and should be sent again. Never while a new reaper runs.
    bool sendAgain();
    // What Steam's log said at the last stillComing(), for the journal.
    SteamLaunchLog::Step lastStep() const;

    // Tests set these short.
    void setStartupMilliseconds(int milliseconds);
    void setLimitMilliseconds(int milliseconds);
    void setWorkLimitMilliseconds(int milliseconds);

    // The app id in `steam -applaunch <appid>`, or empty for anything else.
    static QString appIdOf(const QStringList& exec);

private:
    // The process ids of Steam's reapers for the game: processes of this user
    // called exactly `reaper` with the arguments `SteamLaunch` and
    // `AppId=<appid>`, the same test cairn-give-up makes (#117).
    QStringList reapers() const;
    bool newReaper() const;

    QString m_processRoot;
    SteamLaunchLog m_log;
    SteamStartup m_startup;
    QString m_appId;
    QStringList m_reapersBefore;
    SteamLaunchLog::Step m_lastStep = SteamLaunchLog::Step::Nothing;
    SteamStartup::Now m_steamNow = SteamStartup::Now::Ready;
    bool m_sentAgain = false;
    QElapsedTimer m_sinceSteamStarted;
    QElapsedTimer m_sinceLaunch;
    // Since the tap, or since Steam's log last said it was working.
    QElapsedTimer m_sinceWork;
    // How long after login the Steam client counts as still starting. On the
    // first laptop (2026-10-08) it took the game up 20 s after it was started;
    // a slower machine, or Steam updating itself, takes longer.
    qint64 m_startupMilliseconds = 60000;
    // The longest a Steam game is waited for after the tap, or after Steam
    // last showed work, on the client starting or the reaper alone. A Proton
    // game took 84 s from the tap to its first window on the first laptop
    // (#132).
    qint64 m_limitMilliseconds = 120000;
    // The longest a Steam game is waited for while Steam works on it. Two
    // runtime updates and an install script took 50 s on the kids' laptop's
    // Wi-Fi (2026-10-20); a game update on a slow line takes minutes more.
    qint64 m_workLimitMilliseconds = 600000;
    // How long a client that started after the tap gets to take it. On the
    // kids' laptop a held tap was taken in the same second as the client's
    // own start command, every time (11 of 11, 2026-10-07 to 2026-10-20).
    static constexpr qint64 takeTapSeconds = 3;
};
