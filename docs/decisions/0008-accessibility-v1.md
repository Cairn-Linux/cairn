# ADR-0008: Accessibility in v1 is high-contrast and large-text modes

**Status:** accepted
**Date:** 2026-09-03
**Closes:** ROADMAP D13 (DESIGN §14 Q6)

## Context

DESIGN §14 Q6 notes that many children this age have undiagnosed vision
issues and asks what accessibility beyond typography v1 should carry. The
brand already commits to Atkinson Hyperlegible, 18px minimum child-facing
text and a restrained high-contrast palette.

## Decision

- **In v1:** a **high-contrast mode** and a **large-text mode**, both
  per-child settings a Guardian can turn on, both implemented as variants of
  the brand token set (`brand/tokens.json`) so every first-party surface
  follows automatically. Large text scales the type scale, not the layout
  grid. Third-party apps are not restyled (DESIGN §6.3); Plasma's own
  settings cover them at L3/L4.
- **Phase 3:** screen reader support (Qt's accessibility bridge to AT-SPI
  and Orca, plus sensible accessible names on every QML control from the
  start so this is possible later), dwell click, and a review with an
  accessibility specialist.

## Consequences

- Brand v0.2 (issue #20) must define the high-contrast variant of the
  palette and a large-text scale; `brand/build.py` emits both.
- Every QML control gets an `Accessible.name` from the first commit. It costs
  nothing now and makes Phase 3 possible.
- Colour is never the only carrier of meaning: tile kind is also shown by
  icon and label, which the design already requires.
