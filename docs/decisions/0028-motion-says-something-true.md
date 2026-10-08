# ADR-0028: Motion says something true; a starting tile builds the mark

**Status:** accepted
**Date:** 2026-10-07
**Closes:** (none) — addresses issue #129

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
Starting: the app's window is up, the launch failed (the grown-up screen
takes over), the grace timer ended with no window (the tiles come back), or
a grown-up gave up. Keys and taps while it shows do nothing; the launcher
already runs one program at a time, and the screen now makes that visible.

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
- Launches that come up fast, such as Tux Paint, show the screen only
  for a moment.
- The 15 s grace timer still ends a launch that has no window yet. A Steam
  game pressed right after login can take longer than that, and its
  window then counts as one that opened on its own. That is the timer's
  to fix, not the screen's: #132.
- A child who needs less motion has no way to ask for it yet. A
  reduced-motion setting belongs with the Phase 3 accessibility work
  (ADR-0008); the screen still says what is happening without the stones.
- Brand guide v0.2's motion section (ROADMAP §6) starts from this rule.
