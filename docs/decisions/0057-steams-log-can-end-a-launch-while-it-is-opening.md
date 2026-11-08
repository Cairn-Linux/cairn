# ADR-0057: Steam's log can end a launch while it is opening

**Status:** proposed. It becomes accepted when the check under "Check in
the VM" has passed on cairn-min with this change installed.
**Date:** 2026-11-07
**Closes:** (none) — addresses issue #276; amends ADR-0028's consequence
about a Steam game that closes within 3 s of its window.

## Context

A child taps Freddi Fish 2, its window comes up, and the child quits it at
once with Ctrl+Q.
The game goes, but the starting screen comes back, with the stones
building and "Freddi Fish 2 … is starting.", and stays.
Nothing the child presses does anything until the tiles come back.

Timed on cairn-min on 2026-11-07
(`~/.local/share/cairn-dev/issue-276/notes.md` on the dev PC):
- 90 s after login, quit 2 s after the window came: the tiles came back
  4 s after the window went, when the 15 s launch grace ended.
- 11 s after login, quit at once: the tiles came back between 60 and
  84 s after the tap.
- 14 s after login, the game ended by itself, by Steam's log, just before
  the Ctrl+Q: the tiles came back 60 s after the tap, about 40 s after the
  game was gone (timed to the frame).
  Within a minute of login, each grace that ends is followed by another
  while Steam is starting (ADR-0029).
- After 25 s of play the tiles came back with the window, as they
  should.
- The journal said nothing about any of these launches ending.

The launcher waits because it cannot tell a game that has ended from a
splash.
A launch is opening until one of the app's windows has stayed up 3 s, and
while it is opening and the grace runs, a window going away is taken for a
splash (ADR-0028).
ADR-0028 ends the launch early only when the program the launcher started
quits after its window came up.
`steam -applaunch` never does: it hands the game to the Steam client and
exits before any window.

Steam itself knows when the game ends.
Its `console_log.txt`, which `SteamLaunchLog` already reads (ADR-0036),
has one line when the game's process starts and one when it ends:

    [2026-11-07 20:42:13] Game process added : AppID 294530 "…/reaper SteamLaunch AppId=294530 -- …", ProcID 4050
    [2026-11-07 20:42:42] Game process removed: AppID 294530 "…/reaper SteamLaunch AppId=294530 -- …", ProcID 4050

The process is Steam's `reaper`, which holds the whole game, so "removed"
comes once, when the game is over.
A Proton game on the kids' laptop wrote the same pair, with "Game process
updated" lines between them.
On the VM, Steam wrote "removed" 0.1 to 0.3 s after the window went.
In the 14 s run, where the game ended by itself, Steam wrote it 2 s
before the game's last window left the screen.

The `ProcID` at the end of the two lines does not pair them.
On the kids' laptop (2026-11-04 and 2026-11-06) and in older VM logs
(2026-10-30), "added" names one process and "removed" another, such as
3935 and then 4072: Steam's wrapper first, then the last process it
tracked, sometimes after a "Game process updated" line with a third id.

## Decision

**A Steam game whose window has gone while it is opening, and which Steam
says has ended since the tap, is over: the tiles come back at once.**

- `SteamLaunchLog` also reads the "Game process added" and "Game process
  removed" lines for the game the tile asked for, among the lines Steam
  wrote after the tap.
  The game has ended when the last of them is "removed".
  A process added after that is the game started again, so the game has
  not ended.
  A new tap, or a new log from a new Steam client, starts from nothing.
- The line cannot say which run of the game ended, and a game a grown-up
  gave up on a moment before the tap can end after the tapped one started
  (ADR-0055).
  So `SteamGameWait` counts Steam's "removed" only once no reaper for the
  game that started after the tap is still running, the same reapers it
  already watches (ADR-0029).
  A reaper that was there at the tap, such as the given-up game's, does
  not hold it back.
