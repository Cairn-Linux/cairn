# ADR-0009: No first-party make-app in v1; survey existing software instead

**Status:** accepted
**Date:** 2026-09-03
**Closes:** ROADMAP D14

## Context

DESIGN §6.3 accepted that third-party apps cannot be restyled and proposed
writing "one or two first-party apps in the house style so the aesthetic has
somewhere to live". Candidates weighed: drawing (duplicates Tux Paint),
music composing (a real gap on Linux), a picture-book/story maker (a real
gap, strongest fit to the reading-level ladder), or none yet.

Three jobs were bundled in the proposal: carrying the look, being better for
a child than what exists, and holding the emotional payload of a child's
creations. The third belongs to a creations gallery, which the design
already places in the launcher as the L2 files view (§6.1, §11), not to any
particular make-app.

The maintainer's stance, consistent with DESIGN §1.5: prefer software the
project does not have to maintain.

## Decision

- **No first-party make-app in v1.** The frame (login, launcher, terminal),
  and from Phase 2 the creations gallery inside the launcher, carry the
  visual identity. The terminal is already the most distinctive first-party
  surface in the system.
- **Survey existing, maintained, kid-oriented software** for the front-door
  kinds the catalogue is thin on: music composing, writing/story, camera,
  and coding (see ADR on ScratchJr when it lands). Criteria and a starter
  candidate list are in the roadmap workstream and its issue.
- **Revisit after Phase 2** with the survey results and the child-test
  findings. If a gap is confirmed and matters to children in practice, the
  story maker is the leading candidate, then music.

## Consequences

- Phase 3 loses its "one first-party app" item and gains "catalogue
  additions from the survey".
- The launcher's L2 files view is now explicitly the creations gallery, with
  export to a Guardian-readable place (§11), and inherits the design weight
  the app would have carried. Its quality matters more.
- The website's screenshots in v1 show the frame and third-party apps as
  contents, which is honest about what the system is.
- Anything adopted from the survey must pass: actively maintained, packaged
  (Flatpak or Fedora), works offline, no accounts, ads or telemetry, free
  licence, runs under Wayland/XWayland, fits the age band with mouse and
  keyboard, and sits within the Minimum tier's memory.
