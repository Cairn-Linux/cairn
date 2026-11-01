// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "SteamContentLog.h"
#include "SteamLaunchLog.h"
#include "SteamQuestions.h"
#include "SteamReapers.h"
#include "SteamSignIn.h"
#include "SteamStall.h"
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
//
// A Steam that cannot sign in never takes the tap up (#182). While Steam's
// log says nothing about the game, a refused sign-in ends the wait when the
// grace it is seen in ends, and a Steam that cannot reach its servers gets a
// short wait to come back first. Neither counts as work, so neither shows
// "Getting it ready".
//
// A new child's first Linux game can stop part-way (#206): Steam takes the
// game off its download schedule while its launch still says it is
// downloading, and nothing more happens. SteamStall sees it a minute later;
// Steam is then started again, once in each tap, and the tap is sent again
// once the new client has started, all as work, under a work limit of the
// restarted client's own. If that does not bring the game, the wait ends
// soon, and logging the child out is what will help. A stall seen while
// Steam updates itself, or runs a new reaper for the game, is waited for
// as work, and Steam is started again at a later look once it can be. One
// seen while Steam's sign-in is turned down, or while it cannot reach its
// servers, ends the wait at that look by #182's rules, with the sign-in or
// the connection as the reason.
//
// A question on a hidden window holds its game across taps, also after
// Steam signs in again (#211). A later tap on that game that Steam takes
// without a step is the same question, so it ends as the first one did,
// with the same reason (SteamQuestions).
class SteamGameWait {
public:
    explicit SteamGameWait(QString processRoot = QStringLiteral("/proc"),
                           QString steamLog = SteamLaunchLog::defaultPath(),
                           QString steamStartupLog = SteamStartup::defaultPath(),
                           QString steamContentLog = SteamContentLog::defaultPath());

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
    // or Steam is being started again for it, inside the work limit, or,
    // within the limit since Steam last showed work, Steam is still starting
    // or a new reaper for it is running. Asked when a grace ends, so the wait
    // can run one grace past a limit. While Steam's log says nothing about
    // the game, or Steam stopped getting it ready, false once Steam's sign-in
    // was refused, and false after the offline limit while it cannot reach
    // its servers. False once starting Steam again could not be done or did
    // not help, unless a new reaper runs.
    bool stillComing();
    // At the last stillComing(), Steam's logs said it was working on the
    // game, or starting or updating itself, or a client started since the
    // tap had not taken it yet, or Steam was being started again for it.
    bool steamWorking() const;
    // True once per tap, at the stillComing() after a Steam client that
    // started since the tap, or since Steam was started again, has had a
    // moment to take it and has not: the tap was lost and should be sent
    // again. Never while a new reaper runs, while Steam cannot sign in, nor
    // once starting Steam again did not help.
    bool sendAgain();
    // True once per tap, at the stillComing() that found Steam had stopped
    // getting the game ready, while Steam has started, is signed in and runs
    // no new reaper for it: Steam is to be started again (#206).
    bool restartSteam();
    // Starting Steam again is over; `started` when it runs again. From then
    // on only what Steam writes counts, as after a lost tap, and the new
    // client gets a work limit and an offline wait of its own. Whichever tap
    // it was for, the questions the old client asked went with it.
    void steamRestarted(bool started);
    // Logging the child out and back in would help, and Steam's logs gave no
    // other reason at the last stillComing(): the journal's note is the
    // stall's.
    bool logOutWouldHelp() const;
    // What Steam's log said at the last stillComing(), for the journal. A tap
    // Steam took without a step, on a game it still holds behind a question
    // from an earlier tap, is that question.
    SteamLaunchLog::Step lastStep() const;
    // Whether Steam could sign in, at the last stillComing(), for the
    // journal. Fine whenever Steam's log says something about the game,
    // unless Steam stopped getting it ready.
    SteamSignIn::Now signIn() const;
    // The journal's sentence for a Steam game called `title` that put
    // nothing on the screen, from what Steam said at the last stillComing(),
    // or empty when Steam said nothing that explains it.
    QString journalNote(const QString& title) const;

    // Tests set these short.
    void setStartupMilliseconds(int milliseconds);
    void setLimitMilliseconds(int milliseconds);
    void setWorkLimitMilliseconds(int milliseconds);
    void setOfflineLimitMilliseconds(int milliseconds);
    void setRetakeLimitMilliseconds(int milliseconds);

private:
    // A reaper for the game is running that was not there at the tap.
    bool newReaper() const;
    // Steam's logs said why the game did not start, at the last
    // stillComing(): a sign-in, a question or a refusal.
    bool steamGaveAReason() const;

    SteamReapers m_reapers;
    SteamLaunchLog m_log;
    SteamStartup m_startup;
    SteamSignIn m_signIn;
    SteamStall m_stall;
    SteamQuestions m_questions;
    QString m_appId;
    QStringList m_reapersBefore;
    SteamLaunchLog::Step m_lastStep = SteamLaunchLog::Step::Nothing;
    SteamStartup::Now m_steamNow = SteamStartup::Now::Ready;
    SteamSignIn::Now m_signInNow = SteamSignIn::Now::Fine;
    SteamStall::Now m_stallNow = SteamStall::Now::Fine;
    bool m_sentAgain = false;
    QElapsedTimer m_sinceSteamStarted;
    // Since the tap, or since Steam was started again for it: the work limit
    // counts from here.
    QElapsedTimer m_sinceLaunch;
    // Since the tap, or since Steam was started again for it: a client that
    // cannot reach its servers gets the offline limit from here.
    QElapsedTimer m_sinceTapSent;
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
    // The longest a Steam game is waited for while Steam works on it, from
    // the tap, or from Steam started again for it. Two runtime updates and an
    // install script took 50 s on the kids' laptop's Wi-Fi (2026-10-20); a
    // game update on a slow line takes minutes more.
    qint64 m_workLimitMilliseconds = 600000;
    // The longest a Steam game is waited for after the tap, or after Steam
    // was started again, while Steam cannot reach its servers and has not
    // taken the game up. A chosen short wait, not a count of Steam's own
    // retries: its connection log (2026-10-08 to 2026-10-28) shows its first
    // five tries 8 to 90 s apart, then gaps of 2 to 14 minutes. A child
    // should not watch a frozen screen that long; the Wi-Fi coming back is a
    // grown-up's job.
    qint64 m_offlineLimitMilliseconds = 60000;
    // How long a client that started after the tap gets to take it. On the
    // kids' laptop a held tap was taken in the same second as the client's
    // own start command, every time (11 of 11, 2026-10-07 to 2026-10-20).
    static constexpr qint64 takeTapSeconds = 3;
};
