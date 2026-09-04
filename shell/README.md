# shell

The restricted, child-facing terminal (DESIGN §3.1). A purpose-built shell,
not bash with a constrained `.bashrc`. Its name is **ROADMAP D9**; until
then it is "the restricted shell".

Nothing here yet. Phase 0 task **P0-6** builds v0 in **C++ / Qt / QML**
(ADR-0002).

## Proposed architecture (pending the D9 ADR)

Not a PTY program running inside a terminal emulator. Instead: a pure C++
command interpreter class with no GUI dependency, driven by a QML text
surface. The interpreter is unit-tested on its own with Qt Test; the surface
gives the design its large type, generous line height and icon-augmented
`ls` directly. At L3 the child moves to a real shell in a real terminal
emulator, and that hand-off is an explicit, visible step rather than a
vocabulary cliff.

## Requirements carried from the design

- **Real command names.** `ls`, `cd`, `open`, `cat`. The value is transfer to
  a real shell later; invented kid-verbs mean relearning.
- L1 vocabulary candidate: five commands (`ls`, `cd`, `open`, `cat`, `help`).
  L2: about twelve. Both need testing with a child (DESIGN §14 Q2).
- Large type, generous line height, high contrast. Atkinson Hyperlegible
  Mono, 18px minimum, on Ink (`--cairn-ink`).
- Aggressive completion and hinting.
- Errors suggest, never scold: `I don't know "opn" — did you mean open?`
  Never a stack trace, never a raw errno, never an exit code.
- Nothing destructive reachable at L1 or L2.
- Icon-augmented `ls` for pre-readers.
- Perfect determinism: same input, same result, every time.

## Open

How L3's real shell hands off from this one without a vocabulary cliff
(Phase 2).
