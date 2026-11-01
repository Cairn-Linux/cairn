# ADR-0041: Each child has their own list of games

**Status:** proposed. It becomes accepted when the checks in step 6
below have run in the cairn-iso VM and none of them overturns a bullet
of the Decision.
**Date:** 2026-10-27
**Closes:** (none) — the Games page of issue #167; part of ROADMAP P1-5.
Once accepted, amends ADR-0033, ADR-0032, ADR-0035, ADR-0038, ADR-0039
and ADR-0040.
**Amended by:** ADR-0042, proposed (the Games page lists every game in
the Guardian's own Steam, and Save moves a picked game into the shared
folder before copying it, with new answer lines); ADR-0046, proposed (a
Save, and a Look, also fill a tools folder from the Guardian's Steam, and
the copy step copies Steam's tools to a child with their new games, with
new answer lines and sentences); ADR-0047, proposed (new answer lines,
picture-missing and picture-refused APPID, the journal gets each step's
own words, and the Save's window names each game without a picture)

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

### Games in the Guardian's own Steam

Added 2026-10-29.
A Guardian who installs a game the usual way puts it in their own Steam
library, in their home or on another drive that Steam lists in its
`libraryfolders.vdf`, not in the shared folder, so no child can have it.
Mason chose, for now, that the Games page says so (#167):

- The window, which runs as the Guardian, reads its own Steam's
  libraries: kidscan's four places for Steam in a home, and each library
  its `libraryfolders.vdf` lists, but for the shared folder and
  anything inside it.
  It reads each `appmanifest_*.acf` there, at most 1 MiB, never through
  a link, and passes over any it cannot read.
- It names each game it finds by Steam's own name, but for Steam's
  runtimes, redistributables and Proton, by kidscan's rule, and for the
  games already in the full games list.
  A game still downloading is named too: what matters is where it goes.
  The rule is written again in C++, and a test checks it against
  kidscan's.
- The page then says, in one paragraph, how to move them with Steam's
  own Settings, Storage: add `/var/lib/cairn/steam-library` as a drive,
  pick the games, Move, and optionally make it the default for future
  downloads.
- It reads them again whenever it reads everything else.
  It changes nothing, needs no password and asks root for nothing.

A later decision may replace this with a move as part of Save.

### The root side

- Saving is a new subcommand of the existing root command, `cairn-family
  save-games`, so ADR-0039's "one root command" still holds.
  It gets its own answer lines and refusal words, below.
- Each subcommand has a polkit action of its own, through polkit's
  `org.freedesktop.policykit.exec.argv1` annotation, which pkexec
  supports (polkit 127): `com.cairnlinux.family.create-child`,
  `.set-level`, `.save-games` and `.log-out`, all on `cairn-family`'s
  path and all `auth_admin`.
  A Save asks "A grown-up's password is needed to change which games a
  child has.", a logout "A grown-up's password is needed to log a child
  out.", and the other two keep ADR-0039's message.
  Every action names its first argument, so pkexec never picks one by the
  path alone, whatever order polkit lists them in.
  Any other first argument falls back to pkexec's own action, which also
  asks for an administrator, and `cairn-family` refuses it.
- `cairn-family save-games [LOGIN=APPID,APPID...]...`
  takes each named child's whole new list, never a change to it: `ben=`
  means no games, and a child not named is left alone.
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
- For the Games page's "Log Ada out" (below), `cairn-family log-out
  LOGIN` runs `cairn-steam-visit --log-out CHILD`, which only logs the
  child out and copies nothing.
  It lists the child first, so PAM refuses a new login of theirs until it
  ends.
  It writes its own marker first, as a copy does, so a visit, a copy or
  another logout meanwhile is refused as `logging-out`, not as `busy`,
  which the page would read as a visit to close; a logout tried during
  a visit or a copy is refused as `busy` or `copying`.
  It says `logging-out`, asks the child's Steam to save and quit when it
  is running, ends the child's account with `loginctl terminate-user`,
  kills what is left after 15 seconds, and gives a closing login 5
  seconds to go.
  Then it says `logged-out` and `finished`; a child who was not logged in
  is simply `logged-out`.
  `refused stuck` means a program of the child's would not end, so the
  computer needs a restart; `refused logged-in` means the ended login was
  still closing; and `refused stopped` that a signal came mid-logout.
  Every ending, `stuck` among them, lets the child log in again.
  The pkexec guard comes first, as for a copy, so the visit's own action
  can never log a child out.
  It takes exactly one child, or refuses `usage`.
  A visit's close logs the child out with the same steps, with one
  difference: a stuck visit keeps the child refused at login until a
  restart, while a stuck logout does not.
- A copy never logs anyone out: a copy for a child who is logged in is
  refused `logged-in`, as before.
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
  `cairn-family log-out` runs the helper's logout the same way, as
  `cairn-log-out-UID`.
- The copy job runs the helper inside `systemd-inhibit
  --what=sleep:handle-lid-switch --mode=block --who=Cairn --why="Copying
  games to a child"`, so closing the lid or the sleep timer does not
  suspend the computer halfway through a copy.
  Shutting down stays allowed.
  When logind cannot give the lock, the copy never starts, and is held
  `not-started`.

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
| `copied LOGIN APPID`, `have LOGIN APPID` | The child has the game now; the Save's progress window names each one |
| `skipped LOGIN APPID REASON` | The game is left out, for REASON: `not-shared`, or `no-catalogue` when no full games list could be read, so whether it is shared is not known, or one of the copy step's own reasons, passed on as it says them: `unreadable`, `appid`, `installdir`, `beta-branch`, `not-installed`, `downloading`, `no-folder`, `folder-without-manifest`, `changed`, `copy-failed` or `unreadable-files` (`provision/cairn-copy-games.py` says what each means) |
| `child LOGIN added N removed N [held WORD]` | The child's list is written, and the counts say what it holds now; `held` says why their new games are not all given |
| `done` | The last line of a Save |
| `refused WORD` | Nothing changed |

`held` is one of `logged-in`, `busy` (a visit), `copying`, `logging-out`
(a grown-up is logging a child out), `still-running`,
`stopped`, `not-set-up`, `no-library`, `no-shared`, `lists-shared`,
`unreadable-config`, `no-reflink`, `failed`, `not-started` (the job, or
its inhibitor lock, could not start) or
`not-written` (the child's list could not be written
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
### What `cairn-family log-out` answers

`logging-out LOGIN` when the helper starts to log the child out, then
`logged-out LOGIN` once they are out, with the login from its own
argument, never from the helper.
A logout that did not finish is one line `held LOGIN WORD`: `stuck`,
`logged-in`, `stopped`, `busy`, `copying`, `logging-out` or `not-set-up`
as the helper said it, `not-started` when the job never started, or
`failed`.
Then `done`.
`refused` is one of `not-root`, `not-a-grown-up`, `usage`, `bad-name` or
`not-a-child`, and means nothing was tried.

### A child who is logged in

The copy cannot run while the child is logged in, as today.
On the Games page, a logged-in child's games are greyed and blurred, and
cannot be changed, until they are logged out: one rule, for games taken
off as well as games added, so the parent never chooses games that could
not be given.
Mason decided this on 2026-10-29, replacing "Later" and "Log them out
now": the old flow was needlessly convoluted, and a parent who logged
out mid-copy lost their choice.

- **The page knows who is logged in.** The window asks systemd-logind on
  the system bus, without root, and counts a login by the visit helper's
  own rule: every session, a closing one too, but the service manager's
  and a Steam visit's.
  So a child counts exactly when their copy would be refused.
  It asks again when logind says a session came or went (`SessionNew`,
  `SessionRemoved`), when the page is shown, and after a logout or a
  Save.
  When logind cannot be asked, nobody counts as logged in: the journal
  says so, and a Save still refuses to copy for a child who is.
- **A logged-in child's games.** Over the child's greyed games a card
  says "Ada is still logged in. Log Ada out, then choose their games.",
  with **Log Ada out**.
  "Open Steam for Ada" waits too.
  The other children, Save for them, and Look for new games stay, unless
  a logged-in child has changes waiting (below).
  Where blur cannot be drawn, the fade alone says the games wait.
- **Log Ada out** warns first: "Log Ada out? Anything Ada has not saved
  in a game or program is lost. Steam is asked to save first.", with
  **Log Ada out** and **Not now**.
  Focus goes to **Not now**, so a second Enter never logs a child out.
  Then the password box, with the log-out message, and `cairn-family
  log-out ada` through pkexec, which only logs Ada out.
  While it runs the card says "Logging Ada out. Steam gets a moment to
  save first."
  Once logind says Ada's sessions have gone, the card goes and her games
  show again; the parent chooses them and presses Save as for any child.
- **When Ada is not logged out**, the card says why and offers Close.
  A program that would not close says "One of Ada's programs would not
  close, so Ada is not fully logged out. Restart the computer, then
  choose Ada's games."
  A closed password box, a visit or copy running, and the rest each have
  their own plain sentence.
- **A child who logs in while their changes wait** keeps the changes,
  but Save waits, for every child, until every child with changes is
  logged out, and the page says "Save waits: changes for a child who is
  logged in cannot be saved yet. Logged in: Ada. Log them out first."
  Put back waits too, as everything of a logged-in child's does: one
  rule, so logging the child out is the one way on.
- **A child who logs in between the page's look and the copy** is still
  refused safely: the Save writes the games taken off, the copy is held
  `logged-in`, the result says so, and the page then greys their games.
- **The Save's progress window.** Save opens a window over the whole
  Family window, so nothing else changes meanwhile.
  It says what the Save is doing, names each game as it is copied
  ("Copied Portal 2 for Ada."), and, for as long as any Save runs, says
  "Do not log out or close the lid until this is finished."
  A Save that only takes games off copies nothing and takes no lock, but
  logging out would still end it before the lists are written.
  At the end it shows the Save's own result sentences and Close.
  Escape closes it only once the Save has ended.
  The copy holds the inhibitor lock above, so closing the lid does not
  suspend the computer; logging out still ends `cairn-family`, which is
  why the window warns.
- "Log Ada out now" on a waiting note, a planned follow-up, and the
  "held-again reason" item are moot: there is no waiting list any more.

### Starting, adding and taking away

- A new child starts with no games, so no game tiles and no Steam at
  login, until a parent saves their list.
- Taking a game off a child's list removes its tile at their next login.
- The child's copy and saves stay in their home: the copy step never
  deletes a child's copy.
  Putting the game back on their list brings back their progress.
- `cairn-family create-child` removes any `games/<login>.json` before it
  makes a new account, and refuses `failed` if one stays, so a new child
  never inherits an old account's games.

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
   Until step 5, a logged-in child's games waited for a second Save.
5. **A logged-in child's games wait, and Save shows its progress.**
   First, from 2026-10-28, a logged-in child's new games were written
   down at the Save and copied at their next logout by a system unit
   ("Later"), or the Save logged the child out first ("Log them out
   now", one password).
   Mason replaced both on 2026-10-29 with the rule above: the waiting
   list, `later`, `cairn-family finish-games`, its unit and drop-in on
   `user-runtime-dir@.service`, the `games-waiting` and `games-finished`
   folders, `save-games --log-out=` and the helper's `--copy-games
   --log-out` are gone.
   In their place: `cairn-steam-visit --log-out CHILD` and `cairn-family
   log-out LOGIN` with the polkit action `com.cairnlinux.family.log-out`;
   the page's greyed, blurred games with **Log Ada out**; the Save's
   progress window with the lid warning; and `systemd-inhibit` around the
   copy job.
   Landed 2026-10-29, with tests.
6. **Checks in the cairn-iso VM**, with two children at different
   levels: one password per Save, with the games message, while adding a
   child still shows the account message; each child sees only their own
   tiles; a Guardian logout mid-copy; and a game taken off and put back
   with its saves.
   For step 5: Ada logged in shows her games greyed and blurred, and
   **Log Ada out** with Steam and a game running asks once, with the
   log-out message, logs her out, and her games show again within a few
   seconds without reopening the page; a program that will not close
   gives the restart sentence; Ada logging in while the page is open
   greys her games; the progress window names each copied game and warns
   about the lid; `systemd-inhibit --list` shows Cairn during a copy, and
   neither closing the lid nor `systemctl suspend` as the Guardian
   suspends the computer then, while Plasma's own lid handling is in
   charge (PowerDevil calls logind, which the block lock should refuse);
   a shutdown during a copy still shuts down; and the blur draws on the
   VM's renderer, or the fade alone shows.

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
- **ADR-0039** is amended: `cairn-family` gains a subcommand, with its
  own answer lines and refusal words, and its one polkit action becomes
  one for each subcommand.
  `log-out` is a fourth subcommand, with its own action and message.
- **ADR-0040** is amended: the Games page goes beside Children, with a
  way to move between them.
- Logging a child out and saving their games are two passwords.
  The password box cannot name the child: which child, and that anything
  unsaved may be lost, are said on the page before the box opens.
- A child logged out with **Log Ada out** sees their screen go back
  to the login screen, with no warning inside their session; a notice
  there would need a new channel from root to the launcher.
- **Known limit: the list holds only at L1 and L2.** The shared folder is readable by
  every account, and children keep copies of games taken off their list.
  At L1 and L2 a child reaches games only through their tiles, so the
  list holds.
  ScummVM's own menu is a way around the tiles: its Return to Launcher
  leads to ScummVM's game manager, which could add and start any
  ScummVM game in the shared folder (#215).
  Ctrl+F5 opens that menu in every ScummVM game, and the kiosk keeps
  Ctrl+F5 from the games.
  Plain F5 opens it too in the Humongous games, and so does F5 held
  down before Ctrl in any game, both by ScummVM 2.0.0's source.
  Whether the kiosk takes F5 as well is not decided.
  Before levels 3 and 4 ship (#107), Cairn must keep a child from opening
  games off their list from the shared folder or from old copies.
- Every child the family's setup made before step 4 has a copy of every
  game.
  When step 4 lands they show no game tiles until a parent saves their
  list, and their copies stay.
- If `cairn-family` itself is ended after a copy starts, by a Guardian
  logging out, the copy finishes but the child's list is not written,
  and nothing writes it down for later.
  The window's choices are kept nowhere else, so the child's list holds
  only the games taken off; the games copied stay in the child's library
  but give no tile.
  The parent chooses them again and saves, and that Save finds them as
  `have` and writes the list.
  The progress window's warning is the only guard; moving the whole Save
  into a system job would be a separate decision.
- The inhibitor lock blocks sleep and logind's lid handling, not
  shutdown, and not a long press of the power button.
- A closing session that is stuck keeps a child counted as logged in, so
  their games stay greyed; **Log Ada out** kills what is left, and when
  that fails it says to restart.
- Machines that ran the first version of step 5 by hand keep empty
  `games-waiting` and `games-finished` folders, and any unit they
  installed, until they are reinstalled; they do nothing.
- If the copy is killed outright, by a power cut or the kernel running
  out of memory, the child cannot log in until a restart empties `/run`.
  Visits already carry this risk.
- With one shared Steam account, two children's saves can still meet
  through Steam Cloud.
- Dependencies: none new.
