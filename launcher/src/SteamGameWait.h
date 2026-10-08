// SPDX-License-Identifier: Apache-2.0
#pragma once

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
class SteamGameWait {
public:
    explicit SteamGameWait(QString processRoot = QStringLiteral("/proc"));

    // The kiosk started the Steam client at login (ADR-0027); its startup
    // time is counted from now.
    void steamStartedAtLogin();
    // A tile was pressed. Only `steam -applaunch <appid>` is a Steam game.
    void startLaunch(const QStringList& exec);
    // True while the game is worth waiting for: a Steam game, inside the
    // outer limit, and Steam still starting or its reaper for it running.
    bool stillComing() const;

    // Tests set these short.
    void setStartupMilliseconds(int milliseconds);
    void setLimitMilliseconds(int milliseconds);

    // The app id in `steam -applaunch <appid>`, or empty for anything else.
    static QString appIdOf(const QStringList& exec);

private:
    // Steam's reaper for the game, a process of this user called exactly
    // `reaper` with the arguments `SteamLaunch` and `AppId=<appid>`; the same
    // test cairn-give-up makes (#117).
    bool reaperRunning() const;

    QString m_processRoot;
    QString m_appId;
    QElapsedTimer m_sinceSteamStarted;
    QElapsedTimer m_sinceLaunch;
    // How long after login the Steam client counts as still starting. On the
    // first laptop (2026-10-08) it took the game up 20 s after it was started;
    // a slower machine, or Steam updating itself, takes longer.
    qint64 m_startupMilliseconds = 60000;
    // The longest a Steam game is waited for. A Proton game took 84 s from
    // the tap to its first window on the first laptop (#132).
    qint64 m_limitMilliseconds = 120000;
};
