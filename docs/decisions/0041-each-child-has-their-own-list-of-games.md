# ADR-0041: Each child has their own list of games

**Status:** proposed. It becomes accepted when the checks in step 6
below have run in the cairn-iso VM and none of them overturns a bullet
of the Decision.
**Date:** 2026-10-27
**Closes:** (none) — the Games page of issue #167; part of ROADMAP P1-5.
Once accepted, amends ADR-0033, ADR-0032, ADR-0035, ADR-0038, ADR-0039
and ADR-0040.

## Context

ADR-0033 gives every child a copy of every game in the shared folder, and
a tile for each.
Its safeguard is a rule: only games every child may play go in that
folder.
Children of different ages need different games, so that rule does not
hold for long in a family with a 5-year-old and a 10-year-old.

DESIGN §3.2 already lists a per-child game allowlist among a Guardian's
permissions, and ADR-0004 gates each Steam tile on it.
Nothing builds it yet.
ADR-0032 hoped Steam Family's allow-list would decide a child's tiles,
"with no separate tick", but Cairn cannot read it from the child's Steam,
and it does not cover a game a family shares through one Steam account.

Today the launcher reads one file for every child,
`/var/lib/cairn/games.json`.
The Guardian tool (ADR-0040) cannot give games at all; its Children page
says the family set-up script does.

The maintainer settled the shape on 2026-10-27.

## Decision

**Each child has their own list of games. A Guardian chooses it on the
Guardian tool's Games page, and a child gets copies and tiles only for the
games on their list.**

### The Games page

- The parent picks a child and sees a grid of game pictures, one for each
  game in the shared folder.
- They highlight the games that child may play and press Save.
- Save asks for one password (pkexec, `auth_admin`), which covers
  everything that Save changes.
- Save also rescans the shared folder and remakes the full games list and
  the pictures from the Guardian's own Steam, as provisioning does now.
  So a game the Guardian has just installed shows without a separate
  step.
- With no change to save, **Look for new games** runs the same Save
  with no child named, which changes no list and only rescans.
  So no list the window read, which may be out of date or unreadable,
  is ever sent back to be written. A family whose shared folder had no games yet has
  nothing to highlight, so without it no Save could run.

### The root side

- Saving is a new subcommand of the existing root command, `cairn-family
  save-games`, so ADR-0039's "one root command" still holds.
  It gets its own answer lines and refusal words, below.
- Each subcommand has a polkit action of its own, through polkit's
  `org.freedesktop.policykit.exec.argv1` annotation, which pkexec
  supports (polkit 127): `com.cairnlinux.family.create-child`,
  `.set-level` and `.save-games`, all on `cairn-family`'s path and all
  `auth_admin`.
  A Save asks "A grown-up's password is needed to change which games a
  child has."; the other two keep ADR-0039's message.
  Every action names its first argument, so pkexec never picks one by the
  path alone, whatever order polkit lists them in.
  Any other first argument falls back to pkexec's own action, which also
  asks for an administrator, and `cairn-family` refuses it.
- `cairn-family save-games [LOGIN=APPID,APPID...]...` takes each
  named child's whole new list, never a change to it: `ben=` means no
  games, and a child not named is left alone.
  With no child named it only remakes the pictures and the full games
  list: the window's Look for new games.
  It never passes `--all`, so a child can never get every game this way.
- Before anything changes it checks the caller (root, and through pkexec
  an administrator), every value, and that every name is a child Cairn
  made.
  Then it remakes the pictures from the Guardian's own Steam, whose home
  it takes from the account database, never from `$HOME`.
  Under sudo there is no Guardian to read them from, and the pictures
  stay as they are.
  Then it remakes `/var/lib/cairn/games.json` with kidscan, byte for byte
  as provisioning does, keeping the old file when kidscan fails.
- Each child's list is `/var/lib/cairn/games/<login>.json`, root's, mode
  0644, in a folder systemd-tmpfiles makes at boot, root's and 0755.
  It is the launcher's games file, and holds only games still ticked that
  the child already had, and games newly ticked that the copy reported as
  copied or already there.
  It is written at every Save, so a game taken off goes at once, even
  when the child's copy is held.
  Before any child's copy starts, every child's list is written with the
  games taken off gone and nothing added, and each is written again after
  that child's copy with the games it gave.
  So a Save cut off mid-copy, by a logout or a reboot, still leaves no
  child a game the parent took off.
