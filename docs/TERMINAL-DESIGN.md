# The terminal — Design Document

**Status:** Draft for discussion, 2026-09-05
**Intended home:** its own repository once ROADMAP D9 names it; until then
this file lives beside `DESIGN.md` and is argued with here
**Depends on:** `DESIGN.md` §3.1 (the premise), ADR-0002 (C++/Qt/QML),
ADR-0013 (what the L1 shell sees)

A shell that grows with the child. Five words at five years old, about
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
same answer, every time. The failure is free: nothing outside the terminal's
own world exists to break.

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
2. **One world, two doors.** The terminal shows two kinds of thing: what the
   child can open, and what the child owns. Nothing else.
3. **Consequence-free experimentation.** A child can make, rename and
   delete things freely, because the things are theirs and pretend, and a
   reset costs nothing.
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
make   practice   machine   mine
```

The first three are **doors**: the programs the child can open, grouped by
kind as the launcher groups them (ADR-0013). They come from the host, and
the terminal cannot change them.

`mine` is the **pretend computer**: a tree the child owns outright. It lives
in the terminal's own memory and is saved as one small file wherever the
host says. Every command that makes or changes something works only here.
A child can fill it, wreck it and empty it, and `reset` (a grown-up action
in the host, not a command) puts back the starter set.

At L2 a third kind appears beside `mine`: `files`, the child's real
creations directory (`DESIGN.md` §11), read-only. `ls files` shows the
drawings with their pictures; `open` on one shows it. The terminal never
writes there.

### 3.1 Names

Everything has a lowercase name with no spaces: `draw`, `tux-paint`,
`my-story`. Titles from the host become names by the slug rule in
ADR-0013. Names the child makes are taken as typed, lowercased, with spaces
turned into dashes, so `mkdir My Stuff` makes `my-stuff` and says so.

### 3.2 Starter set

`mine` starts with three things, so `cat` has a purpose at L1 and `ls` is
never empty: a note that says hello, a note that lists the five words, and
an empty folder called `things`. The wording is part of the vocabulary
write-up and gets child-tested like everything else.

---

## 4. The vocabulary ladder

| Level | Words | What is new |
|---|---|---|
| L1 (5–6) | `ls` `cd` `open` `cat` `help` | looking, going, opening, reading |
| L2 (7–8) | L1 plus `pwd` `mkdir` `touch` `echo` `mv` `cp` `rm` | making and changing things in `mine`; `files` appears |
| L3 (9–11) | a real shell | the same words in bash, plus everything else |

Twelve at L2 is the design's number; the exact set is child-tested
(`DESIGN.md` §14 Q2). Candidates not in the table: `clear`, `history`,
`tree`.

### 4.1 Rules that hold at every level

- A command behaves as bash does for the forms it accepts. `cd ..` goes up,
  `cd` alone goes home, `ls name` looks into a folder, `echo hi > note`
  writes a note. Forms it does not accept get a suggestion, not a surprise.
- No flags at L1. `ls -la` is a name the terminal cannot find. Flags arrive
  when a level needs them, one at a time, and each is a real one.
- One name at a time at L1. L2 allows what its commands need (`mv a b`).
- `open` always means the host starts something. On a door it starts the
  program. On a file in `files` it shows the file. On a thing in `mine` it
  reads it, like `cat`, because a pretend thing has nothing else to open.
- `rm` in `mine` asks nothing and deletes. The pretend computer is the
  place to learn that `rm` means gone; the reset is the safety net, and it
  is the grown-up's, not the child's.
- Nothing the child types ever reaches the real filesystem, the network or
  another program, except through `open`, which the host carries out from
  its own allowlist.

### 4.2 The hand-off at L3

At L3 the child gets bash in a real terminal emulator. Three things make
that a step and not a cliff:

- Every word they know works, because they were real. `open` works through
  a small `/usr/bin/open` wrapper the OS packages (issue #46).
- `mine` becomes real: on the day the level changes, the pretend tree is
  written out as an actual folder in the child's home, with the same names.
  What they built stays built.
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
- **The prompt is the location.** `/`, `/make`, `/mine/things`. Nothing
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
  drawing in `files` shows the drawing. A pre-reader can `ls` and `open` by
  picture alone.
- **Up arrow recalls.** History is the last twenty lines of this session,
  nothing more.
- **Nothing scrolls away.** Output is short by design; the surface keeps the
  last screenful and no more.
- **One way out.** Escape, or the host's Back control, returns to the
  launcher. There is no `exit` at L1; it arrives at L2, because it is a
  real word.
- Every control has an `Accessible.name` (ADR-0008); the surface is
  keyboard-first and mouse-optional.

---

## 7. Architecture

C++20 with Qt 6 and QML, as ADR-0002 says for every first-party surface.
Three parts, each testable on its own:

1. **The interpreter.** Qt Core only. Takes a line, returns a reply: lines
   with kinds, an optional launch, a new location. Holds the level and the
   world. Never touches a file or a process.
2. **The world.** Two providers behind one interface. The *doors* provider
   is handed a list of titles, kinds and exec lists by the host. The
   *pretend computer* is the interpreter's own tree, loaded from and saved
   to a single JSON file the host names. The *files* provider, from L2, is
   handed a real directory by the host and only ever reads it.
3. **The surface.** A QML component the host places in its window. It draws
   replies, handles input, completion and history, and emits `launch` and
   `leave`.

The host interface is deliberately small: give the terminal its doors, its
level, its save file and (at L2) its files directory; receive `launch`
requests and a `leave` signal. Cairn's launcher is the first host and does
those through the manifest reader and `AppLauncher` it already has. A
standalone window with a demo world is the second host and lives in this
project.

Slice 1 of the interpreter, the doors provider and the reply type exist
today in Cairn's `shell/`. They move with the split.

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

- The interpreter has no filesystem access except the one save file the
  host names and the read-only directory it is handed at L2.
- No network, ever.
- No child process is started by this project. `open` is a request to the
  host, which starts things from its own allowlist.
- What `ls` shows is the whole world; there is no hidden name to guess.
- The save file is the child's own data and is disposable: a corrupt or
  missing file means the starter set, not an error.

---

## 9. Open questions

| # | Question | Notes |
|---|---|---|
| 1 | The name | ROADMAP D9. A package and project name, not something the child types. Decides the repository. |
| 2 | The exact L2 twelve | Table in §4 is the candidate. Child-tested. |
| 3 | `rm` with no net | §4.1 says delete means gone in `mine`. A `trash` folder would be more forgiving but is not a real word. Decide after the first child test. |
| 4 | Discoveries | Small things to find (`help` growing, a note that mentions another) could make play richer. Or they are clutter. Not in v1. |
| 5 | Save format | One JSON file per child. Where the host keeps it and what a reset restores. |
| 6 | The demo world | What the standalone window shows a child who is not on Cairn: doors that open nothing real need honest wording. |
| 7 | Sharing `mine` | Two children on one machine each get their own; whether they can see each other's is the OS's question. |
