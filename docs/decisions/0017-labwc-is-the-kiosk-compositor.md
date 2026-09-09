# ADR-0017: labwc is the kiosk compositor for L1 and L2

**Status:** accepted
**Date:** 2026-09-08
**Closes:** ROADMAP D3

## Context

[DESIGN §4.5](../DESIGN.md#45-sessions-per-level) gives L1 and L2 a kiosk
compositor running the launcher fullscreen and nothing else. ADR-0004 made
Steam titles at those levels a v1 requirement, and
[DESIGN §8.3](../DESIGN.md#83-steam-and-proton-available-but-never-visible)
says the client's own windows are never what a child touches. A kiosk has
no tray to hide them in, so the compositor has to do two things a
single-app compositor does not: keep windows it did not ask for off the
screen by rule, and tell the launcher that one appeared so it can show
"Something needs a grown-up".

Two candidates were weighed from 2026-09-03: **cage**, which runs one
application and has neither window rules nor a foreign-toplevel protocol,
and **labwc**, a wlroots stacking compositor with an Openbox-style
configuration that has both. labwc was the recommendation from ADR-0004 on
and every document treated it as settled; it stayed proposed until the
Steam spike proved both halves against a real client.

The evidence is `docs/research/kiosk-containment.md` (P0-4) and
`docs/research/steam-containment.md` (P0-10), run in the Bazzite VM:

- With no default bindings, no server decorations and focus requests
  refused, a child cannot leave the launcher by keyboard, VT switch or a
  focus-stealing client (P0-4, 2026-09-06 nested and 2026-09-08 at the
  console).
- One window rule matching the WM_CLASS instance `steamwebhelper` puts the
  sign-in window and, signed in, the forced main window into the hidden
  state from their first frame, and the launcher hears about them through
  `ext-foreign-toplevel-list-v1` and shows the grown-up screen naming them
  (P0-10, signed out on 2026-09-08 and signed in the same evening).
- A signed-in `-silent` client in the kiosk maps no window at all, so the
  child sees the tiles at login with nothing wrong.
- A tile running `steam -applaunch` brings the game up over the launcher
  and the tiles are back within two seconds of the game quitting; the
  client survives a reboot signed in.

## Decision

labwc in kiosk configuration is the L1 and L2 compositor. Its
configuration lives in `session/labwc/` and is owned by Cairn: no default
key or mouse bindings, no server decorations, focus requests refused, the
launcher fullscreen by rule, Steam's windows iconified by rule but never
dropped from the toplevel list, and the VT-switch keysyms removed from the
keymap. The session dispatcher (ADR-0012) starts it with the launcher as
its only client. cage is not used; it stays a name in the research notes
as the measurement baseline it was.

## Consequences

- The kiosk is a configuration of an upstream compositor, not a fork or a
  program of ours (CLAUDE.md, "inherit from upstream"). labwc's version
  matters: two of the properties the containment rests on
  (`ignoreFocusRequest`, and the way an empty keybind list is handled) are
  named in `kiosk-containment.md` with the log line to check on every
  upgrade.
- The launcher's foreign-toplevel listener is a hard dependency on the
  compositor offering `ext-foreign-toplevel-list-v1`; KWin does not, so the
  Wayland path is checked under labwc, never in CTest.
- Steam's own toasts and any Wayland-native dialog from a wrapper are not
  reachable by a WM_CLASS rule; P1-13 starts `/usr/bin/steam -silent`
  itself and the grown-up screen remains the universal fallback.
- A signed-in client idles at about 1.4 GB proportional in the VM, so the
  session with the client resident is about 1.7 GB of the 4 GB floor
  before a game runs. That is a memory budget for P1-13, not a compositor
  question.
