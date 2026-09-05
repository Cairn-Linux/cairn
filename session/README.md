# session

Login-to-session plumbing. Makes "level is a property of the account"
(DESIGN §3, §4.3) true on a real Linux system.

So far only the kiosk compositor's configuration, `labwc/rc.xml`: no
titlebars, no default key or mouse bindings, and a window rule that makes
the launcher fullscreen on sight.
Fullscreen is the compositor's decision, made per session, never a launcher
option.
`tests/test_labwc_config.py` checks the file says so; CTest runs it.
Phase 0 task **P0-3** builds the rest, resolving **ROADMAP D6** only.

## Decided

- **Level = supplementary group (ADR-0011).** `cairn-l1`, `cairn-l2`,
  `cairn-l3`, `cairn-l4`, `cairn-guardian`; exactly one per account.
  Level change is a group change.
- **One session entry (ADR-0012).** `cairn.desktop` runs `cairn-session`,
  which reads the account's level group and execs either the kiosk
  compositor with the launcher (L1/L2) or `startplasma-wayland` (L3/L4,
  Guardian).
  The greeter offers only this session.

## Proposed mechanism (pending ADRs)

- **Greeter.** SDDM with a Cairn QML theme (since ADR-0002 everything
  first-party is QML, and SDDM themes are QML): large avatar tiles, no
  password for `cairn-l1`/`cairn-l2` (PAM rule), Guardians hidden with
  `HideUsers`.
  Fedora 44 KDE ships Plasma Login Manager (`plasmalogin.service`), not
  SDDM, as verified on 2026-09-04.
  Bazzite's Containerfile swaps to SDDM only in its Steam Deck stage.
  P0-3 first records what stock Bazzite KDE ships; the SDDM recommendation
  rests on that check, and the D6 ADR must make any swap explicit.
  greetd with a custom greeter is the fallback.
- **Kiosk compositor.** labwc in kiosk configuration (ADR-0004): window
  rules keep Steam's forced windows off the screen and
  wlr-foreign-toplevel-management tells the launcher when one appears. cage
  is a measurement baseline only.
  labwc remains proposed until P0-10 closes D3 with an ADR.

## Files expected here

`cairn-session`, `cairn.desktop` and `pam.d/`, beside `labwc/`.
If D6 chooses SDDM, also `sddm.conf.d/` and the theme under `theme/`.
