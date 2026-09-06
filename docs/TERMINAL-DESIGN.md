# The terminal — Design Document

**Status:** Draft for discussion, revised 2026-09-05
**Intended home:** its own repository once ROADMAP D9 names it; until then
this file lives beside `DESIGN.md` and is argued with here
**Depends on:** `DESIGN.md` §3.1 (the premise), ADR-0002 (C++/Qt/QML),
ADR-0013 (what the L1 shell sees)

A shell that grows with the child. Six words at five years old, about
twelve at seven, and at nine the same words work in a real terminal.

---

## 1. What it is

A program, not bash with a constrained profile. It looks like a terminal
and answers like one, but every command is ours, so every answer can be
calm, every mistake can suggest, and nothing a child types can hurt.

It runs first as a screen inside the Cairn launcher, on the Terminal tile.
It is built to run anywhere else too: as a window on any Linux desktop, with
a demo world, so a family that does not run Cairn can still hand it to a
child. That is why it is its own project.

### 1.1 Why a child would want it

`DESIGN.md` §3.1 gives the premise: a generation learned computing by typing
`cd games` and a name into MS-DOS, and it worked because there was a reward
on the other side, a tiny vocabulary, perfect determinism and
consequence-free failure. The terminal is those four properties, built on
purpose.

The reward is real: `open draw` starts the drawing program. The vocabulary
is real: `ls`, `cd`, `open`, `cat`. The determinism is total: same input,
same answer, every time. The failure is free: nothing the child can type
changes anything, so there is nothing to break.

### 1.2 Non-goals

- Not a terminal emulator. It runs no other programs inside itself and
  speaks no escape codes.
- Not a programming environment. Scripting, pipes and variables are L3's,
  in bash.
- Not a game. There are no points, streaks or badges. The reward is the
  door opening.
- Not a mascot. Nobody talks to the child except the terminal, in the calm
  voice of `DESIGN.md` §6.

---

## 2. Principles

1. **Real words only.** Every command name and every argument form is one
   bash accepts, or will accept through a small wrapper (`open`). A child
   who learns here relearns nothing later.
2. **A small world to explore.** The terminal shows two kinds of thing:
   places to go and look, and things to open. Nothing else.
3. **Consequence-free exploration.** No word the child knows makes,
   changes or deletes anything. The world is the same tomorrow as today.
4. **Suggest, never scold.** A mistake is answered with the next thing to
   type. No errors, no codes, no blame.
5. **Same answer every time.** No randomness, no timing, no state the child
   cannot see.
6. **Grows without changing.** More words arrive with the child's level.
   The screen, the prompt and the old words stay exactly as they were.
7. **Nothing phones home.** No network, no accounts, no analytics.
8. **The host decides what is real.** What can be opened, where a file is
   really kept and which level the child is are told to the terminal by
   whatever hosts it. The terminal never finds them out for itself.

---

## 3. The world

What `ls` shows at the root:

```
make   practice   machine   home
```

The first three are **doors**: the programs the child can open, grouped by
kind as the launcher groups them (ADR-0013). They come from the host, and
the terminal cannot change them.

`home` is a **small fictional computer**: a few folders and notes laid out
the way a real Linux home is, so that `cd home`, `cd notes`, `ls` and
`cat hello` teach the shape a child will meet again at L3. It is authored,
not made by the child, and the child cannot change it. It ships with the
terminal as one small file; a host may replace it with its own.

At L2 one real thing appears inside it: `home/<name>/pictures`, the child's
creations directory (`DESIGN.md` §11), read-only. `ls` shows the drawings
with their pictures; `open` on one shows it. The terminal never writes
there.

### 3.1 Names

Everything has a lowercase name with no spaces: `draw`, `tux-paint`,
`notes`. Titles from the host become names by the slug rule in ADR-0013.
Files the host hands over at L2 keep their real names, lowercased for
matching.

### 3.2 What is in `home`

The starter layout is small on purpose: `home/<name>` with a note that
says hello, a note that lists the words, a folder `notes` with two more
notes, and `pictures` (empty until L2). Enough that `ls` is never empty and
`cat` has something to read at L1; not so much that the layout is a maze.
The wording of the notes is part of the vocabulary write-up and gets
child-tested like everything else.

---

## 4. The vocabulary ladder

| Level | Words | What is new |
|---|---|---|
| L1 (5–6) | `ls` `cd` `open` `cat` `help` `exit` | looking, going, opening, reading, leaving |
| L2 (7–8) | L1 plus `pwd` `tree` `clear` `history` `echo` `whoami` | seeing where you are and what you did; `pictures` appears |
| L3 (9–11) | a real shell | the same words in bash, plus everything else |

Every word at L1 and L2 looks, moves or opens. None makes, changes or
deletes anything, at any level: that is bash's job at L3, in a real home.
Twelve at L2 is the design's number; the exact set is child-tested
(`DESIGN.md` §14 Q2). Candidates not in the table: `find`, `head`, `date`.

### 4.1 Rules that hold at every level

- A command behaves as bash does for the forms it accepts. `cd ..` goes up,
  `cd` alone goes to the root, `ls name` looks into a folder, `echo hi`
  says hi. Forms it does not accept get a suggestion, not a surprise.
- No flags at L1. `ls -la` is a name the terminal cannot find. Flags arrive
  when a level needs them, one at a time, and each is a real one.
- One name at a time at L1 and L2. No command here needs two.
- `open` always means the host starts something. On a door it starts the
  program. On a picture it shows the picture. On a note it reads it, like
  `cat`, because a note has nothing else to open.
- `exit` leaves the terminal and returns to the launcher, at every level.
  It is the one word that does something outside the world, and it is the
  first word a child should be sure of.
