# ADR-0011: Level is a supplementary group

**Status:** accepted
**Date:** 2026-09-04
**Closes:** ROADMAP D5

## Context

[DESIGN §3](../DESIGN.md#3-the-capability-ladder) makes level a property of
the account, with symmetric changes up and down the ladder.
[DESIGN §4.3](../DESIGN.md#43-sessions-and-accounts) gives each child a real
Linux account whose level determines the session at login.
These requirements need one mechanism that session dispatch and access
policy can both read.
P0-2 needs that mechanism before it creates the first accounts; choosing it
does not depend on the P0-3 greeter experiment.

## Decision

Use supplementary groups `cairn-l1`, `cairn-l2`, `cairn-l3`, `cairn-l4` and
`cairn-guardian`, with exactly one of these groups per Cairn account.
A level change is a group change.
PAM, polkit, the session dispatcher and malcontent policy key off the group.
Per-child name, avatar and allowlists live in a small config owned by the
Guardian tool.

## Consequences

- The level is inspectable with `id` and legible through standard Linux
  account tools, as DESIGN §2 requires.
- Raising and lowering a level use the same group-change mechanism, keeping
  level changes symmetric.
- P0-2 can create accounts without waiting for P0-3 to choose a greeter.
- The privilege-model ADR in issue #35 still owes the polkit rules and where
  the PIN hash lives.
