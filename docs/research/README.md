# Research notes

Observations from real hardware and real children. Phase 0 exits on the
strength of what is written here, not on what was built.

Files:

- `child-test-01.md` — the plan and notes sheet for P0-9, the first test
  with children on a laptop: setup steps rehearsed in the VM, the ways
  out, what to watch, and where the journal keeps what went wrong. It was
  planned for two cousins on 2026-10-02, and that run never happened. The
  notes cover the kids' laptop: two children at L1 on 2026-10-10, and the
  younger one alone on 2026-10-11, with a table of those evenings against
  the exit criterion. Phase 0 ended on those evenings (ADR-0031).
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
  app; the power button taps to ignore and holds to power off. The child's
  own way out (#77), missing in the first pilot, is Super+Q (ADR-0019),
  checked in the VM on 2026-09-29 against Tux Paint, GCompris, a Steam
  game and the frame. Since 2026-10-31 the kiosk keeps Ctrl+F5 from
  every app, so it no longer opens ScummVM's own menu over a game
  (#215); whether a program can get past a labwc binding, and the
  ScummVM keys still open, are written down there too.
- `steam-containment.md` — 2026-09-08 P0-10 spike in the VM with a
  signed-out client: which window rule hides Steam's windows and why, what
  the launcher shows, what Settings › Family offers now, and the client's
  memory against the 4 GB floor. The signed-in rows ran the same evening:
  no window from a silent client, `-applaunch` from a tile up and back,
  the login through a reboot, 1.4 GB proportional signed in. D3 closed
  with ADR-0017.
- `steam-visit.md` — 2026-10-12 P1-5 check in the VM: a logged-out
  child's own Steam opened on the Guardian's Plasma desktop. Sharing the
  Guardian's Xwayland with `xhost` works but exposes the Guardian's X11
  windows, keys and clipboard, and is rejected. cage in a window, on a
  socket systemd hands over, works and held in the tests run, but any
  process of the child can copy cage's connection to KWin, which is
  offered the Guardian's clipboard and more. Ends with what the helper of
  ADR-0032 (proposed) must do.
- `shared-steam-library.md` — 2026-10-12 and 13, #130: desk research, a
  probe on the development PC and read-only checks on the kids' laptop.
  One Steam library shared between Linux accounts fails on Steam's
  runtime, Wine's prefix and Mesa's cache, lets any child change a game,
  and shares saves, which the games keep inside their own folder. Leads to
  ADR-0033 (proposed), a copy of every game for every child, and ends with
  the checks still to run on the laptop. A section from 2026-10-29 covers
  moving a game between libraries by hand, for ADR-0042 (proposed).

- `malcontent-plasma.md` — 2026-09-06 P0-7 check in the VM: malcontent 0.14
  blocks a blocklisted Flatpak and user installs under Plasma, and its
  control app runs there. Its session timer and web filter are noted for
  P1-9 and D10.
- `launcher-footprint.md` — 2026-09-08 P0-5 measurements in the VM: the
  launcher idles at about 145 MB proportional and the whole L1 session at
  about 235 MB, leaving 2.9 GB of the 4 GB floor; first frame 0.2 s after
  exec, tiles 1.75 s after Enter at the greeter, Tux Paint 6 to 7 s from
  tile to a usable screen under llvmpipe. The laptop run is still owed.

Write what happened, not what should have happened.
