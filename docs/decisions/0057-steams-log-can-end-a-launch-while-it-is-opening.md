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

## Record

2026-11-07, cairn-min on `test.20261107-276-292-2c4643a` (main 57bd89a
with this change at ed16050 and #296 merged; 105 of 105 tests in the
image build). Freddi Fish 2, child ada, Ctrl+Q sent with `virsh
send-key`:
- Quit 1.5 s after the window, 23 s after login: tiles back 0.25 s after
  the window went, with "Steam says … ended while it was starting, 20 s
  after the tap".
- Quit again 39 to 45 s after login: tiles back 0.50 s after; at 76 s
  after login: 0.29 s after. Both with the journal line.
- After 30 s of play: the tiles came back with the window, and no "Steam
  says" line.
- Ctrl-Alt-Home during a game still ended it, tiles back in 0.07 s.
The check above has passed. Not tried: a Proton game, a game with a
splash, a game crashing in its first seconds, and the reaper guard (a
game given up on just before the tap). Sanitizer builds do not link on
the dev PC. Seen in the same run, and the same on main: after Steam last
exited uncleanly, its "Updating Steam" window can stay past the steady
wait and count as the game's window (#297).

2026-11-09, cairn-min again, on `test.20261109-295-296-300-4a5f38d`
(main 57bd89a with this change at b254364 and #300 at a782c4a, which
holds #296 at 73e519e; 106 of 106 tests in the image build). The same
game, child and keys, in two logins: the first with no language file,
the second with #300's language file for ada (en_GB):
- First login, quit 1.5 s after the window, 34 s after login: tiles
  back 0.32 s after the window went, with "Steam says … ended while it
  was starting, 31 s after the tap".
- First login, quit again 81 s after login: tiles back 0.54 s after,
  with the journal line.
- Second login, quit 1.5 s after the window, 23 s after login: tiles
  back 0.30 s after, with the journal line.
- After 30 s of play, once in each login: the tiles came back with the
  window, 0.08 s and 0.09 s after it went, and the launcher wrote no
  line.
- Ctrl-Alt-Home 5 s after the window, first login only: the window
  went 0.50 s after the keys and the tiles came back with it (0.09 s).
  No reaper and no ScummVM process was left, and the launcher, labwc
  and Steam were still running.

The check above passed again on this image, for one child and one game.
Steam wrote "removed" about 0.16 s and 0.27 s after the window went in
the first login's two quick quits, once the 0.06 to 0.07 s that the
VM's clock runs ahead of the dev PC's is taken off.
In the second login the game's processes had `LANG=en_GB.UTF-8`, and
`LC_ALL=C` as in the first; that shows the environment only, not a
game in another language.
No Steam update window is in any saved frame of the three quick quits.
The recorder keeps a frame only when one of three sampled points on the
screen changes, and in the first run it kept none from 14.4 s to 28.9 s
after the tap, so that is true of the saved frames only.
ada's login, the Log out button, the Guardian's login and Family still
worked.
The last three were checked less than fully: Steam had been shut down
by script before the Log out button was used, so it found no Steam
client to close; the Guardian's login was counted from a `plasmashell`
process and a screenshot that names no account; and Family was started
by command from its menu entry, not by a click.

Seen in the same run and not explained: in the first login the game's
window came 28.9 s after the tap, against 18.2 s in the second.
Steam's log gave its own start-up as 14.37 s the first time and 10.06 s
the second.
In the first login only, the launcher wrote "Steam is getting … ready
before it starts." and the starting screen added "Getting it ready.
This can take a few minutes." about 14 s after the tap (ADR-0036).
Why Steam was slower that time was not found.
The quick quit ended the same way both times.

Not checked yet:
- More than one child, one game and these six runs: three quick quits,
  two long plays and one Ctrl-Alt-Home.
- A Proton game, which is what the ADR waits on. Freddi Fish 2 runs
  through ScummVM.
- A game with a splash, a game crashing in its first seconds, and the
  reaper guard, as on 2026-11-07.
- The #297 case. Steam had ended cleanly before both logins, so its
  update window after an unclean exit did not come up.
- The later quit and Ctrl-Alt-Home in a login with a language file.
- The Log out button while Steam is running, which is how a child logs
  out.
- Whether anything is faster or slower than on 2026-11-07. That was
  another image, and its figures were not read again for this run.

The ADR stays proposed. The maintainer decided on 2026-11-09 to merge
the change and accept the ADR only after more Proton games have been
tried on a computer with #295: quick quits of several Windows games
through Proton, a game with a splash, and the kids' first games. Going
back to the tiles quietly when a game crashes in its first seconds
stays until feedback says otherwise (#133).