- The copy itself runs as before: as the child, through
  `cairn-steam-visit --copy-games CHILD APPID...`, under the visit lock,
  which keeps the child from logging in while it runs.
  `cairn-copy-games --only APPID...` copies only the listed games.
  Every shared game takes its own word,
  `cairn-steam-visit --copy-games CHILD --all`, which provisioning
  passes.
  An empty list copies nothing, so a child with no games never gets
  every game by a caller passing an empty list.
- For "Log them out now" (step 5),
  `cairn-steam-visit --copy-games --log-out CHILD APPID...` logs the
  child out first, and only when they are logged in.
  It does so only once the child is listed, so PAM refuses them from the
  logout to the end of the copy.
  It says `logging-out`, asks the child's Steam to save and quit when it
  is running, ends the child's account with `loginctl terminate-user`,
  kills what is left after 15 seconds, and gives a closing login 5
  seconds to go.
  Then it says `logged-out` and copies as above.
  `refused stuck` means a program of the child's would not end, so
  nothing was copied and the computer needs a restart; `refused
  logged-in` means the ended login was still closing; and `refused
  stopped` that a signal came mid-logout.
  Every ending, `stuck` among them, lets the child log in again.
  The pkexec guard comes first, so the visit's own action can never log
  a child out.
  `--log-out` goes straight after `--copy-games`, once, and never with
  `--all`, or the helper refuses `usage`.
  An empty list still copies nothing and logs nobody out.
  A visit's close logs the child out with the same steps, with one
  difference: a stuck visit keeps the child refused at login until a
  restart, while a stuck copy does not.
- A listed game that is not in the shared folder is reported as
  `skipped APPID not-shared`, and the rest go ahead.
  An appid that is not a plain number is refused before anything
  changes.
- The copy runs as a system job, `systemd-run --wait --pipe` in a
  transient service of its own, outside the Guardian's session.
  `cairn-family` waits for it with no time limit and never kills it.
  A Guardian who logs out mid-copy ends `cairn-family`, not the copy,
  which then lets the child log in again.
  The visit helper's pkexec guard stays as it is: pkexec always sets
  `PKEXEC_UID`, and a system job starts with the service manager's
  environment, so the visit's own action can never start a copy, and the
  one route from a password box is `cairn-family save-games`.
  The guard's comment names that route.
  Nothing admits `cairn-family` by name, because only a root program
  could set or fake whatever would.

### What `cairn-family save-games` answers

Lines of fixed words, checked login names and appids, as it goes, never
a Steam name or a path:

| Line | Meaning |
|---|---|
| `looking` | The first line, before the pictures and the full games list are made: the password was accepted and the work starts |
| `pictures made` or `pictures kept` | The picture step ran to its end, or the pictures stay as they were |
| `catalogue made` or `catalogue kept` | The full games list is new, or the old one stays |
| `copying LOGIN` | The child's copy starts |
| `waiting LOGIN` | The child's last login is still ending |
| `copied LOGIN APPID`, `have LOGIN APPID` | The child has the game now |
| `skipped LOGIN APPID REASON` | The game is left out, for REASON: `not-shared`, or `no-catalogue` when no full games list could be read, so whether it is shared is not known, or one of the copy step's own reasons, passed on as it says them: `unreadable`, `appid`, `installdir`, `beta-branch`, `not-installed`, `downloading`, `no-folder`, `folder-without-manifest`, `changed`, `copy-failed` or `unreadable-files` (`provision/cairn-copy-games.py` says what each means) |
| `child LOGIN added N removed N [held WORD]` | The child's list is written, and the counts say what it holds now; `held` says why their new games are not all given |
| `later LOGIN` | After the child's line: their new games are written down, and are copied the next time they log out |
| `done` | The last line of a Save |
| `refused WORD` | Nothing changed |

