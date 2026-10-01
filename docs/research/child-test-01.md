# Child test 1: two cousins on a laptop (P0-9)

The Phase 0 exit test (ROADMAP P0-9): real hardware, real children, about
twenty minutes each, and an observer who takes notes and does not step in
unless asked. First run planned for 2026-10-02 with two cousins aged 9 and
10, one account at L1 and one at L2. The pilot in the VM (#8) found #77 to
#83; this is the first run on a laptop.

This page is the plan and the notes sheet. Results go under "What
happened" after the test.

## Before the day

- **Ask the cousins' parents** whether their child may try it, and say what
  the notes will hold. Notes name no one: "Cousin A (10, L2)". No photos,
  recordings or screenshots of a child.
- **Build** on the dev PC, after merging the evening's PRs:

  ```sh
  sudo dnf install wlr-protocols-devel libasan libubsan   # once
  git pull && cmake --preset release && cmake --build --preset release
  ```

- **Install Bazzite KDE** on the laptop from its USB image, connect it to
  Wi-Fi and to power, and update it (`ujust update`, then reboot).
- **Copy the checkout over.** Either `rsync -a --exclude .git --exclude
  build/debug ~/Code/cairn/ LAPTOP:cairn/` after `sudo systemctl enable
  --now sshd` on the laptop, or a USB stick. Keep ssh on if you would like
  help during setup.
- **Provision** at a terminal on the laptop, so the script can ask for the
  Guardian's password itself (hidden, typed twice), never in a variable:

  ```sh
  cd ~/cairn && sudo ./provision/cairn-provision.sh --guardian YOU \
      --child YOUNGER --child OLDER:2
  ```

  Then reboot and run the same command again, which switches the login
  screen to Cairn's; reboot again and run it once more, which should end
  `done: 0 change(s)`. The first run takes a while: it layers packages and
  downloads Tux Paint and GCompris.
- **Steam games (optional).** Log in as the Guardian (Plasma), sign in to
  Steam and install the games. ScummVM classics such as Putt-Putt, Freddi
  Fish, Pajama Sam and Spy Fox can become tiles that run the game files
  directly, for both children, with no Steam login in their sessions and
  no Steam overlay. Anything else runs through Steam and needs it signed
  in for that child. If a game runs through Steam, turn off the in-game
  overlay in Steam's settings (#74).
- **Set the brightness** from the Guardian's desktop: the kiosk does not
  bind the brightness keys. Leave the laptop **plugged in**: nothing in the
  kiosk warns about a low battery yet (#43).

Rehearsed on 2026-10-01 in the VM from stock Bazzite with the evening's
changes: three runs (47 changes, then 2, then 0), the login screen showing
both children and the Guardian, each child in with one press, Draw opening
Tux Paint, Music saying it is coming soon, Ctrl-Alt-Home and Log out
working.

## What the children have

- Tiles: Draw (Tux Paint), Practice (GCompris), the Terminal, Music, Build
  and Story, which say "coming soon", and any games added.
- **Super+Q** leaves an app. **Log out** is at the top right.
- The volume keys work, up to 100%. L2 has the same tiles and Terminal as
  L1 for now; its extras are not built yet.

## Ways out for the grown-up

- **Ctrl-Alt-Home** ends whatever is stuck and brings the tiles back.
- **Hold the power button for five seconds** to power off in order; a tap
  does nothing.
- What went wrong is in the journal, readable from the Guardian's account:

  ```sh
  journalctl -t cairn-launcher -t cairn-kiosk --since today
  ```

## What to watch

Note the time against each, so the journal can be matched up later.

- What they try first, and how long until something they wanted opened.
- Where they get stuck, what they say, and whether they ask for help.
- Whether they find the way out of an app (Super+Q) and Log out on their
  own.
- What they do with the Terminal, and what they type.
- What they make of "coming soon", and of any grown-up screen.
- The volume keys; the trackpad, including tap to click; anything about the
  laptop itself.
- Anything that looks slow, broken or confusing, and any time a grown-up
  had to step in.

Afterwards, three questions each: what did you like, what was confusing,
and what would you want it to do?

## Notes

| Time | Who | What happened | Said |
|---|---|---|---|
| | | | |

## What happened

Not run yet.
