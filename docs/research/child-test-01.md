# Child test 1: children on a laptop (P0-9)

The Phase 0 exit test (ROADMAP P0-9): real hardware, real children, about
twenty minutes each, and an observer who takes notes and does not step in
unless asked. First run planned for 2026-10-02 with two cousins aged 9 and
10, one account at L1 and one at L2. The pilot in the VM (#8) found #77 to
#83; this is the first run on a laptop.

The cousins' run never happened: they left before it could start. Two
children in the maintainer's house used the kids' laptop instead on
2026-10-10, and the younger one again on 2026-10-11. Phase 0 ends on those
evenings (ADR-0031).
The plan below is kept as it was written.

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

### 2026-10-10: two children at home, no internet

Not the planned run with the cousins: two children in the house, both at
L1, on the kids' laptop, while the maintainer watched the journal over ssh.
A power cut had taken the house's internet down, so Steam could not reach
its servers all evening. The launcher was the #136 build (the starting
screen) on top of main at `de8d2cf`. Times are from the journal; what the
children said comes from the grown-up who was with them.

| Time | Who | What happened | Said |
|---|---|---|---|
| 17:29 | Child A (L1) | Logged in; the launcher was up 10 s later. Opened Draw (Tux Paint) within a second and drew for about a minute and a half. | |
| 17:31 | Child A | Pressed Music and Build: "coming soon". | |
| 17:31 | Child A | Putt-Putt Goes to the Moon started and ran for 3 minutes, the only Steam game that started all evening. | |
| 17:34–17:41 | Child A | Five more Steam games, Balloon-o-Rama three times in 46 s: none started (#139). Draw again at 17:36, Practice (GCompris) at 17:40 without its voices (#141). Music, Build and Story again. | Could not tell which game was which, since she cannot read the titles (#138). |
| 17:41 | Child A | Logged out; two runs of the log-out script started (#140). | |
| 17:42–18:02 | Child B (L1) | Logged in, opened Draw, then five Steam games from 18:00 to 18:02, Big Thinkers 1st Grade twice: none started. Logged out; four runs of the log-out script. | |
| 18:03–18:45 | Child B | Back in, Draw, Story. The lid was closed and opened six times; each time the laptop slept and the session came back as it was. Music twice; logged out (seven runs of the script) and back in. | |
| 18:46–18:47 | Child B | Pep's Birthday Surprise and Travels Through Time: neither started. | |

### 2026-10-11: the younger child again, online

Child A (L1) alone for about 45 minutes, with the internet back. The
launcher was a build of main at `796ff34`: the starting screen, and the
longer wait for a Steam game (ADR-0029). Times are from the journal.

| Time | Who | What happened | Said |
|---|---|---|---|
| 18:55 | Child A | Logged in and pressed Big Thinkers Kindergarten within seconds. The game started 9 s after the press, with the Steam client still new from login. Played 12½ minutes. | |
| 19:08 | Child A | Pressed the same game 21 s after it ended; up in 2 s. Played 16 minutes. | |
| 19:17 | Child A | The lid was closed for 27 s; the laptop slept, and the game was where she left it. | |
| 19:24 | Child A | Putt-Putt and Fatty Bear's Activity Pack, 3 s after Big Thinkers ended. Played 11½ minutes. | |
| 19:36 | Child A | The same game again, 9 s after it ended; still running at 19:42. | |

## What happened

What 2026-10-10 showed:

- **Pictures matter more than anything else on the tiles.** A child who
  cannot read cannot tell one Putt-Putt from another by its title. DESIGN
  §6 asks for icons and §14 Q3 is still open; #138.
- **Offline, Steam stops almost every game, and the child cannot see why.**
  Steam could not check each game's cloud saves and waited on its "Cloud
  sync failed" dialog, which the kiosk hides. Twelve of thirteen Steam presses
  started nothing; the launcher showed the starting screen for 15 s and then
  the tiles, with nothing said, so the children pressed again. #139.
- **Log out looks like it does nothing** while it waits up to 10 s for
  Steam, so children press it again; #140.
- **GCompris has no voices offline:** it downloads them the first time it
  runs; #141.
- **Closing the lid** sleeps the laptop and keeps the child's session; no
  child was logged out by it.
- The children enjoyed the games that did start. The younger child is to be
  asked more later: whether the game's own picture would tell her which game
  it is, and whether she wants tile names read aloud.

On 2026-10-11, the younger child alone and online:

- **Online, with Steam started at login, games start reliably:** the
  first in 9 s, the rest in about 2 s. Four presses, four games, no
  grown-up screen and no window that opened on its own.
- **She stays with a game for 11 to 16 minutes**, and went straight back
  into the same game twice within seconds of it ending. Whether she quit
  on purpose or something ended the game unexpectedly (Super+Q, a ScummVM
  menu, the game's own Quit) is not in the logs.
- **Steam's in-game overlay was running** for her games, so Shift+Tab would
  most likely have opened Steam's friends, chat and community inside the
  game; #74. Until Cairn handles it, turn the overlay off in each child's
  Steam settings.

## Against the exit criterion

ROADMAP §2's rows, and what the two evenings showed. Phase 0 ends on these
evenings (ADR-0031); what was not seen moves to P1-17 and P1-1.

| Row | Seen? | Where |
|---|---|---|
| Logs in from an avatar tile | Yes | Both children on 2026-10-10; the younger child on 2026-10-11. |
| Launches and returns from at least two apps, one through Steam | Yes, as far as the journal shows | 2026-10-11: two Steam games, each back to the tiles. 2026-10-10: Draw, Practice and one Steam game. Whether the child left each app on purpose is in neither evening's notes; the pilot child could not (#77). |
| Uses the shell to open something | No | Not in either evening's notes. The pilot found it too advanced for a child who cannot read yet (#83). |
| Hits an error and recovers without help | Partly | 2026-10-10: twelve of thirteen Steam presses started nothing, and the children went on to other tiles. The launcher said nothing then; the grown-up screen for it (ADR-0030) came after. |
| Never leaves the kiosk | Yes, as far as the journal shows | No Plasma, no Steam window. Steam's overlay was running on 2026-10-11 and could likely have opened Steam's friends and chat (#74). |
| One child aged 5–8 | Not recorded | The notes give no ages. |
| One child, twenty minutes, unsupervised | Yes | 2026-10-11: the younger child alone for about 45 minutes. |
| On a Minimum-tier laptop | No | The kids' laptop has 14 GB; the 4 GB floor is P1-1. |
