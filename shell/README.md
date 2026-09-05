# shell

The restricted, child-facing terminal (DESIGN §3.1). A purpose-built shell,
not bash with a constrained `.bashrc`. Its name is **ROADMAP D9**; until
then it is "the restricted shell", and `cairn-shell-core` below is a
placeholder CMake target name.

Phase 0 task **P0-6** builds v0 in **C++ / Qt / QML** (ADR-0002), in slices.

- **Slice 1 (2026-09-04):** the interpreter, with no GUI dependency: five
  commands over the app tree, suggestions instead of errors, completion,
  and tests including the refusal cases.
- Next: the QML text surface, `open` wired to the launcher, a shared
  manifest reader, and the D9 decision on where the shell runs.

## What the shell sees (ADR-0013)

Not the filesystem. The tree is the same `kidscan` manifest the launcher's
tiles come from: the root holds one folder per kind that has entries, in
the launcher's order (`make`, `practice`, `machine`), and each entry is the
lowercase slug of its title (`draw`, `tux-paint`, `draw-2` when a name
repeats).
`open` hands the entry's exec list to the host; the interpreter never runs
anything and never touches a file.
Paths, program names and anything else outside the tree are names the shell
cannot find.

## The five commands

| Typed | Does |
|---|---|
| `ls`, `ls make` | shows what is here, or what is in a folder at the root |
| `cd make`, `cd ..`, `cd /`, `cd` | goes into a folder; the other three go back to the root |
| `open draw` | names the program for the host to start |
| `cat draw` | reads a thing; at L1 nothing is readable yet |
| `help`, `help open` | one line per command |

Input is case-insensitive.
A command takes at most one name.
The prompt shows `/` at the root and `/make` inside a folder.
Completion offers commands before the first space and names in the current
folder after it.

## What it says when something goes wrong

Every sentence names the next thing to type and never blames the child.
This is the vocabulary write-up for child testing (DESIGN §14 Q2); the
tests pin every string.

| Situation | Says |
|---|---|
| unknown command, close to one | I don't know "opn". Did you mean open? |
| unknown command | I don't know "sudo". Type help to see what I know. |
| two names | open takes one name at a time. |
| `open` or `cat` alone | open needs a name. Type ls to see them. |
| a folder where a thing was wanted | make is a place. Type cd make to go there. |
| a thing where a folder was wanted | draw is a thing to open, not a place. Type open draw. |
| `cat` on a thing | draw is a thing to open, not to read. Type open draw. |
| the thing is in another folder, from the root | draw is in make. Type cd make first. |
| the thing is in another folder, from a folder | draw is in make. Type cd .. and then cd make. |
| `cd` from one folder straight to another | practice is next door. Type cd .. first. |
| a name close to one here | I can't find "drw". Did you mean draw? |
| no such name | I can't find "freddi" here. Type ls to see what is here. |
| an empty manifest | Nothing here yet. |

Did-you-mean allows one edit for words of three letters or fewer and two
for longer ones, and a tie goes to the earlier candidate, so the answer is
the same every time.

## Build and test

Part of the root CMake project; `ctest --preset debug` runs the three
suites (`shell_apptree`, `shell_didyoumean`, `shell_interpreter`).
Refresh the translation catalogue with
`cmake --build --preset debug --target shell_lupdate`.

## Requirements carried from the design

- **Real command names.** `ls`, `cd`, `open`, `cat`. The value is transfer to
  a real shell later; invented kid-verbs mean relearning.
- L1 vocabulary candidate: five commands (`ls`, `cd`, `open`, `cat`, `help`).
  L2: about twelve. Both need testing with a child (DESIGN §14 Q2).
- Large type, generous line height, high contrast. Atkinson Hyperlegible
  Mono, 18px minimum, on Ink (`Tokens.fontFamilyMono`, `Tokens.terminalSize`).
- Aggressive completion and hinting.
- Errors suggest, never scold. Never a stack trace, never a raw errno, never
  an exit code.
- Nothing destructive reachable at L1 or L2.
- Icon-augmented `ls` for pre-readers: every `ls` line carries its kind, so
  the surface can draw the icon.
- Perfect determinism: same input, same result, every time.

## Open

- Where the shell runs: a screen inside the launcher's window, or its own
  program started from the Terminal tile. D9 decides; slice 2 needs it.
- How L3's real shell hands off from this one without a vocabulary cliff
  (Phase 2).
