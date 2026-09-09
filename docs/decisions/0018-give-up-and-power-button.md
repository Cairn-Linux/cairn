# ADR-0018: A grown-up's way out of a stuck program, and the power button

**Status:** accepted
**Date:** 2026-09-08
**Closes:** (none) — addresses issue #41

## Context

At L1 and L2 a child cannot leave a window on their own: there is no
Alt-Tab, no titlebar, no taskbar (ADR-0017). That is the point. But a game
can freeze, or have no quit a pre-reader can find (ScummVM is Ctrl-Q, Tux
Paint is a button and a dialog), and then the child is trapped with nothing
to do but call a grown-up or hold the power button until the machine cuts
out mid-write. Issue #41.

The P0-10 run confirmed the trap: with no bindings, a frozen Tux Paint sat
on screen and only SIGKILL ended it. Two facts shaped the fix. First, every
program a tile or the terminal starts is **sandboxed** and reparents away
from the launcher: a Flatpak app becomes a tree under `bwrap` in its own
session, a Steam game a tree under Steam's `reaper`. So the launcher's own
child process (`flatpak run`, `steam -applaunch`) has already exited by the
time the app's window is up; the launcher cannot end the app by killing what
it started, and in fact is already showing its tiles behind the fullscreen
window. The compositor knows the window, but `ext-foreign-toplevel-list-v1`
does not carry a pid, and labwc's own `Kill` sends only SIGTERM, which a
stopped or wedged process never acts on. Second, a stopped process must be
sent SIGCONT before any other signal can reach it.

## Decision

Two mechanisms, both in the session layer, neither in the launcher.

**The escape is a grown-up's key combination, Ctrl-Alt-Home**, bound in
`session/labwc/rc.xml` to run `session/bin/cairn-give-up`. It is deliberately
not a single key or a gesture a five-year-old finds by accident: the child
who is stuck fetches an adult, and the adult presses the keys. The helper
ends everything the child launched and nothing of the frame, by signalling
the sandbox roots in the child's own session: SIGCONT first, so a frozen
process can receive what follows, then SIGTERM as a courtesy, then SIGKILL.
It matches `bwrap` (every Flatpak app) and `reaper SteamLaunch` (every Steam
game), which the launcher, the compositor, the terminal, the session's
services and the resident Steam client never are, so the client stays signed
in. The launcher needs no change: when the window dies, its tiles, already
behind it, are simply uncovered.

**The power button is a tap to ignore and a hold to power off**, set in
`session/logind.conf.d/10-cairn-power.conf` with `HandlePowerKey=ignore` and
`HandlePowerKeyLongPress=poweroff`. A stray tap does nothing at any level;
holding it five seconds powers the machine off in order through logind, so
nothing is left half-written. This is the maintainer's choice over issue
#41's suggestion of mapping a tap to "close the app": the phone-like tap-to-
sleep, hold-to-off is more familiar and keeps the panic action on the
keyboard where an adult uses it. Plasma at L3, L4 and for Guardians takes its
own logind inhibitor and is unaffected.

## Consequences

- The mechanism is a shell helper and two configuration keys, not launcher
  code. It is tested at the config level (`session/tests/`) and end to end in
  the VM (`docs/research/kiosk-containment.md`): live Tux Paint and GCompris,
  a SIGSTOP-frozen Tux Paint, and a Steam game all end within about two
  seconds and return the tiles, the frame and the Steam client survive, and
  the helper is harmless when nothing is running.
- `cairn-give-up` knows the shape of what a child can launch (Flatpak and
  Steam). A future launch path that is neither would need a line here. That
  coupling is deliberate: matching the sandbox roots is what lets it never
  touch the frame, where matching windows or "foreground" processes could.
- It does not dismiss a Steam **client** window that forces itself open
  (the sign-in or update window): that is `steamwebhelper`, not a game, and
  ending it is a Guardian action (`steam -shutdown`), not a child's panic
  button. The launcher's grown-up screen already covers that case (§8.3).
- A hard-locked compositor or a kernel hang is still only answered by the
  power button's five-second hold. That is the layer below this one and the
  reason the hold exists.
