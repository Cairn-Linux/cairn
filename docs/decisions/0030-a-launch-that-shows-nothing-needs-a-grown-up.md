# ADR-0030: A launch that shows nothing needs a grown-up

**Status:** accepted
**Date:** 2026-10-11
**Closes:** (none) — addresses issue #139; amends ADR-0026, ADR-0028 and
ADR-0029

## Context

When a launch put nothing on the screen before its wait ended (the 15 s
grace, longer for a Steam game Steam is working on, ADR-0029), the
launcher went back to the tiles and said nothing. The thinking was that
nothing had visibly gone wrong, so there was nothing to tell a child.

On 2026-10-10 two children used the kids' laptop while a power cut had taken
the house's internet down. Steam could not check each game's cloud saves,
so it opened its "Cloud sync failed" dialog and waited for an answer. The
kiosk hides Steam's own windows (ADR-0026, #99), so nobody saw the dialog
and no game started. Twelve of the evening's thirteen Steam presses went
that way. Each time the starting screen built the mark for 15 s, the tiles
came back, and the child pressed the same tile again: one pressed
Balloon-o-Rama three times in 46 s.

To a child a quiet return reads as "nothing happened, press again". DESIGN
§8.3 already asks the launcher to show "Something needs a grown-up" for a
failed or slow launch, rather than leave the child with whatever Steam
decided to display. The quiet return did not do that.

## Decision

**A launch that has put nothing on the screen when its wait ends is a
failed launch.** The grown-up screen comes up, as it does for a program
that cannot start: "Something needs a grown-up. Putt-Putt Enters the Race
did not start.", with Back. The same goes for a program the launcher
started that runs past the settle window (5 s) and exits without ever
putting a window up. This holds for every tile, not only Steam ones.

**The game still wins if it comes late.** Until the child presses Back,
the launch stays open: a window that comes now is the app's, takes over
from the grown-up screen, and is Running once it has stayed up for the
steady wait (ADR-0028). It is not a window that opened on its own. After
Back, the launch is over and ADR-0026's rules apply as before. A program
still running when the grown-up screen comes up runs on by itself, and its
exit later changes nothing.

**A window that came and went is not covered.** A launch whose window was on
the screen and then went before the app was open still returns to the
tiles when the wait ends; that is #133's question.

## Consequences

- A Steam game blocked behind a dialog the kiosk hides, a game Steam must
  download first, or a program that dies without a window now ends on the
  grown-up screen, naming the tile, instead of at the tiles.
- A slow app whose window comes after the wait, such as a Flatpak on a slow
  disk, shows the grown-up screen for a moment and then its window. That is
  better than before, when its window counted as one that opened on its
  own.
- The grown-up screen does not say why: the launcher cannot read Steam's
  dialog, and guessing would be worse. The Guardian finds the cause in
  Steam's own log (`~/.local/share/Steam/logs/console_log.txt`, "waiting
  for user response"); `docs/TRY.md` says so and names Steam Cloud as the
  usual reason a game will not start offline.
- A game blocked this way leaves Steam's request pending. Pressed again
  after Back, it waits behind the same dialog.
- The journal says "Nothing of <tile> came on the screen in time." for each
  one.
- Tested in C++: a launch with no window ends Failed at the grace, a
  program that exits after the settle window with no window ends Failed, a
  Steam game Steam never starts ends Failed with its tile's name, a window
  after that is the app's, Back ends the launch so a window after it opened
  on its own, and a program's exit after the grown-up screen changes
  nothing. In QML, the starting screen gives way to the grown-up screen,
  Back returns to the tile, and a late window takes over.
