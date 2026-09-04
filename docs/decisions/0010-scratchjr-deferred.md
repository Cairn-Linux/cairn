# ADR-0010: ScratchJr is a desired feature; how to provide it is decided after Phase 0

**Status:** accepted
**Amends:** ADR-0001
**Date:** 2026-09-03
**Closes:** ROADMAP D15 (defers the implementation question)

## Context

ScratchJr is the canonical block-coding tool for ages five to seven and has
no Linux build. DESIGN §5.2 treated porting it as a distinct workstream that
could start any time, and §13 listed it as ongoing/parallel work. Options
weighed: port it now under the Cairn organisation, contribute a Linux patch
to the community desktop port without owning it, fill the "Build" tile from
software that already exists, or decide after the child test.

The maintainer's position: get the distribution to a solid place first.
Nothing about ScratchJr is needed to validate the experience in Phase 0, and
a port with no upstream home is the kind of fork DESIGN §1.5 warns about.

## Decision

- **ScratchJr stays on the wish list as a desired feature.** It is not a
  workstream, not a repository, and not a Phase 0 or Phase 1 task.
- **How to provide it is a future decision**, taken after Phase 0 exits and
  the image is stable, with two facts in hand: whether a young child missed
  block coding at L1 during the child test, and what the community port and
  Flathub look like at that time. The options recorded above remain open.
- **Meanwhile the "Build" tile is filled from existing software** via the
  catalogue survey (ADR-0009, issue #26): GCompris programming activities at
  L1; Scratch 3 and KTurtle as candidates at L2 and above.

## Consequences

- DESIGN §5.2's "ScratchJr problem" paragraph and §13's parallel workstream
  are rewritten to say this.
- No `Cairn-Linux/scratchjr-linux` repository is created.
- The Scratch Foundation branding review (issue #22) is no longer
  pre-release work unless the Scratch 3 Flatpak is adopted, in which case it
  applies to how that tile is named.
- Issue #23 becomes a future item rather than a Phase 3 deliverable.
