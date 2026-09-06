# ADR-0016: SDDM is the display manager; the login screen is an SDDM theme

**Status:** accepted
**Date:** 2026-09-06
**Closes:** ROADMAP D6

## Context

[DESIGN §4.3](../DESIGN.md#43-sessions-and-accounts) asks the login screen
for three things: large avatar tiles, no password for L1 and L2 accounts,
and Guardians not shown beside the children. ADR-0012 adds a fourth: the
greeter offers only the one Cairn session.

Stock Bazzite KDE, recorded by `provision/cairn-provision.sh` in the VM on
2026-09-06 (`bazzite-44.20260902`), ships **Plasma Login Manager**
(`plasmalogin.service`), as does Fedora 44 KDE. It is KDE's fork of SDDM with
a compiled greeter. Its whole configuration schema (`mainconfig.kcfg`) is
the X11 and Wayland server paths, log files, autologin, namespaces and the
default `PATH`. There is no way to hide a user, no way to limit which
sessions are offered, and no theme: the greeter's QML is built into the
binary. Its README says the project is not yet recommended outside a virtual
machine. None of the four requirements can be met on it without patching
the greeter, which would be a fork.

Three options were weighed:

- **A. Keep Plasma Login Manager.** Nothing to swap, but §4.3 cannot be
  met, and the project's own README says it is not ready.
- **B. SDDM with a Cairn theme.** SDDM 0.21 is in Fedora 44 with
  `sddm-breeze` and `sddm-wayland-plasma`, which runs the greeter under
  `kwin_wayland` exactly as Plasma Login Manager does. Its configuration has
  `HideUsers`, `SessionDir` and `Theme`, and its themes are QML, so the
  login screen becomes a theme, not a program (ADR-0002). Bazzite itself
  swaps to SDDM in the Steam Deck stage of its Containerfile, so the swap
  is a move upstream already makes.
- **C. greetd with a first-party QML greeter.** Meets every requirement
  but is a whole program to write and keep, with no upstream theme
  ecosystem behind it.

## Decision

SDDM is the display manager for Cairn. Provisioning now, and the image in
Phase 1, install `sddm`, `sddm-breeze` and `sddm-wayland-plasma`, disable
`plasmalogin.service` and enable `sddm.service`; the swap is explicit and
recorded, never a side effect. Configuration lives in `session/sddm/`:
`SessionDir` names only Cairn's session directory, so `cairn.desktop` is the
one session offered; `HideUsers` lists the members of `cairn-guardian`,
written by provisioning now and by the Guardian tool later; and
`/etc/pam.d/sddm` gains `pam_succeed_if` lines that let `cairn-l1` and
`cairn-l2` members in without a password. That rule exists only in the
greeter's PAM service, never in `system-auth` or `password-auth`, so `su`,
`ssh` and the lock screen still see a locked password (issue #35). Phase 0
uses the Breeze theme, whose "different user" control is how a Guardian
reaches the password prompt; the Cairn theme is a Phase 1 task and gives
that control a plain "Grown-up" label.

## Consequences

- The login screen is QML in a theme directory, the same language and
  tokens as every other first-party surface.
- Until Phase 1 removes it from the image, Plasma Login Manager stays
  installed but disabled; Plasma's own login settings module targets it and
  is therefore inert, which is fine, because Guardians configure Cairn
  through the Guardian tool.
- KDE is moving from SDDM to its fork, so SDDM's pace may slow. If Fedora
  ever drops it, option C is the fallback, and the theme's QML carries over
  as the greeter's screens.
- P0-3's done-when is unchanged: a child lands in the kiosk with no
  password prompt; a Guardian lands in Plasma with one; `su - <child>` and
  an empty-password unlock both fail.
