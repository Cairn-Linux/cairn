# session

Login-to-session plumbing. Makes "level is a property of the account"
(DESIGN §3, §4.3) true on a real Linux system.

Nothing here yet. Phase 0 task **P0-3** builds it, resolving **ROADMAP D5,
D6, D7**.

## Proposed mechanism (pending ADRs)

- **Level = supplementary group.** `cairn-l1`, `cairn-l2`, `cairn-l3`,
  `cairn-l4`, `cairn-guardian`; exactly one per account. Level change is a
  group change.
- **One session entry.** `cairn.desktop` runs `cairn-session`, which reads
  the account's level group and execs either the kiosk compositor with the
  launcher (L1/L2) or `startplasma-wayland` (L3/L4, Guardian). The greeter
  offers only this session.
- **Greeter.** SDDM with a Cairn QML theme (since ADR-0002 everything
  first-party is QML, and SDDM themes are QML): large avatar tiles, no
  password for `cairn-l1`/`cairn-l2` (PAM rule), Guardians hidden with
  `HideUsers`. greetd with a custom greeter is the fallback.
- **Kiosk compositor.** labwc in kiosk configuration (ADR-0004): window
  rules keep Steam's forced windows off the screen and
  wlr-foreign-toplevel-management tells the launcher when one appears. cage
  is a measurement baseline only. Proven in P0-10.

## Files expected here

`cairn-session`, `cairn.desktop`, `sddm.conf.d/`, `pam.d/`, and the SDDM
theme under `theme/`.
