# ADR-0041: Each child has their own list of games

**Status:** proposed. It becomes accepted when the checks in step 5
below have run in the cairn-iso VM and none of them overturns a bullet
of the Decision.
**Date:** 2026-10-27
**Closes:** (none) — the Games page of issue #167; part of ROADMAP P1-5.
Once accepted, amends ADR-0033, ADR-0032, ADR-0035, ADR-0039 and
ADR-0040.

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
- `cairn-family save-games LOGIN=APPID,APPID... [LOGIN=...]` takes each
  named child's whole new list, never a change to it: `ben=` means no
  games, and a child not named is left alone.
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
| `pictures made` or `pictures kept` | The picture step ran to its end, or the pictures stay as they were |
| `catalogue made` or `catalogue kept` | The full games list is new, or the old one stays |
| `copying LOGIN` | The child's copy starts |
| `waiting LOGIN` | The child's last login is still ending |
| `copied LOGIN APPID`, `have LOGIN APPID` | The child has the game now |
| `skipped LOGIN APPID REASON` | The game is left out, for REASON: `not-shared`, or `no-catalogue` when no full games list could be read, so whether it is shared is not known |
| `child LOGIN added N removed N [held WORD]` | The child's list is written, and the counts say what it holds now; `held` says why their new games are not all given |
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

### A child who is logged in

The copy cannot run while the child is logged in, as today.
The parent is told, and chooses "Log them out now" or "Later".
How "Later" finishes the save is decided in a later step.

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
4. **Highlight and Save.**
   In the same change: the launcher's wrapper reads
   `/var/lib/cairn/games/<login>.json`, and the family's setup stops
   copying every game on the image.
   Per-child tiles and the only way to fill them ship together.
5. **Checks in the cairn-iso VM**, with two children at different
   levels: one password per Save, with the games message, while adding a
   child still shows the account message, each child sees only their own
   tiles,
   a Save while a child is logged in, a Guardian logout mid-copy, and a
   game taken off and put back with its saves.

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
- **ADR-0039** is amended: `cairn-family` gains a subcommand, with its
  own answer lines and refusal words, and its one polkit action becomes
  one for each subcommand.
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
  The child then has the copy and no tile, and the next Save finds it as
  `have` and adds the tile.
- If the copy is killed outright, by a power cut or the kernel running
  out of memory, the child cannot log in until a restart empties `/run`.
  Visits already carry this risk.
- With one shared Steam account, two children's saves can still meet
  through Steam Cloud.
- Dependencies: none new.
