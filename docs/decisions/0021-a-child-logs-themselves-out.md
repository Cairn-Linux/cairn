# ADR-0021: A child logs themselves out

**Status:** accepted
**Date:** 2026-09-30
**Closes:** (none) — addresses issue #44

## Context

[DESIGN §3.2](../DESIGN.md#32-the-guardian-role) listed "exit to the login
screen" and "switch user" among the quick actions behind the Guardian's PIN
overlay (P1-7). An L1 or L2 child had no way to end their own session:
Super+Q at the tiles does nothing, so that it never ends the session by
accident (ADR-0019), and the session ends only when the launcher does.

On a family computer the end of a turn is routine, not administration.
Needing a parent's PIN for it breaks the goal that a child uses the computer
unsupervised (DESIGN §1.2), and teaches children to hand the machine over
still logged in. ADR-0020 made this more pressing: the login screen now
shows the whole family, so the natural end of a turn is back at "Who's
playing today?" for the next person.

Ending the session has to end everything the child started. Quitting the
launcher ends labwc and the session's own processes, but a Flatpak app runs
in its own scope and the Steam client under the user's service manager, and
both would stay up behind the login screen. logind ends them all when asked
to end the user, and lets any user end their own sessions without a
password. Checked in the VM on 2026-09-30 from inside `ada`'s session: no
polkit prompt, and no process of hers left.

## Decision

The launcher has a Log out button above the tiles, always in view, never
scrolled away. Up from the top row of tiles reaches it. It asks once, "Log
out now?", with Back and Log out, and the focus starts on Back, so a stray
double press changes nothing; Escape and Super+Q answer as Back does. Log
out runs `cairn-log-out`, which gives a running Steam client a moment to
shut down and save, then ends the child's own account with `loginctl
terminate-user`. The login screen comes back.

The launcher shows Log out only when it is given the program with
`--log-out`, which the kiosk's wrapper does. A launcher run without it, as
a grown-up's run under Plasma, has no Log out.

"Exit to the login screen" and "switch user" leave the quick-actions list:
switching user is logging out and choosing another name.

## Consequences

- A child finishes their turn alone, and nothing of theirs keeps running
  for the next person: no app, no Steam client.
- "Log out" is the real word, the one every system uses (non-negotiable 4).
  A child who cannot read yet needs to be shown the button once; the
  first-run guide (#78) can do that.
- While the grown-up screen is up for a window that opened on its own, the
  tiles and Log out are hidden, so a child cannot log out past it. That
  window is a grown-up's to deal with (#88).
- Log out is reached only from the tiles, so an app the child was using has
  already been left with Super+Q, and asked about unsaved work if it asks
  at all (ADR-0019).
