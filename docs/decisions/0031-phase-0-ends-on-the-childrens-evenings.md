# ADR-0031: Phase 0 ends on the children's evenings

**Status:** accepted
**Date:** 2026-10-12
**Closes:** (none) — addresses issue #8 (ROADMAP P0-9); amends ADR-0003 and
ADR-0004

## Context

Phase 0 exists to put the launcher in front of a real child before any
image tooling is built (DESIGN §13). ROADMAP §2 set the bar: one child aged
5–8, on a Minimum-tier laptop (ADR-0003), uses the L1 session unsupervised
for twenty minutes, and logs in from an avatar tile, launches and returns
from at least two apps (one through Steam), uses the shell to open
something, hits an error and recovers without help, and never leaves the
kiosk.

The run that was planned for it, two cousins on 2026-10-02, never happened:
they left before it could start. The cousins were 9 and 10, outside the
5–8 band, so it would not have met the bar as written either.

Instead, the maintainer's own children used the kids' laptop on two
evenings, written up in `docs/research/child-test-01.md`. On 2026-10-10 two
children at L1 used it offline, with a grown-up in the room. On 2026-10-11
the younger child used it alone, online, for about 45 minutes. The second
evening ran a newer build, with the longer wait for a Steam game
(ADR-0029). The fix for what the first evening found came after both
(ADR-0030).

Against the bar:

- **Seen:** logging in from a tile; launching apps, including two Steam
  games on 2026-10-11; twenty minutes alone.
- **Seen as far as the journal shows:** coming back to the tiles after each
  app, and staying in the kiosk. The journal does not say how each game
  ended, so whether the child left it on purpose is not recorded; the pilot
  child could not leave an app without help (#77). Steam's overlay was on
  and could likely have opened Steam's chat (#74).
- **Partly seen:** an error. On 2026-10-10 twelve of thirteen Steam presses
  started nothing, and the children went on to press other tiles. But the
  launcher said nothing then, so there was no error on the screen to
  recover from. ADR-0030 now shows the grown-up screen, and no child has
  seen it yet.
- **Not seen:** a child opening something from the Terminal. The pilot
  found it too advanced for a child who cannot read yet (#83).
- **Not recorded:** the children's ages. The notes give none, so whether
  the younger child is in the 5–8 band is not on record.
- **Not met:** the hardware. The kids' laptop has 14 GB of memory, not the
  4 GB floor. It is the tier where ADR-0003 puts Steam games, but no child
  has used the floor itself.
- **Not met:** #8's prerequisites for Steam. The Steam containment
  leftovers (#25), the overlay (#74) and a Guardian's way to step in (#88)
  were still open. Steam was included because the family's games are
  Steam games.

## Decision

**Phase 0 ends on 2026-10-12, on the strength of the two evenings.** Its
purpose is met: real children used the launcher on real hardware, and
what they ran into has been fixed or filed (#138 to #141, ADR-0030). None
of the rows left open has to be answered before the image is built, and
each one can be tested on the image once it exists.

**What was not seen is carried into Phase 1 by name, not dropped:**

- **P1-17, child test 2:** the Terminal, once #83 decides how a child who
  cannot read meets it; a child leaving an app on their own (#77); the
  grown-up screen and Back (ADR-0030); Steam with the overlay off (#74) and
  a Guardian's way to step in (#88), or no Steam in that run, as #8 asked;
  and #8's seven script rows, none of which an evening ran. ROADMAP's
  P1-17 row lists them.
- **The 4 GB floor** moves to P1-1, which already measures both tiers: the
  launcher's laptop memory run (P0-5) and Steam's (P0-10). P1-1 measures
  the floor without a child. P1-17 uses a Minimum-tier laptop if the
  project has one by then; it is not required.
- **The Steam leftovers** of P0-10 (the child role, the toast over the game,
  what Valve forces) move to P1-13.

Image work starts with P1-1 to P1-3, built and booted on the dev PC. No
image is made public, or goes to anyone outside the family, before P1-14,
the Guardian audit (#86) and P1-17.

## Consequences

- CLAUDE.md's rule against image plumbing is replaced by the Phase 1 start
  (P1-1 to P1-3) and the publishing gates.
- ADR-0003 said the Phase 0 child test runs on a Minimum-tier laptop. It
  did not. No child has used Cairn on the 4 GB floor, and nothing in Phase
  1 requires one to: the floor is known only by measurement.
- ADR-0004 said Steam containment is proven in Phase 0 (P0-10). Its
  leftover rows are now P1-13's, and its laptop memory run is P1-1's.
- The exit rests on the maintainer's own family, on one laptop, and mostly
  on one child. That is weaker evidence than the bar asked for, and it is
  why P1-17 exists.
- No child has been seen opening something from the Terminal. The
  Terminal's design (DESIGN §3.1) is untested with a child until P1-17.
- D15 (ScratchJr, ADR-0010) still waits. ADR-0010 takes it up once the
  image is stable and a child test has shown whether a young child misses
  block coding. The evenings showed nothing about that: Build said "coming
  soon".