`held` is one of `logged-in`, `busy` (a visit), `copying`, `still-running`,
`stopped`, `not-set-up`, `no-library`, `no-shared`, `lists-shared`,
`unreadable-config`, `no-reflink`, `failed`, `not-started` (the job could
not start) or `not-written` (the child's list could not be written
after the copy, so it holds none of the new games: only the games taken
off are gone, when that first write worked, and the counts say so).
A list that is in place but could not be labelled again for SELinux
counts as written.
A child's problem is never a refusal of the whole Save; the next child
goes ahead.

`refused` is one of ADR-0039's `not-root`, `not-a-grown-up`, `usage`,
`bad-name` and `not-a-child`, or one of two new words: `bad-appid`, an
appid that is not a plain number, and `busy`, another Save still
running.
`cairn-family finish-games`, below, answers `done`, or `refused` with
`not-root`, `usage` or a third new word, `not-for-pkexec`.

### A child who is logged in

The copy cannot run while the child is logged in, as today.
The games taken off go at once, and the new games are copied by
themselves after the child's next logout, with nothing more for the
parent to do.
The maintainer settled how on 2026-10-28.

- **The waiting list.** Before anything changes for a child with games
  to copy, a Save writes their whole new list to
  `/var/lib/cairn/games-waiting/<login>`, one line, exactly that child's
  argument, such as `ada=570,620`.
  It is root's, mode 0644, in a folder systemd-tmpfiles makes, root's and
  0755.
  It is written first so that nothing the parent asked for is lost if
  the window closes, the parent logs out or the power goes mid-Save.
  A child with nothing to copy has no file, and an older one goes.
  A Save that names no child, a refused Save, and a Save that does not
  name the child leave it alone.
- **Which endings keep it.** A child held as `logged-in`,
  `still-running`, `stopped` or `not-started` keeps waiting, because a
  logout settles each; the Save says `later LOGIN` after their line.
  Any other ending, a clean one included, removes it, and `busy`,
  `copying`, `failed`, `not-set-up` and `not-written` keep their "press
  Save again" sentences.
  When the waiting list could not be written, there is no `later`, and
  the page says to press Save again, as before.
- **The copy at the logout.** A drop-in on systemd's own
  `user-runtime-dir@.service` starts `cairn-finish-games@UID.service`
  when it stops, about ten seconds after the account's last session ends,
  closing ones included:
  `ExecStopPost=-/usr/bin/systemctl start --no-block
  cairn-finish-games@%i.service`.
  The `-` means a failure never breaks a logout, and `--no-block` means
  the copy never holds up the stop.
  At shutdown that start conflicts with `shutdown.target` and is dropped.
  The unit has `ConditionDirectoryNotEmpty=/var/lib/cairn/games-waiting`,
  so no logout starts anything while no child is waiting, and no
  `Restart=`.
  There is no boot unit: a copy at boot would surprise a child at their
  first login.
- **`cairn-family finish-games UID`.** The unit runs it as root.
  It has no polkit action, and refuses `not-for-pkexec` when `PKEXEC_UID`
  is set, `not-root`, and `usage` for a uid that is not digits.
  An account that is not a Cairn child, or a child with no waiting list,
  is nothing to do, and it exits silently.
  It takes the Save lock, waiting for a Save that is running, then reads
  the waiting list again: one that is gone was settled meanwhile.
  The file must be a regular file of root's under 16 KiB in a folder only
  root may write, opened without following a link; its one line must be
  a list `save-games` takes, naming this child; and the account must be a
  child Cairn made.
  A file that fails a check is removed, noted in the journal and never
  copied.
  The copy is a Save's for one child, from `games.json` as it is, with no
  pictures made, through the same system job and visit lock: the games
  not on the list go, the new ones are copied, and the child's list is
  written.
  An ending in the keep set leaves the waiting list for the next logout.
  Any other writes the child's lines to
  `/var/lib/cairn/games-finished/<login>` and removes the waiting list.
  Its notes go to the journal as `cairn-family`, such as "at ada's
  logout: child ada added 2 removed 0".
- **The child.** A child who logs in during that copy is refused by the
  visit lock, and the login screen says "A grown-up is setting something
  up for you. Try again soon.", as for any copy.
  The child sees no new text.
