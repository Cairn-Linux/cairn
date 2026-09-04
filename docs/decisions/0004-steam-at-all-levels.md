# ADR-0004: Steam titles are available at every level, including L1 and L2

**Status:** accepted
**Date:** 2026-09-03
**Closes:** ROADMAP D12

## Context

DESIGN §8.3 says Steam titles are supported at all levels, including L1 and
L2, through a silently running client and `steam -applaunch` tiles. The
ladder table in §3 said games at L1 and L2 were "curated tiles" and placed
"Steam, allowlisted" at L3. The two had to be reconciled.

The cost of L1/L2 Steam is containment. A kiosk has no tray. When the Steam
client forces its window open (client update, game update, login expiry,
Steam Guard, cloud-save conflict) that window lands fullscreen in front of a
five-year-old. Keeping it off the screen needs a compositor with window rules
and a way for the launcher to learn that a foreign window appeared, and a
client running in every young child's session costs memory on the 4 GB floor
(ADR-0003).

Three options were weighed: Steam at every level (A), Steam from L3 up with
native ScummVM titles only at L1/L2 (B), and B for v1 with the kiosk case as
a post-v1 goal (C). The recommendation was C. The maintainer chose A: they
have a large library of kid-friendly Steam games and want it available to
their own children at every level from the first release.

## Decision

- Steam-routed tiles are a **v1 requirement at every level**, gated per
  child by the Steam Family allowlist and the Guardian's per-child game
  allowlist (DESIGN §3.2).
- The L1/L2 kiosk compositor must provide **window rules** and
  **foreign-toplevel notifications**. This rules out a single-app compositor;
  D3 moves to **labwc** as the primary candidate, with cage kept only as a
  measurement baseline.
- **Containment is proven in Phase 0**, before the launcher is finished, as
  a dedicated spike (ROADMAP P0-10). The child test (P0-9) includes launching
  at least one Steam-routed title.
- **Steam integration moves from Phase 3 to Phase 1** (P1-13): silent client
  at login, `-applaunch` tiles, the launch watchdog and the "Something needs a
  grown-up" screen, Family View PIN, Steam Families documentation. Minecraft,
  the `kidscan` GUI and icon sourcing stay in Phase 3.
- The Steam client's idle memory counts against the Minimum tier and is
  measured in P0-10.

## Consequences

- The base image decision (D4) tilts further toward Bazzite, which ships the
  Steam client and its runtime dependencies already integrated.
- The kiosk compositor is now labwc with window rules rather than cage; P0-4
  and P0-10 test both the rules and the failure path where a rule misses.
- Phase 0 is larger and its riskiest item is first, which is the point of
  Phase 0.
- The same containment problem applies to Minecraft's launcher window in
  Phase 3; the spike's findings apply directly.
- `tools/kidscan` must emit the correct launch command for Flatpak Steam
  (issue #24) sooner than planned.