- While the launch is opening, the grace runs, and one of the app's
  windows has been on the screen but none is now, `AppLauncher` asks
  Steam's log.
  It asks at once, and again every 250 ms while that lasts, because
  Steam can write the line just after the window goes.
  That lasts as long as the window stays gone and the grace runs, which
  while Steam starts or gets the game ready can be the whole of ADR-0029's
  wait, up to its work limit of minutes.
- When the log says the game ended, the launch ends as it does for any
  other app that quit while opening, and the tiles come back.
  No new words for the child: the child chose to quit, or the game
  closed, and either way the tiles are the calm answer.
- The journal says "Steam says <tile> ended while it was starting, <n> s
  after the tap; back to the tiles."
  The grace's own "came and went" end now writes a line too: "<tile>
  came and went before it opened; back to the tiles."
- A window that goes while Steam says nothing about the game ending is a
  splash, as before, and the launch keeps waiting, past the grace too
  while Steam is starting (ADR-0029).
  Nothing about splashes changes.

## Consequences

- A child who quits a Steam game in its first seconds is back at the
  tiles within about a second of the window going, also within a minute
  of login.
  Before, they came back 15 s after the tap, or about 60 s after it just
  after login.
- A game that crashes, or closes itself, in its first seconds comes back
  to the tiles the same way.
  This is the signal #133 needs to bring up the grown-up screen for such
  a game later, but the launcher still cannot tell Ctrl+Q from a game
  closing itself: Steam's line is the same.
  #133 decides that, with a threshold that leaves room for a child who
  quits on purpose straight away.
- The journal now has the time a launch ended this way, so the next check
  can read it back.
- Steam's log is read every 250 ms while a Steam game's window is gone
  and its launch is opening.
  That is usually a second or two, but after a splash while Steam starts,
  or gets the game ready, it can run as long as ADR-0029's wait, minutes.
  Each read takes only what Steam added since the last one, and a look at
  the process list for the game's reapers; it was kept rather than cut
  short, so a game that ends later in that wait still brings the tiles
  back when it does.
- The launcher now relies on two more lines of Steam's log.
  If Steam stops writing them, the launcher falls back to the behaviour
  before this change, the grace and its extensions.
- A wrong "ended" costs more than a missing one.
  If Steam ever says the game ended while it is in fact still on its way,
  the tiles come back, and the game's window then opens over them as a
  window that opened on its own, which puts up the grown-up screen
  (ADR-0026).
  The reaper check rules this out for a given-up game ending late.
  It does not for a game that Steam runs as a short process first and
  then a second one, with a window from the first: not seen on the VM or
  the laptop, but the check in the VM and child test 2 watch for it.
- A game that hides its only window while opening, and goes on running,
  keeps waiting: Steam has not said it ended.
- Tested in C++ with fixture logs: Steam's end is seen; an end from
  before the tap, of another game, or of a game whose id starts the same
  does not count; a game started again after an end has not ended; a new
  tap or a new log forgets it.
  In `SteamGameWait`: the end is seen with the laptop's differing process
  ids; an end while a reaper started since the tap still runs is not, and
  is once that reaper goes; nothing ends for a tile that is not a Steam
  game.
  In `AppLauncher`: a Steam game quit during the steady wait returns to
  Idle as soon as Steam writes its line, without waiting out the grace;
  so does one that ended before its window went, within a minute of
  login; a splash while the game runs on keeps the starting screen, and
  `aSplashAcrossAGraceWhileSteamStartsIsPartOfTheStart` still passes.
  The log is read again only while the window is gone, not once it is
  back nor after the launch ends, and the grace's "came and went" line is
  checked.

## Check in the VM

On cairn-min with this change installed in a child's session, quit Freddi
Fish 2 with Ctrl+Q 1 to 2 s after its window comes, once within a minute
of login and once later: the tiles come back within about a second of the
window going, and the journal has the "Steam says … ended" line.
After longer play, the tiles come back with the window as before.