- **The Games page.** After a Save with `later`, the child's sentence
  says why their games wait, then "Ada's new games are copied the next
  time Ada logs out. You do not need to do anything else. The games taken
  off are already gone from Ada's list."
  The Save's first sentence no longer says that each child sees their
  games at their next login whenever a child was held or had a game left
  out.
  A child's waiting list is the page's starting point for them: their
  waiting games are marked "Waits for Ada to log out", Save stays off
  until the parent changes something, and "Put back as saved" returns to
  it.
  A note says "Ada's new games are copied the next time Ada logs out."
  Once the copy at the logout has run, a clean one reads "Ada's new games
  were copied when Ada logged out on" its date; any other says what
  stopped it in the Save's own sentences, and that those games are not on
  the list now.
  That note stays until the next Save for that child, which removes the
  finished file.
  The page reads both folders without root, with the same checks, and
  watches them, so an open page follows a copy at a logout.
- **A new child.** `cairn-family create-child` removes any
  `games/<login>.json`, waiting list and finished file before it makes a
  new account, and refuses `failed` if one stays, so a new child never
  inherits an old account's games.

### Starting, adding and taking away

- A new child starts with no games, so no game tiles and no Steam at
  login, until a parent saves their list.
- Taking a game off a child's list removes its tile at their next login.
- The child's copy and saves stay in their home: the copy step never
  deletes a child's copy.
  Putting the game back on their list brings back their progress.

### Where it applies

The image only.
The kids' laptop, a stock Bazzite provisioned by hand, keeps
`cairn-provision.sh`'s copy of every game to every child.
It has no `cairn-family` and no Guardian tool (ADR-0039, ADR-0040), and
its launcher wrapper is provisioning's own.
It moves to per-child lists when it is reinstalled from the image.

### Steps

1. **Copies follow a list.**
   `cairn-copy-games --only` and `cairn-steam-visit --copy-games CHILD
   [APPID...]`, where no appids copy nothing and `--all` copies every
   game, with tests.
   The change at the trust boundary lands alone.
2. **`cairn-family save-games`, the root side.**
   The visit helper's pkexec guard stays, and its comment names the one
   route past it.
   The copy runs as a system job, so a Guardian who logs out mid-copy
   cannot kill it and leave the child locked out by the visit lock.
   Landed 2026-10-27, with no window yet.
3. **A switch between pages, and a read-only Games page.**
   Landed 2026-10-28: Children and Games above the page, and a Games
   page that shows, for the child picked, every game in the full games
   list with the games on the child's own list highlighted, read with
   the launcher's rules, then any game on that list the shared folder
   no longer has, and "Open Steam for <child>", which starts
   `cairn-steam-for` as the menu entries do.
   It reads both files when it is shown and changes nothing.
4. **Highlight and Save.**
   In the same change: the launcher's wrapper reads
   `/var/lib/cairn/games/<login>.json`, and the family's setup stops
   copying every game on the image.
   Per-child tiles and the only way to fill them ship together.
   Landed 2026-10-28: a tap, or Space or Enter, puts a game on the
   picked child's list or takes it off, for as many children as the
   parent likes, with each change marked until Save, and "Put back as
   saved" for the child picked.
   Save runs `cairn-family save-games` through pkexec once, with each
   changed child's whole new list, and reads its lines as they come, so
   the page says whose games are being copied, then one plain sentence
   for each child's result, held word and game left out, or why nothing
   was saved.
   The wrapper takes the login from `id -un` and names the file only for
   a login that follows `cairn-family`'s rule; a child with no file has
   no game tiles and no Steam at login, as the launcher already did for a
   missing games file.
   A logged-in child's games wait for a second Save, as above.
5. **"Later" and "Log them out now"** for a child who is logged in, in
   three small steps.
   - **5a, "Later".** The waiting list written at every Save, `later
     LOGIN`, `cairn-family finish-games`, the unit and its drop-in on
     `user-runtime-dir@.service`, the tmpfiles lines, create-child's
     clean-up, and the page's waiting and finished notes, all as above.
     Landed 2026-10-28.
   - **5b, the visit helper's `--log-out`.** The change at the trust
     boundary lands alone: `cairn-steam-visit --copy-games --log-out
     CHILD APPID...` asks Steam to save, logs the child out, waits until
     their sessions have gone, then copies, refusing `stuck` when a
     program of theirs will not close.
     It is shaped to be driven by `save-games`: CopyJob passes the flag
     for a child the Save names to log out, and never otherwise.
   - **5c, "Log them out now".** One password, with the choice before
     Save.
     Before it runs `pkexec`, the window checks without root whether each
     changed child is logged in, and asks "Log Ada out now" or "Later"
     first.
     It then runs one `cairn-family save-games` through `pkexec`, which
     also says which children to log out, so one password box, with the
     games message, covers the whole Save.
     Its form, the window's confirmation that unsaved progress in a game
     may be lost, and the `stuck` sentence are settled there.
