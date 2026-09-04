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
- **Greeter.** greetd with a first-party greeter in the house style: large
  avatar tiles, no password for `cairn-l1`/`cairn-l2` (PAM rule), Guardian
  accounts hidden and password-protected. SDDM + QML theme is the fallback.
- **Kiosk compositor.** cage first; labwc if Steam's forced windows can't be
  contained under cage (D3, tested in P0-4).

## Files expected here

`cairn-session`, `cairn.desktop`, `greetd/config.toml`, `pam.d/`, and the
greeter once D2 settles the UI technology.
