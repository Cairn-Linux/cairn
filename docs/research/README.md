# Research notes

Observations from real hardware and real children. Phase 0 exits on the
strength of what is written here, not on what was built.

Files:

- `base-image-policy.md` — 2026-09-03 review of Bazzite, Universal Blue and
  Fedora trademark and licence terms for building Cairn on Bazzite. No
  blocker; compliance checklist for Phase 1.
- `kiosk-containment.md` — 2026-09-06 P0-4 checklist under nested labwc:
  what a child can and cannot reach from inside the L1 session. Two rows
  passed only after changes to `session/labwc/`; three rows wait for the VM.

- `malcontent-plasma.md` — 2026-09-06 P0-7 check in the VM: malcontent 0.14
  blocks a blocklisted Flatpak and user installs under Plasma, and its
  control app runs there. Its session timer and web filter are noted for
  P1-9 and D10.

Planned files:

- `launcher-footprint.md` — P0-5: idle RSS and launch latency of the launcher
  on the Minimum tier (ADR-0003), against the 4 GB floor.
- `child-test-01.md` — P0-9: who, how old, what hardware, twenty minutes,
  what happened. Observer does not intervene unless asked.
- `steam-containment.md` — P0-10: client-window containment in the VM,
  idle RSS against the 4 GB floor and VT switching on the Minimum-tier laptop.

Write what happened, not what should have happened.
