// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QByteArray>
#include <QString>

#include <cstdint>

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
    // Reads what the log has gained since last time and says where the
    // launch has got to. The last line about the game decides.
    Step read();

    // What one line of the log says about the game, or Nothing when the
    // line is not about it.
    static Step stepOf(const QByteArray& line, const QString& appId);
    // `console_log.txt` in the child's own Steam folder.
    static QString defaultPath();

private:
    QString m_path;
    QString m_appId;
    qint64 m_offset = 0;
    Step m_step = Step::Nothing;
};
