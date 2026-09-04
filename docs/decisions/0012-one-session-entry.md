# ADR-0012: One session entry dispatches by account level

**Status:** accepted
**Date:** 2026-09-04
**Closes:** ROADMAP D7

## Context

[DESIGN §4.3](../DESIGN.md#43-sessions-and-accounts) makes the account's level
determine the session at login.
[DESIGN §4.5](../DESIGN.md#45-sessions-per-level) separates the L1/L2 kiosk
from the L3/L4 Plasma session.
ADR-0011 supplies the level group the session dispatcher reads.
A session picker must not let a child bypass that account property.

## Decision

Provide one Wayland session entry, `cairn.desktop`, running `cairn-session`.
The dispatcher reads the account's level group and execs either the kiosk
compositor with the launcher (L1/L2) or `startplasma-wayland` (L3/L4,
Guardian).
The greeter offers only this session.

## Consequences

- A child cannot pick another session at the greeter.
- The dispatcher is a trust boundary and gets the plainest code and the most
  tests, following [CLAUDE.md](../../CLAUDE.md).
- The greeter itself remains ROADMAP D6; P0-3 records what stock Bazzite KDE
  ships before choosing it.
