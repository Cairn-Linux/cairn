# ADR-0014: The terminal is Footpath, inside the launcher, in its own repository

**Status:** accepted
**Date:** 2026-09-05
**Closes:** ROADMAP D9
**Amends:** ADR-0001 (the shell leaves the monorepo)

## Context

ROADMAP D9 asked for the restricted shell's name and architecture before
Phase 1 packages it (P1-2).
ADR-0002 set the toolkit; the maintainer decided on 2026-09-04 that the
shell runs as a screen inside the launcher window, and shell slice 2 built
it that way.
[TERMINAL-DESIGN.md](../TERMINAL-DESIGN.md) makes the terminal a program
with its own purpose and audience: a shell that grows with the child and can
run on any Linux desktop, not only on Cairn.
ADR-0001 keeps every first-party component in one repository but names the
shell as the likely one to split out.
The child never sees the name; they see a Terminal tile.

## Decision

The terminal is **Footpath**: a small path a child walks that leads
somewhere, one of the cairn's trail family.
The package, the repository and the CMake target carry the name
(`footpath`, `Cairn-Linux/footpath`, `footpath-core`); the tile stays
"Terminal".
Its architecture is what exists today: a Qt Core interpreter with no GUI
dependency, a QML surface the host places in its window, and a small host
interface (doors, level, the child's name, launch and leave), not a PTY
program in a terminal emulator.
Footpath moves to its own repository with `TERMINAL-DESIGN.md` as its design
document and a standalone window with a demo world as its second host.
Cairn consumes it as a git submodule pinned to a tag, built by the
launcher's CMake, and Phase 1 packages it as an RPM from that tag.

Collisions checked 2026-09-05: no Fedora package, no PyPI project, no
crates.io crate; only unrelated small repositories on GitHub.
Trademark review stays with issue #22.

## Consequences

- D9 closes; P1-2 packages `footpath` from its repository.
- The split is staged. This ADR names it and renames the target; the next
  change creates `Cairn-Linux/footpath` from `shell/`, the launcher's
  `Terminal.qml`, `TerminalSession` and `OutputModel`, and the design
  document, then replaces them here with the submodule.
- Cairn's launcher gains its first non-Qt dependency, which is this
  project's own code; CLAUDE.md's "Qt only" rule reads "Qt and Footpath".
- Footpath's code is Apache-2.0 like Cairn's; the Cairn name and mark do not
  travel with it.
- Two repositories mean two issue trackers and, later, two CI setups; the
  manifest and the host interface become a contract between them.
