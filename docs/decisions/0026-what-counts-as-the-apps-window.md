# ADR-0026: What counts as the app's window

**Status:** accepted
**Date:** 2026-10-01
**Closes:** (none) — addresses issue #99; amends ADR-0017 and ADR-0018

## Context

The launcher tracks an app by its windows, because `steam -applaunch`
returns long before the game is over (#42).
It took every window that opened during a launch for the app's, and showed
"Something needs a grown-up" for any window that opened at the tiles.
Both went wrong in the VM on 2026-09-30, with Steam's own windows, which
labwc hides the moment they open (ADR-0017):

- **The tiles stopped working.** Steam opened its main window while a game
  ran. The launcher took it for part of the game, so once the child left
  the game it kept waiting for that hidden window, and two tiles each did
  nothing for 20 s. Only Log out still worked.
- **The grown-up screen could not be left.** The same window opening at the
  tiles brought up the grown-up screen. It has no buttons, Escape and
  Super+Q did nothing, and the hidden window never closes.

The window list the launcher used, `ext-foreign-toplevel-list-v1`, gives a
window's app id and title, both chosen by the program, and nothing else.
No protocol labwc offers says which process drew a window.
labwc 0.9.6 also offers `wlr-foreign-toplevel-management-unstable-v1`,
whose version 3 adds two facts from the compositor itself: whether a
window is hidden, and which window it belongs to.
And `flatpak run` does not exit early as ADR-0018 says: it becomes the
app's sandbox, `bwrap`, and lives as long as the app, so the launcher's own
child ending means the app has ended, for a Flatpak app as for ScummVM.
Only `steam -applaunch` still hands off.

## Decision

The launcher reads windows through
`wlr-foreign-toplevel-management-unstable-v1`, version 3, built from
Fedora's `wlr-protocols-devel`.
`LaunchWindows` decides whose each window is, from what the compositor
says:

1. **A hidden window never counts.** It neither keeps the tiles waiting nor
   brings up the grown-up screen. It is decided when it is first shown.
2. **During a launch, the first window on the screen is the app's,** and so
   is any window that belongs to one of the app's, such as a dialog.
3. **Any other window on the screen opened on its own** and gets the
   grown-up screen until it closes.

A window's app id and title only name it, except that the launcher's own
app id still marks its own windows.
The launch is over when the app's last window leaves the screen, or when
the program the launcher started has run past the settle window and exits;
then the app's windows have two seconds to close, and any still up count as
opened on their own.
Every launch has its own process, so a program left from an earlier one
never stops the next.
The grown-up's Ctrl-Alt-Home always brings the tiles back: after
`cairn-give-up` has ended the child's programs, it writes
`$XDG_RUNTIME_DIR/cairn/give-up`, and the launcher forgets the launch and
every window open then.
A way out that asks for a PIN or a password waits for the Guardian's way
in (#88, P1-7).

## Consequences

- Checked in the VM on 2026-10-01: with Steam's main window hidden, a game
  left with Super+Q started again with one Enter. A new hidden Steam window
  at the tiles ("Friends List") brought up no grown-up screen, and the
  tiles still worked. Ctrl-Alt-Home wrote the file and the next launch
  worked.
- `wlr-protocols-devel` is a build dependency in place of
  `wayland-protocols-devel`; nothing else uses either.
- A game that hides its own window and runs on leaves the tiles usable, and
  a second tap starts a second copy; the give-up key ends both.
- A visible window that opened on its own sits in front of the grown-up
  screen. Super+Q asks it to close, and Ctrl-Alt-Home brings the tiles back
  if it will not.
- A program could still copy the launcher's app id to go unnoticed. That
  needs a deliberate program, not an accident, and the Phase 1 image only
  runs software a Guardian chose.
- The same VM run found that give-up's `bwrap` match now ends Steam's own
  interface, which runs in Steam's `srt-bwrap` sandbox; that is #117.
