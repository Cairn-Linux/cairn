# Research notes

Observations from real hardware and real children. Phase 0 exits on the
strength of what is written here, not on what was built.

Files:

- `base-image-policy.md` — 2026-09-03 review of Bazzite, Universal Blue and
  Fedora trademark and licence terms for building Cairn on Bazzite. No
  blocker; compliance checklist for Phase 1.
- `kiosk-containment.md` — 2026-09-06 P0-4 checklist under nested labwc:
  what a child can and cannot reach from inside the L1 session. Two rows
  passed only after changes to `session/labwc/`. The three VM rows ran on
  2026-09-08: VT switching and the shortcuts hold; `pkcheck` as the child
  found polkit wide open, and the rule in `session/polkit/` (#35) closed
  it the same day. The hung-app row (#41) is answered by ADR-0018: a
  grown-up key combination runs `cairn-give-up`, verified against a frozen
  app; the power button taps to ignore and holds to power off.
- `steam-containment.md` — 2026-09-08 P0-10 spike in the VM with a
  signed-out client: which window rule hides Steam's windows and why, what
  the launcher shows, what Settings › Family offers now, and the client's
  memory against the 4 GB floor. The signed-in rows ran the same evening:
  no window from a silent client, `-applaunch` from a tile up and back,
  the login through a reboot, 1.4 GB proportional signed in. D3 closed
  with ADR-0017.

- `malcontent-plasma.md` — 2026-09-06 P0-7 check in the VM: malcontent 0.14
  blocks a blocklisted Flatpak and user installs under Plasma, and its
  control app runs there. Its session timer and web filter are noted for
  P1-9 and D10.
- `launcher-footprint.md` — 2026-09-08 P0-5 measurements in the VM: the
  launcher idles at about 145 MB proportional and the whole L1 session at
  about 235 MB, leaving 2.9 GB of the 4 GB floor; first frame 0.2 s after
  exec, tiles 1.75 s after Enter at the greeter, Tux Paint 6 to 7 s from
  tile to a usable screen under llvmpipe. The laptop run is still owed.

Planned files:

- `child-test-01.md` — P0-9: who, how old, what hardware, twenty minutes,
  what happened. Observer does not intervene unless asked.

Write what happened, not what should have happened.
