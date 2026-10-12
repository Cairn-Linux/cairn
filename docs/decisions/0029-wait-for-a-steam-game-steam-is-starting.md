# ADR-0029: Wait for a Steam game while Steam is working on it

**Status:** accepted
**Date:** 2026-10-08
**Closes:** (none) — addresses issue #132; amends ADR-0026 and ADR-0028

## Context

The launcher waits 15 s for a launched app's first window (its grace
timer). Then it goes back to the tiles, and a window that comes later counts
as one that opened on its own (ADR-0026). That fits every native game on the
first laptop, about 4 to 7 s from the tap with Steam up. It does not fit two
cases seen there:

- **Steam still starting.** On 2026-10-08 a child pressed Putt-Putt Goes to
  the Moon 3 s after login. The Steam client, started at login (ADR-0027),
  took the game up 16.8 s after the tap, and its window came at 21.5 s. The
  starting screen (ADR-0028) went at 15 s, the tiles came back, and the
  game's window was named as one that opened on its own.
- **A Proton game.** Pep's Birthday Surprise took 49, 53 and 84 s from the
  tap to its first window on 2026-10-07, with Steam already up.

A longer fixed wait would also cover both, but every launch that really
failed would then keep a child watching the stones for a minute or more.
`steam -applaunch` exits at once, so its process says nothing. Two other
facts do say Steam is still working on the game:

1. The kiosk started the Steam client at login a short while ago.
2. Steam's `reaper` for that game is running: a process of the child's
   called exactly `reaper` with the arguments `SteamLaunch` and
   `AppId=<appid>`. Steam starts it when it takes the game up, and it lives
   as long as the game. `cairn-give-up` already recognises it (#117).

## Decision

**A Steam tile's launch is waited for while Steam is working on it.** When
the grace ends with no window of the app on the screen, and the tile runs
`steam -applaunch <appid>`, the launcher waits one more grace if either:

- the launcher started the Steam client at login (`--start-steam`) less
  than 60 s ago, or
- Steam's reaper for that app id is running.

It then looks again, with or without a window of the app up, so a splash
that closes after a grace has ended is still part of the start. It stops
looking once 120 s have passed since the tap, so the wait can run one grace
past that. A reaper that was already running when the tile was pressed is
the same game left from an earlier start and does not count. The exit of
`steam -applaunch` never ends a Steam game's launch, however late it
comes: right after login it can wait for the client before handing over. When
neither fact holds, or the limit is reached, the launch ends quietly in the
tiles as before. `SteamGameWait` makes the decision. It reads `/proc` the
way `cairn-give-up` does, and only when a grace ends. Any other tile still
gets the usual 15 s.

## Consequences

- Pressed right after login, a Steam game keeps the starting screen up
  until its window comes, and that window is the app's. With Steam up and a
  game that has really failed, the launch ends at the first grace once
  Steam's reaper has gone.
- A game whose reaper has gone ends the wait at the next grace, up to 15 s
  later. A Steam game that closes itself at once (#133) is not shortened by
  this.
- With Steam up, a game Steam must first download or update (a game
  update, the Steam Linux Runtime, a first Proton setup) has no reaper
  until that is done. Its launch still ends at 15 s and its window counts
  as one that opened on its own. A download the child cannot see is a
  P1-13 question, not this one's.
- Steam's age is checked when a grace ends, not at the tap: a game pressed
  50 s after login is checked at 65 s, by its reaper alone.
- 60 s and 120 s come from one laptop. A slower machine, or Steam updating
  itself at login, may need more; both are constants in `SteamGameWait`.
- A game outside Steam that takes longer than 15 s, such as a Flatpak app
  on a slow disk, still loses its starting screen at 15 s.
- Tested in C++ with a stand-in `/proc` folder: the app id from the tile,
  the reaper matched by name and both arguments only, Steam starting at
  login, the outer limit, and a non-Steam tile left alone. An `AppLauncher`
  test runs a stand-in `steam` with Steam started at login and keeps the
  launch Starting past the grace, and another keeps a splash that closes
  just after a grace ends part of the start. A reaper from before the press
  does not count.