- Nothing the child types ever reaches the real filesystem, the network or
  another program, except through `open`, which the host carries out from
  its own allowlist.

### 4.2 The hand-off at L3

At L3 the child gets bash in a real terminal emulator. Three things make
that a step and not a cliff:

- Every word they know works, because they were real. `open` works through
  a small `/usr/bin/open` wrapper the OS packages (issue #46).
- The shape is the same. `home/<name>/notes` and `home/<name>/pictures`
  were laid out like a real home on purpose, so the real one is familiar,
  and `pictures` was already the real one.
- The first `help` in bash is a page that says which words they already know
  and which are new. That page is the OS's, not this project's.

---

## 5. What it says

The rules from `DESIGN.md` §6 and the Cairn voice apply without change:
calm adult, short sentences, small words, sentence case, never an
exclamation mark as a reward, never blame.

Specific to the terminal:

- **Every wrong turn names the next thing to type.**
  `I can't find "drw". Did you mean draw?`
- **Did-you-mean is deterministic.** One edit for words of three letters or
  fewer, two for longer ones, ties to the earlier word. `rm` at L1 gets no
  suggestion, because guessing `ls` would be wrong.
- **Success is mostly quiet.** `cd make` says nothing; the prompt changes.
  `open draw` says `Opening Draw.` once.
- **The prompt is the location.** `/`, `/make`, `/home/sam/notes`. Nothing
  else on it.
- **Every string is translatable from the first commit** (ADR-0007) and the
  complete wording table lives in the repository beside the tests that pin
  it.

The current table is in `shell/README.md`.

---

## 6. The surface

- Atkinson Hyperlegible Mono at the terminal size, Sand on Ink, from the
  brand tokens. 18px minimum. Generous line height.
- **Ghost completion.** As the child types, the rest of the most likely
  word shows in Sky after the cursor; Tab or Right accepts it. When several
  words fit, they are listed under the line, and the list narrows with each
  letter. A child typing one letter every five seconds sees help arrive,
  not vanish.
- **Icons on `ls`.** Every line carries its kind, so a door shows the kind's
  colour and mark, a folder shows a folder, a note shows a note, and a
  drawing in `pictures` shows the drawing. A pre-reader can `ls` and `open` by
  picture alone.
- **Up arrow recalls.** History is the last twenty lines of this session,
  nothing more.
- **Nothing scrolls away.** Output is short by design; the surface keeps the
  last screenful and no more.
- **One way out, three ways to say it.** `exit`, Escape, or the host's Back
  control, and all three return to the launcher. `exit` is an L1 word
  because leaving is the first thing a child should be able to do alone.
- Every control has an `Accessible.name` (ADR-0008); the surface is
  keyboard-first and mouse-optional.

---

## 7. Architecture

C++20 with Qt 6 and QML, as ADR-0002 says for every first-party surface.
Three parts, each testable on its own:

1. **The interpreter.** Qt Core only. Takes a line, returns a reply: lines
   with kinds, an optional launch, a leave, a new location. Holds the level
   and the world. Never writes a file or starts a process.
2. **The world.** Three read-only providers behind one interface. The
   *doors* provider is handed a list of titles, kinds and exec lists by the
   host. The *home* provider loads the fictional layout from one JSON file,
   the project's own or one the host names. The *pictures* provider, from
   L2, is handed a real directory by the host and only ever reads it.
3. **The surface.** A QML component the host places in its window. It draws
   replies, handles input, completion and history, and emits `launch` and
   `leave`.

The host interface is deliberately small: give the terminal its doors, its
level, the child's name and (at L2) the pictures directory; receive `launch`
requests and a `leave` signal. Cairn's launcher is the first host and does
those through the manifest reader and `AppLauncher` it already has. A
standalone window with a demo world is the second host and lives in this
project.

The interpreter, the world with its built-in `home`, the reply type and
the surface exist today in Cairn's `shell/` and `launcher/`, with the six
L1 words. They move with the split.

### 7.1 What stays in Cairn

The manifest reader, the launcher's hosting code, the `/usr/bin/open`
wrapper for L3, and the page bash shows on the first day. Level, accounts
and reset stay the OS's business; the terminal only receives the level.

### 7.2 Testing

The interpreter and the world get the most tests and the plainest code,
because they are the trust boundary. Every command, every sentence, every
level's word list, and the refusal cases: paths, program names, flags,
shell syntax, anything outside the world. The surface is tested with Qt
Quick Test the way the launcher's is.

---

## 8. Safety

- The interpreter writes nothing, anywhere, ever. It reads one layout file
  and, at L2, the one directory the host hands it.
- No network, ever.
- No child process is started by this project. `open` is a request to the
  host, which starts things from its own allowlist.
- What `ls` shows is the whole world; there is no hidden name to guess.
- A missing or broken layout file means the built-in one, not an error.

---

## 9. Open questions

| # | Question | Notes |
|---|---|---|
| 1 | The name | ROADMAP D9. A package and project name, not something the child types. Decides the repository. |
| 2 | The exact L2 twelve | Table in §4 is the candidate. Child-tested. |
| 3 | Can a grown-up add to `home`? | A note from a parent in `notes` would be a reason to `cat` something new. Cheap if the layout file is editable by the Guardian tool; decide after the first child test. |
| 4 | Discoveries | Small things to find (`help` growing, a note that mentions another) could make exploring richer. Or they are clutter. Not in v1. |
| 5 | The demo world | What the standalone window shows a child who is not on Cairn: doors that open nothing real need honest wording. |
| 6 | The child's name in the tree | `home/<name>` needs the name from the host; the demo world needs a default. |
