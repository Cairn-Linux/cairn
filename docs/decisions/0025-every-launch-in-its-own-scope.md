# ADR-0025: Every launch runs in a systemd scope of its own

**Status:** accepted
**Date:** 2026-09-30
**Closes:** (none) — addresses issue #98; amends ADR-0018

## Context

ADR-0018 gave the grown-up Ctrl-Alt-Home, which runs `cairn-give-up` to end
whatever a child launched.
The helper finds the app by its sandbox root: a Flatpak app is a tree under
`bwrap`, a Steam game a tree under Steam's `reaper`.
ADR-0018 said a launch path that was neither would need a line there.
Native ScummVM, which kidscan's game tiles start since ADR-0024, is
neither: it is a plain child of the launcher, so a frozen game survived the
key (#98).
Adding `scummvm` to the pattern would answer this one program and the next
gap would be the same; the issue asked for a process identity or a systemd
scope instead.

## Decision

In the kiosk the launcher starts every program, from a tile or from the
Terminal, in a systemd user scope of its own: it runs
`systemd-run --user --scope --collect --unit=cairn-app-<launcher pid>-<n> --
<exec>`, so the scope holds the program and everything the program starts.
`systemd-run` becomes the program, so the launcher still watches the app's
own process as before.
The kiosk wrapper turns this on with `--scope-apps`; without it, as on a
developer's desktop or in the tests, the launcher starts programs directly.
`cairn-give-up` sends SIGCONT, SIGTERM and then SIGKILL to
`cairn-app-*.scope` in the child's own systemd, beside the sandbox roots it
already matched.
A Flatpak app moves itself into Flatpak's own scope and a Steam game is
started by the Steam client, so the sandbox-root match stays for those.

## Consequences

- Any native program a tile starts is covered without a new line in the
  helper, and so is whatever it started: in the VM, ScummVM had started
  speech-dispatcher for its text-to-speech, and both ended together.
- Checked in the VM on 2026-09-30: a SIGSTOP-frozen Putt-Putt in native
  ScummVM ended 0.7 s after Ctrl-Alt-Home, its scope was gone, and the
  launcher, labwc and PipeWire stayed up. A frozen Tux Paint, which Flatpak
  moves into its own scope, still ended through the `bwrap` match. A game
  opened from the Terminal got a scope of its own too, and the helper with
  nothing running exited 0.
- Programs a child starts now run under the user's systemd manager rather
  than in the login session's scope, as Flatpak apps already did.
  `loginctl terminate-user` at Log out (ADR-0021) still ends them.
- If a Steam tile starts the Steam client because nothing started it at
  login, the client runs in that tile's scope, and the give-up key ends the
  client along with the game. ADR-0018 kept the client running; P1-13
  starts it at login, outside any app scope, which restores that.
- What still needs the power button's five-second hold is what ADR-0018
  named: a frozen compositor or kernel. A program that asks systemd for a
  unit of its own, other than through Flatpak, would leave its scope and
  the key would miss it; nothing a tile starts today does.
- #99 can use the same scope to tell the app's windows from others.
