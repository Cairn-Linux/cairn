# ADR-0028: Motion says something true; a starting tile builds the mark

**Status:** accepted
**Date:** 2026-10-07, amended 2026-10-08
**Closes:** (none) — addresses issue #129; amends ADR-0026
**Amended by:** ADR-0029 (a Steam game is waited for past the grace while Steam works on it)

## Context

On the first laptop with real Steam games, on 2026-10-07, a child pressed a
game tile and the screen did not change until the game's window appeared.
That took about 7 s with Steam already running, and up to 20 s with Steam
still starting after login. The tiles looked the same after a press as
before it, so the press looked broken, and a second press asked Steam for
the game again ("Game already running"). `AppLauncher` already knew the
launch was under way, in its Starting state, but nothing on the screen
showed it.

What the screen could show was held back by a rule. The brand guide's own
words are "nothing animated for its own sake"
([brand guide](../brand-guide/), and
[DESIGN §6.2](../DESIGN.md#62-visual-direction)'s "creative workspace, not
toy"). The note on the motion token in `brand/tokens.json`, added with the
scrolling grid on 2026-09-05, went further: "rows of tiles sliding into
view. Nothing else animates." It cited DESIGN §6.1, which does not say
that. The launcher's README meanwhile listed launch feedback as "the one
motion that earns its place". The maintainer found the token's rule too
strict: motion is not wrong, motion that pulls at a child's attention is.

## Decision

**Motion only says something true.** It shows a child that something is
moving or happening: tiles sliding into view, a tile starting. It is never
used to decorate, distract, reward, or hold a child's attention, it is slow
and small, and nothing flashes. Every duration is a token in
`brand/tokens.json`. This replaces the token's "nothing else animates"; the
brand guide's "nothing animated for its own sake" stands as it is.

**The first use is the starting screen.** From the moment a tile is pressed
until the app's window is on the screen, the launcher shows its
`StartingScreen` in front of the tiles, or of the Terminal when `open`
started it. On the Sand ground, the Cairn mark builds itself: the bottom
stone fades in, then each stone above it, one every `motionStone` (500 ms),
until the mark is whole. The whole mark stays for one step, fades for one,
and the building starts again from the bottom. Under it, one sentence names
the tile: "Putt-Putt Joins the Circus is starting." The stones are the
mark's own, from the `mark` tokens. The screen goes when the launcher leaves
Starting: the app is open, the launch failed (the grown-up screen
takes over), the grace timer ended with no window (the tiles come back), or
a grown-up gave up. Keys and taps while it shows do nothing; the launcher
already runs one program at a time, and the screen now makes that visible.

**The app is open when its window stays up.** The maintainer watched the
screen on the laptop on 2026-10-08: it went as soon as the game's first
window appeared, before the game was really there. A Steam or Proton game
often shows a window for a moment, a splash or a blank one, closes it and
opens its own. The tiles flashed between them, and under ADR-0026's rules
the game's own window, coming after the app's first had gone, could count
as one that opened on its own. The maintainer would rather the stones stay
a little long, even under the game's window, than see windows come and go.
So a launch is opening until one of the app's windows has stayed on the
screen for 3 s (`AppLauncher`'s steady wait); each window that comes up
starts the wait again. While it is opening, every
window on the screen is the app's, which amends ADR-0026's rule 2 ("the
first window on the screen is the app's"), and the app's windows leaving
the screen does not end the launch while the grace timer runs, unless the
program the launcher started has quit since its window came up: that is
the app ending, as when a child closes it at once. After the
app is open, ADR-0026's rules stand unchanged. The launcher cannot draw over
another program's window, so the stones show behind and between the game's
windows; the game covering them is fine.

## Consequences

- A press is answered at once, in the child's voice, with no new decision
  in C++: the screen follows `AppLauncher`'s state. Tested in the QML suite:
  the screen appears with the tile's name, builds a second stone, ignores a
  second press, and gives way to the tiles when the app's window has come
  and gone, or when no window came.
- `Tokens.qml` gains `motionStone` and the mark's stones in the units of its
  box (`markStones`, `markWidth`, `markHeight`, `markStoneRadius`), so a
  screen can draw the mark without a shape of its own. The CSS gains
  `--cairn-motion-stone`.
- Launches that come up fast, such as Tux Paint, show the screen for the
  steady wait, mostly behind the app's window.
- A window that opens during those first seconds is the app's even if
  something else opened it. Steam's own windows are hidden by the kiosk's
  rules and never count, and a launch rarely coincides with anything else.
  The window stays the app's after the app is open, as a dialog would.
- A Steam game that closes itself within 3 s of its window appearing
  (#133) leaves the stones up until the grace timer ends, then the tiles
  return; `steam -applaunch` has handed off, so the launcher cannot tell
  it from a splash. An app the launcher started itself, such as a Flatpak,
  ends with its window as before.
- A splash that stays up longer than 3 s still opens the app, and the
  game's own window after it counts as one that opened on its own. Whether
  any of the laptop's games do that is checked there before more is built
  for it.
- Tested in C++: a splash before the game is one start, with no state
  between Starting and Running; two windows at once are both the app's;
  each window that comes up, or is shown, gets the whole wait; an app that
  quits while opening ends when its window goes; a
  window that comes and goes alone returns to the tiles at the grace; one
  up when the grace ends gets the rest of its wait; one that goes after
  the grace ends the launch.
- The 15 s grace timer still ends a launch that has no window yet. A Steam
  game pressed right after login can take longer than that, and its
  window then counts as one that opened on its own. That is the timer's
  to fix, not the screen's: #132, answered for Steam games by ADR-0029.
- A child who needs less motion has no way to ask for it yet. A
  reduced-motion setting belongs with the Phase 3 accessibility work
  (ADR-0008); the screen still says what is happening without the stones.
- Brand guide v0.2's motion section (ROADMAP §6) starts from this rule.
