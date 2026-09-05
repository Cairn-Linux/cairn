# ADR-0013: The L1 shell sees the launcher's manifest, not a filesystem

**Status:** accepted
**Date:** 2026-09-04

## Context

[DESIGN §3](../DESIGN.md#3-the-capability-ladder) gives L1 "launcher only, no
filesystem" and a shell that "launches apps only".
[DESIGN §3.1](../DESIGN.md#31-the-terminal) and ROADMAP P0-6 give that shell
`ls`, `cd`, `open`, `cat` and `help`.
Issues #46 and #35 ask what those commands see, where `cd` may go, and
whether `open` can reach `xdg-open`, which would be a door around the
allowlist.
The first shell code needs an answer.

## Decision

At L1 the restricted shell's tree is built from the same `kidscan` manifest
the launcher's tiles come from.
The root holds one folder per kind that has entries, in the launcher's order:
`make`, `practice`, `machine`.
Each entry is named by the lowercase slug of its title (`draw`, `tux-paint`),
with a number appended when a name repeats.
`ls` and `cd` walk that tree and nothing else.
`open` resolves only to an entry in the current folder and hands its exec
list to whatever hosts the shell, which starts it the way a tile does; a
path, a program name or `xdg-open` is just a name the shell cannot find.
`cat` stays in the vocabulary for continuity with L2, where it will read the
child's own files, but at L1 nothing is readable and it answers with the next
step.
The interpreter has no filesystem access at all.
L2's `files` folder over the creations directory
([DESIGN §11](../DESIGN.md#11-user-data-and-reset)) is Phase 2's to design.

## Consequences

- The DOS-era loop the design wants (`cd games`, then a name, then a reward)
  exists without any filesystem: `cd make`, `open draw`.
- Nothing outside the tree is reachable; the refusal cases are tests.
- Whether L1 keeps `cat`, or five commands is the right number, is what child
  testing decides (DESIGN §14 Q2). The shell's wording is written up in
  `shell/README.md` for that.
- The shell and the launcher read one manifest, so the manifest reader must
  become shared code before the shell is wired in.
- The `/usr/bin/open` wrapper for L3 (#46) is unaffected and still owed.