6. **Checks in the cairn-iso VM**, with two children at different
   levels: one password per Save, with the games message, while adding a
   child still shows the account message, each child sees only their own
   tiles,
   a Save while a child is logged in, a Guardian logout mid-copy, and a
   game taken off and put back with its saves.
   For 5a: the drop-in fires on Bazzite's systemd under SELinux;
   `finish-games` can write `/var/lib/cairn` and run `restorecon` from a
   system unit; a shutdown with a child waiting does not hang; a logout
   leads to the new tiles at the next login; a child who logs back in
   within ten seconds keeps waiting; `virsh destroy` mid-finish, then the
   child can log in and the next logout finishes; and whether
   `user-runtime-dir@` stops while a closing session is stuck.

## Consequences

- A younger child no longer sees an older child's games.
- The per-child game allowlist that DESIGN §3.2 and ADR-0004 ask for
  exists.
- **ADR-0033** is amended: a child gets copies of the games on their
  list, not of every game.
  Its rule that only games every child may play go in the shared folder
  is dropped on the image.
- **ADR-0032** is amended: its "no separate tick" is replaced by the
  parent's list.
  Its planned ADR that would let Steam Family's allow-list take the
  place of ADR-0004's per-child game allowlist is withdrawn: the
  parent's list is that allowlist, and ADR-0004 and DESIGN §3.2 stand.
  Steam Family's allow-list still applies on top when each child has
  their own Steam account.
  A listed game the child's Steam account cannot play ends on the
  grown-up screen (ADR-0030).
- **ADR-0035** is amended: pictures are also remade on every Save, from
  the window.
- **ADR-0038** is amended: the family's setup on the image makes no
  child's copy of the games; a Guardian's Save on the Games page does.
  The image gains its first systemd unit and a drop-in on an upstream
  unit, installed under `/usr/lib/systemd/system` with the build's path
  in them.
- **ADR-0039** is amended: `cairn-family` gains a subcommand, with its
  own answer lines and refusal words, and its one polkit action becomes
  one for each subcommand.
  `finish-games` is a subcommand with no polkit action, guarded like the
  visit helper's copy.
- **ADR-0040** is amended: the Games page goes beside Children, with a
  way to move between them.
- **Known limit: the list holds only at L1 and L2.** The shared folder is readable by
  every account, and children keep copies of games taken off their list.
  At L1 and L2 a child reaches games only through their tiles, so the
  list holds.
  Before levels 3 and 4 ship (#107), Cairn must keep a child from opening
  games off their list from the shared folder or from old copies.
- Every child the family's setup made before step 4 has a copy of every
  game.
  When step 4 lands they show no game tiles until a parent saves their
  list, and their copies stay.
- If `cairn-family` itself is ended after a copy starts, by a Guardian
  logging out, the copy finishes but the child's list is not written.
  The waiting list is already written, so the child's next logout finds
  the copy as `have` and writes the list, and the tile comes at the login
  after.
- A restart while a child is waiting means one more session on their old
  list: the copy runs at the first logout after it.
- A child who logs back in within about ten seconds of logging out keeps
  waiting, until a logout that stays a logout.
- A closing session that is stuck keeps `user-runtime-dir@` up, so the
  child keeps waiting until a restart.
- An account removed and made again with the same login outside
  `cairn-family`, as a child, inherits a waiting list.
  `create-child` clears the files; a uid in the file would bring back a
  second format, so the risk is accepted.
- If the copy is killed outright, by a power cut or the kernel running
  out of memory, the child cannot log in until a restart empties `/run`.
  Visits already carry this risk.
- With one shared Steam account, two children's saves can still meet
  through Steam Cloud.
- Dependencies: none new.
