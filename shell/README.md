# shell

The restricted, child-facing terminal (DESIGN §3.1). A purpose-built shell,
not bash with a constrained `.bashrc`. Its name is **ROADMAP D9**; until
then it is "the restricted shell", and `cairn-shell-core` below is a
placeholder CMake target name.

Phase 0 task **P0-6** builds v0 in **C++ / Qt / QML** (ADR-0002), in slices.

- **Slice 1 (2026-09-04):** the interpreter, with no GUI dependency: five
  commands over the app tree, suggestions instead of errors, completion,
  and tests including the refusal cases.
- **Slice 2 (2026-09-05):** the world grows into a tree with `home`
  (TERMINAL-DESIGN §3), `exit` arrives, and the shell runs inside the
  launcher window from the Terminal tile: `launcher/qml/Terminal.qml` draws
  it and `launcher/src/TerminalSession` feeds it.
- Next: a `--home` layout file, L2's words, and the split into the
  terminal's own repository once D9 names it.

## What the shell sees (ADR-0013, TERMINAL-DESIGN §3)

Not the filesystem. The root holds the doors, one folder per kind that has
entries in the launcher's order (`make`, `practice`, `machine`), and
`home`, a small read-only layout shaped like a real Linux home:
`home/<name>` with the notes `hello` and `words`, a `notes` folder with two
more, and an empty `pictures`.
Doors come from the same `kidscan` manifest the launcher's tiles come from,
each named by the lowercase slug of its title (`draw`, `tux-paint`,
`draw-2` when a name repeats); the Terminal tile itself is not a door.
The home layout is built in, or read from a version-1 JSON file
(`HomeLayout::read`); the launcher does not pass one yet.
`open` hands a door's exec list to the host; the interpreter never runs
anything and never writes a file.
Paths, program names and anything else outside the world are names the
shell cannot find.

## The six commands

| Typed | Does |
|---|---|
| `ls`, `ls notes` | shows what is here, or what is in a folder here |
| `cd notes`, `cd ..`, `cd /`, `cd` | goes into a folder, up one, or back to the root |
| `open draw` | names the program for the host to start; on a note, reads it |
| `cat hello` | reads a note |
| `help`, `help open` | one line per command |
| `exit` | leaves the terminal; the host returns to the tiles |

Input is case-insensitive.
A command takes at most one name, and a name is one step: `cd home/sam` is
a name the shell cannot find.
The prompt shows the location: `/`, `/make`, `/home/sam/notes`.
Completion offers commands before the first space and names in the current
folder after it; the surface shows the rest of the first match as ghost
text and Tab or Right accepts it.

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
| a note where a folder or thing was wanted | hello is a note. Type cat hello to read it. |
| the thing is elsewhere, from the root | draw is in make. Type cd make first. |
| the thing is elsewhere, from a folder | draw is in make. Type cd / and then cd make. |
| a top-level folder named from inside another | practice is at the top. Type cd / first. |
| a name close to one here | I can't find "drw". Did you mean draw? |
| no such name | I can't find "freddi" here. Type ls to see what is here. |
| an empty manifest | Nothing here yet. |

Did-you-mean allows one edit for words of three letters or fewer and two
for longer ones, and a tie goes to the earlier candidate, so the answer is
the same every time.

## Build and test

Part of the root CMake project; `ctest --preset debug` runs the four
suites (`shell_world`, `shell_homelayout`, `shell_didyoumean`,
`shell_interpreter`) and, in the launcher, `terminalsession` and the QML
navigation suite's terminal cases.
Refresh the translation catalogue with
`cmake --build --preset debug --target shell_lupdate`.

## Requirements carried from the design

- **Real command names.** `ls`, `cd`, `open`, `cat`. The value is transfer to
  a real shell later; invented kid-verbs mean relearning.
- L1 vocabulary candidate: six commands (`ls`, `cd`, `open`, `cat`, `help`,
  `exit`). L2: about twelve. Both need testing with a child (DESIGN §14 Q2).
- Large type, generous line height, high contrast. Atkinson Hyperlegible
  Mono, 18px minimum, on Ink (`Tokens.fontFamilyMono`, `Tokens.terminalSize`).
- Aggressive completion and hinting.
- Errors suggest, never scold. Never a stack trace, never a raw errno, never
  an exit code.
- Nothing destructive reachable at L1 or L2.
- Icon-augmented `ls` for pre-readers: every `ls` line carries its kind, so
  the surface can draw the icon.
- Perfect determinism: same input, same result, every time.

## Where this is going

The terminal is to become its own project; its design, including the L2
words and the L3 hand-off, is `../docs/TERMINAL-DESIGN.md`. What is here,
with the surface and session in `../launcher/`, moves with it once D9
names it.
