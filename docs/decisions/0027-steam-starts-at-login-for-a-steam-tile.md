# ADR-0027: Steam starts at login for a child with a Steam tile

**Status:** accepted
**Date:** 2026-10-01
**Closes:** (none) — addresses issue #119

## Context

[DESIGN §8.3](../DESIGN.md#83-steam-and-proton-available-but-never-visible)
has the Steam client start with `-silent` at login, but nothing started it.
The first Steam tile a child pressed started the whole client first. In the
VM that took a game 14.9 s to start, against 4.5 s with the client already
running. Steam's own update and sign-in windows also came in the middle of
play instead of before it. And since ADR-0025 a client started by a tile
runs inside that tile's scope, so the grown-up's Ctrl-Alt-Home ended the
client along with the game.

The client is not free. On the VM's 4 GB, an idle child session used
250 MB with 3,168 MB available; with the client signed in and idle it used
1,694 MB with 2,119 MB available (`docs/research/launcher-footprint.md`).
That is about a gigabyte of a Minimum-tier machine (ADR-0003).

## Decision

With `--start-steam`, which the kiosk wrapper passes, the launcher starts
the Steam client at login when any of the child's tiles runs
`steam -applaunch`. It runs
`systemd-run --user --unit=cairn-steam --collect -- steam -silent`, carrying
the child's `DISPLAY` and `WAYLAND_DISPLAY`: a service of the child's own
systemd, outside any app's scope. A second start in the same session finds
the service running and does nothing.
A child with no Steam tile never starts it.
Log out already shuts the client down cleanly (ADR-0021).
For now the tiles are the switch; a per-child setting belongs in the
Guardian tool (P1-5) when it exists.

## Consequences

- Checked in the VM on 2026-10-01: with a Steam tile, the client's
  interface was up 9 s after Enter at the login screen, in
  `cairn-steam.service`, with nothing on the screen. The first game started
  in 4.5 s. Ctrl-Alt-Home ended the game and left the client running
  (#117). Log out shut the client down before the session ended. Without a
  Steam tile, no client started.
- A child with Steam tiles has about 2 GB available at idle on a 4 GB
  machine, against about 3 GB without.
- Steam's windows at login open before play; labwc hides them and the
  launcher ignores hidden windows (ADR-0026).
- The Flatpak Steam client is not covered: a tile running
  `flatpak run com.valvesoftware.Steam` starts no client at login. That is
  #109.
- The launcher now knows one launch form by name, `steam -applaunch`, as
  kidscan already does.
