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
- 11 and 14 s after login, quit at once: the tiles came back about 40 s
  after the game was gone, 60 s after the tap (timed to the frame once;
  the other time they were back between 60 and 84 s).
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
Once, when the game ended by itself before the Ctrl+Q, Steam wrote it 2 s
before the game's last window left the screen.

## Decision

**A Steam game whose window has gone while it is opening, and which Steam
says has ended since the tap, is over: the tiles come back at once.**

- `SteamLaunchLog` also reads the "Game process added" and "Game process
  removed" lines for the game the tile asked for, among the lines Steam
  wrote after the tap.
  The game has ended when the last of them is "removed".
  A process added after that is the game started again, as when a child
  taps a game a grown-up gave up on a moment before (ADR-0055), so the
  game has not ended.
  A new tap, or a new log from a new Steam client, starts from nothing.
- While the launch is opening, the grace runs, and one of the app's
  windows has been on the screen but none is now, `AppLauncher` asks
  Steam's log.
  It asks at once, and again every 250 ms while that lasts, because
  Steam can write the line just after the window goes.
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
- Steam's log is read every 250 ms only while a Steam game's window is
  gone and its launch is opening; reading it takes only what Steam added
  since the last read.
- The launcher now relies on two more lines of Steam's log.
  If Steam changes their words, the launcher falls back to today's
  behaviour, the grace and its extensions, and nothing worse.
- A game that hides its only window while opening, and goes on running,
  keeps waiting: Steam has not said it ended.
- Tested in C++ with fixture logs: Steam's end is seen; an end from
  before the tap, of another game, or of a game whose id starts the same
  does not count; a game started again after an end has not ended; a new
  tap or a new log forgets it.
  In `AppLauncher`: a Steam game quit during the steady wait returns to
  Idle as soon as Steam writes its line, without waiting out the grace;
  so does one that ended before its window went, within a minute of
  login; a splash while the game runs on keeps the starting screen, and
  `aSplashAcrossAGraceWhileSteamStartsIsPartOfTheStart` still passes.

## Check in the VM

On cairn-min with this change installed in a child's session, quit Freddi
Fish 2 with Ctrl+Q 1 to 2 s after its window comes, once within a minute
of login and once later: the tiles come back within about a second of the
window going, and the journal has the "Steam says … ended" line.
After longer play, the tiles come back with the window as before.
