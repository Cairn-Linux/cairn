// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "SteamContentLog.h"
#include "SteamLaunchLog.h"
#include "SteamStartup.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QString>

#include <cstdint>

// Whether Steam has stopped part-way through getting a tapped game ready, and
// the one time in each tap that Steam may be started again for it (#206).
//
// A native Linux game runs in Steam Linux Runtime 1.0, scout, which itself
// runs inside 2.0, soldier, and each account fetches its own tools
// (ADR-0033). On the test VM (2026-10-30, 2 of 2) a new child's Steam
// fetched scout, took the game off its schedule and never asked for soldier.
// Its launch still said it was downloading, and nothing more happened. A
// Steam started again asked for soldier at once, and the game started a
// minute later. So the launcher starts Steam again once in each tap and
// sends the tap again (SteamRestart, ADR-0037).
class SteamStall {
public:
    enum class Now : std::uint8_t {
        // Nothing has stopped, or the tile is not a Steam game.
        Fine,
        // Stopped, for the first time in this tap: Steam may be started again.
        Stalled,
        // The launcher is mending the stall: Steam is being started again,
        // or was and has not taken the tap yet.
        Mending,
        // Starting Steam again could not be done or did not help.
        Stuck,
    };

    explicit SteamStall(QString contentLog = SteamContentLog::defaultPath());

    // A tile was pressed: a new tap, which may start Steam again once. The
    // app id is empty for anything but a Steam game.
    void startLaunch(const QString& appId);
    // Where the launch is at `now`, from Steam's launch log as just read,
    // where Steam's client is, and Steam's content log.
    Now read(const SteamLaunchLog& launch, SteamStartup::Now steam, const QDateTime& now);
    // True once in each tap, right after read() said Stalled: Steam is to be
    // started again.
    bool askRestart();
    // Starting Steam again is over: `started` when Steam runs again. Changes
    // nothing and is false unless this tap asked for it.
    bool restarted(bool started);
    // Logging the child out and back in would help: Steam stopped during
    // this tap, or still keeps the game stuck from an earlier one.
    bool logOutWouldHelp() const;
    // The journal's sentence for the game called `title` when logging out
    // would help, or empty.
    QString journalNote(const QString& title) const;

    // Tests set this short.
    void setRetakeLimitMilliseconds(int milliseconds);

private:
    enum class Restart : std::uint8_t { NotYet, Asked, Done, Failed };

    Now decide(const SteamLaunchLog& launch, SteamStartup::Now steam, const QDateTime& now);
    // Steam was started again and has not taken the tap yet.
    Now retaking(const SteamLaunchLog& launch, SteamStartup::Now steam, const QDateTime& now) const;

    SteamContentLog m_content;
    QString m_appId;
    // A game whose stall ended Stuck. Steam keeps its launch and takes a
    // later tap on it without a word, until the child logs out or Steam
    // works on it again.
    QString m_stuckAppId;
    Restart m_restart = Restart::NotYet;
    Now m_now = Now::Fine;
    bool m_stalledThisTap = false;
    // Since Steam was started again.
    QElapsedTimer m_sinceRestart;
    // How long a Steam started again gets to take the tap, from its own start
    // command, or from being started when it never gets that far. Its
    // starting or updating itself before that is bounded by the work limit
    // instead. On the test VM it took the tap 15 s after it was stopped; an
    // update of itself at a login took 57 s (ADR-0037).
    qint64 m_retakeLimitMilliseconds = 120000;
    // How long after Steam took the game off its schedule a launch still
    // downloading counts as stopped. When Steam goes on, it writes the
    // launch's next step in the same second (test VM, 2 of 2 working runs);
    // a minute leaves most of the work limit for starting Steam again.
    static constexpr qint64 stallSeconds = 60;
};
