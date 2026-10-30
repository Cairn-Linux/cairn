# ADR-0042: A game moves to the family folder when a parent picks it

**Status:** proposed. It becomes accepted when the checks under
"Checks in the VM" have run in the cairn-iso VM and none of them
overturns a bullet of the Decision.
**Date:** 2026-10-29
**Closes:** (none) — the Games page of issue #167 and the parent's set-up
step in #130; part of ROADMAP P1-5. Once accepted, amends ADR-0033 and
ADR-0041.

## Context

ADR-0033 has the Guardian install a game once, from their own Steam, into
the shared folder `/var/lib/cairn/steam-library`. A parent would call it
the family folder, and this ADR does too. Each child then gets their own
reflink copy of a game from there. ADR-0041 gives each child their own
list, chosen on the Guardian tool's Games page, which shows one picture
for each game in the family folder.

For that to work, the parent first has to add the family folder to their
own Steam in Settings › Storage, and then install each game into it, or
move it there with Steam's own Move. `docs/TRY.md` shows the steps.
A parent who just installs a game, the way Steam offers, gets it in their
own folder, `~/.local/share/Steam`, and the Games page never shows it.

That is what happened in the cairn-iso VM on 2026-10-29. The maintainer
installed three games in the Guardian's Steam, and they landed in
`~/.local/share/Steam`. He moved them with Steam's Storage › Move, and
they arrived in the family folder. A Save then copied Arthur's Birthday
(138 MiB) to the test child `ada` by reflink, with 0 bytes extra. `ada`'s
Steam had never been signed in, so whether that copy starts is still
unchecked. A parent who does not know about Storage › Move would have
been stuck on the first step.

While this ADR is written, another change (branch
`guardian/games-elsewhere`) puts a note on the Games page that explains
the move: games in the parent's own Steam do not show until they are
moved to the family folder with Steam's Storage › Move. That note is the
stopgap this ADR replaces.

ADR-0033 rejected making the family folder the Guardian's default library,
because then every game, the parent's own among them, would be copied to
every child. ADR-0041 removed that objection: a child gets only the games
on their own list, whatever is in the family folder.

What is known about moving a game by hand is in
`docs/research/shared-steam-library.md`, "A game moved between libraries
with Steam closed". In short:

- A game is installed in a library when that library's `steamapps/` has
  its `appmanifest_<appid>.acf`, whose `installdir` names its folder under
  `common/`. A manifest names no library.
- Each library in `libraryfolders.vdf` has `path`, `label`, `contentid`,
  `totalsize`, and an `"apps"` map from appid to size. Tools that read the
  file treat the map as possibly stale and read the manifests instead.
  Cairn's copy step adds manifests to a child's own library without
  touching the map, and Steam loads them (ADR-0033, check 7).
- A hand-written entry for the family folder, with a `libraryfolder.vdf`
  marker holding the same `contentid`, worked in every account's Steam on
  the kids' laptop on 2026-10-07.
- Steam writes the library list itself, so an edit has to be made with
  Steam closed.
- Valve documents no way to set the default library, and no source names
  where Steam keeps it.
- On Bazzite, homes and `/var` are separate btrfs subvolumes, so a rename
  between them fails with `EXDEV`. The same filesystem can still reflink
  between them, which is what the VM's copy to `ada` did.
- No source says which copy Steam uses when two libraries it lists both
  hold one game.

## Decision

**The Games page lists every game in the Guardian's own Steam, wherever
it is installed. When the parent picks a game for a child and presses
Save, and the game is not in the family folder yet, Cairn moves it
there, and then copies it to each child as before. Cairn adds the family
folder to the Guardian's Steam once, itself. The Guardian's Steam is
closed while a game moves, and Save asks the parent first.**

### The Games page

- The page shows the games in the family folder, as now, and every other
  game in the Guardian's own Steam, from every library in its list.
  Steam's runtimes, Proton and redistributables are left out as kidscan
  leaves them out. A game in both places shows once.
- The window reads the Guardian's own Steam as the Guardian, without
  root, by running kidscan with `--no-scummvm` over the Guardian's own
  Steam folder. A game's picture comes from the Guardian's own
  `librarycache`, read with the picture step's rules. Root's picture step
  and `/var/lib/cairn/game-pictures` stay as they are: they hold only the
  family folder's games, so no child can read the names or pictures of
  the parent's other games.
- A game that is not in the family folder yet looks the same as any other
  game. Picking it is all a parent does.
- If any game that Save would move is picked, and the Guardian's Steam is
  running, Save first asks:
  "Steam needs to close while Pep's Birthday Surprise moves into the
  family's games. Steam opens again when this is finished." with
  **Close Steam and save** and **Not now**, and focus on **Not now**.
  **Close Steam and save** closes the Guardian's Steam with the code a
  visit already uses (`OwnSteam`, ADR-0032), which asks Steam to save and
  quit and waits up to 30 seconds. A parent who closes Steam themselves
  meanwhile is fine too: the window notices and carries on.
  If Steam is still open after 30 seconds, nothing is saved, and the
  window says "Steam did not close, so nothing changed. Close Steam, then
  press Save again."
  When the Save has finished, Steam starts again, only if the window
  closed it.
- The Save's progress window says "Moving Pep's Birthday Surprise into
  the family's games." for each game it moves.
- A game that could not move gets one plain sentence in the result and
  stays in the parent's own Steam, unchanged. For a game on another
  drive: "Pep's Birthday Surprise is on another drive, so Cairn cannot
  move it without copying all of it. To share it, move it in Steam:
  Settings, Storage, then Move, to the family's games folder." For the
  rest: "Pep's Birthday Surprise could not be moved, so it is still only
  in your own Steam." The window may name the reason where it helps the
  parent, as it does for a copy's reasons today.
- **Look for new games** also reads the Guardian's Steam again.
- These words are proposals; the change that builds the page fixes them,
  in the voice of CLAUDE.md.

### The move

- **What moves.** A game's manifest and its `common/<installdir>`. Not
  its Wine prefix (`compatdata/<appid>`), its shader cache, or its
  workshop items, which stay where they are (see Consequences).
- **A move, not a copy.** After the move the game is in the family
  folder only. Two installed copies of one game in one Steam would leave
  Steam to pick one to update, and the family copy, which every child's
  copy is made from, could go stale. No source says which copy Steam
  picks.
- **Reflink, then remove.** A rename cannot cross from the Guardian's home
  to `/var/lib/cairn`, so the move is a reflink copy of the folder, then
  the removal of the original. The copy uses the copy step's own flags,
  `cp -R --reflink=always --preserve=timestamps`, and keeps no mode or
  ACL from the source. Each new file and folder then takes the family
  folder's default ACL and setgid bit, and its run bits from the
  source's mode, as Steam's own install there would. It takes no
  extra disk, and a few seconds even for a large game. If the filesystem
  cannot reflink, as for a library on another drive, the game does not
  move: Cairn never fills the disk with a full copy, as the copy step
  never does.
- **In an order that leaves the game in one library.**
  1. The folder is copied under a temporary name in the family folder's
     `steamapps/common/`.
  2. The manifest is written in the family folder's `steamapps/`, under
     a temporary name.
  3. The original manifest is renamed aside, under a name only this step
     uses, so Steam no longer sees the game in the old library.
  4. The copy is renamed to `common/<installdir>`, and then the family
     manifest is renamed to its own name.
  5. The original folder and the set-aside manifest are removed.
  Ctrl-C is held off from step 3 to the end of step 4. A kill, a power
  cut or a stop timeout can still end the step anywhere. Its next run
  looks at each set-aside manifest. If the family manifest is in place,
  it removes the original folder and the set-aside manifest. If the
  copy already has its full name, the temporary family manifest is
  there too, since step 2 came first, so it renames that and finishes
  the move. Otherwise it removes its temporary folder and temporary
  manifest, and puts the set-aside manifest back. A Steam started
  meanwhile sees the game in one library or, between steps 3 and 4, in
  none, until it starts again.
- **Which games.** A game moves only if the copy step would then copy
  it: the same rules, read once from the one manifest. Its appid is a
  number, its `installdir` is one plain folder name, its `StateFlags` is
  exactly 4, it is not on a beta branch, and `downloading/` has nothing
  for it. Every file in it can be read by every account, since the
  copy step runs as the child. The family folder must have neither a
  manifest nor a `common/<installdir>` for it. Otherwise the game stays
  where it is, with a reason.
- **As the Guardian.** The step runs as the Guardian whose password
  opened the Save (`PKEXEC_UID`), never as root. The Guardian owns the
  game's files in their own home, and the family folder is writable by
  `cairn-guardian` (ADR-0033: root and `cairn-guardian`, mode 2775,
  default ACL for the group). So the moved files end up as Steam's own
  install there would: owned by the Guardian, group `cairn-guardian`
  through setgid, the group in each access ACL and each folder's default
  ACL through the family folder's default ACL, readable by every
  account. Files a later Steam update makes inside a moved game follow
  ADR-0033's rule too. A link planted anywhere can send a write
  nowhere the Guardian could not already write. The step drops to the
  Guardian as the copy step drops to a child (`setpriv`, `env -i`,
  `python3 -I`, stdin from `/dev/null`), finds the Guardian's home in the
  password database, and prints only fixed words and appids.
- **Steam closed.** Before each game the step checks that no process of
  the Guardian's Steam runs, with the rule `OwnSteam` uses. If one does,
  the game does not move. The window has already asked, so this is the
  guard, not the way the parent hears of it.
- **Under sudo** there is no Guardian, nothing moves, and a game not in
  the family folder is `not-shared`, as now.

### The family folder in the Guardian's Steam

- At the first Save that moves a game, Cairn adds the family folder to
  the Guardian's own library list, once, with Steam closed and as the
  Guardian. This replaces ADR-0033's step in Settings › Storage;
  ADR-0033 already named it as that step's fallback.
- It adds one entry to both `config/libraryfolders.vdf` and
  `steamapps/libraryfolders.vdf`: the next free number, `path`
  `/var/lib/cairn/steam-library`, `label` `""`, a `contentid`,
  `totalsize` `"0"`, and an `"apps"` map of the games it is about to
  move. The entry is inserted as text before the file's last closing
  brace, after the file has been parsed with kidscan's parser, so none of
  Steam's own text changes. Each file is written under a temporary name
  and renamed, and a copy of each, as it was, stays beside it as
  `libraryfolders.vdf.before-cairn`.
- The `contentid` comes from the family folder's `libraryfolder.vdf`
  marker when there is one. Otherwise it is a new random 64-bit number,
  and Cairn writes the marker, with that `contentid` and `label` `""`, as
  the laptop's script did.
- A list that already names the family folder, as the VM's does after
  the maintainer's Storage step, is left alone. A list Cairn cannot read
  or write leaves every game where it is, with a reason.
- Cairn changes no other entry and no `"apps"` map after this one write.
  Steam rewrites the list at its next start (ADR-0033, check 5). So a
  game moved at a later Save is missing from the family entry's `"apps"`
  map until Steam next starts. Whether Steam loads such a game, in an
  entry other than `"0"`, is untested (VM check 12). If it does not, the
  move step adds each moved appid to the family entry's map instead,
  with Steam closed, in the same way.
- Cairn does not make the family folder the Guardian's default library.
  Steam keeps that somewhere undocumented, and a parent's new game still
  lands in their own folder, which is fine: it moves when they pick it.

### The root side

- `cairn-family save-games` keeps its arguments, polkit action and
  password message ("A grown-up's password is needed to change which
  games a child has."), so ADR-0039's one root command still holds.
- After `looking`, it takes every appid newly ticked for any child that
  the full games list does not have, and runs the move step with them as
  a system job, as the Guardian, under the same `systemd-inhibit` lock
  as a copy (ADR-0041). Then it makes the pictures and the full games
  list, so a moved game is in it, and then copies, as now.
- The move step is a new small standard-library Python program,
  `provision/cairn-share-games.py`, installed in `$PREFIX/libexec/cairn/`
  beside kidscan, which it shares the VDF parser with, with tests. Its
  lines:

| Line | Meaning |
|---|---|
| `library added` or `library had` | The family folder is now in the Guardian's list, or already was |
| `moved APPID` | The game is in the family folder now, and only there |
| `shared APPID` | The game was in the family folder already |
| `not-moved APPID REASON` | The game stays where it is |
| `refused root`, `refused appid` | It was run as root, or given an appid that is not a number, and did nothing |

  REASON is one of `steam-open`, `not-found` (in no library of the
  Guardian's Steam), `not-installed`, `downloading`, `installdir`,
  `beta-branch`, `unreadable-files`, `exists` (the family folder has its
  folder but no manifest), `other-drive` (no reflink), `no-shared`,
  `unreadable-config` (the library list could not be read or written),
  or `failed`.
- `cairn-family` passes these on as `moving APPID`, `moved APPID` and
  `not-moved APPID REASON`, and `library added`. A child who had a game
  ticked that did not move gets `skipped LOGIN APPID REASON` with the
  move's reason, and the rest of the Save goes ahead. A child's problem
  is still never a refusal of the whole Save. `not-shared` stays for an
  appid in neither place.

## Consequences

- A parent installs a game the way Steam offers, picks it on the Games
  page, and presses Save. They never visit Settings › Storage, and the
  Games page's note that explains the move goes.
- **The parent's game is now in the family folder.** Their Steam shows it
  as installed there and plays it from there. A game they play there
  keeps its saves in its own folder there, and Steam Cloud brings their
  cloud saves there, so a copy made later for a child starts with them.
  ADR-0033 already says so for games installed there.
- Taking a game off every child's list does not move it back. It stays in
  the family folder, where every account can read it, as ADR-0041's
  known limit says for levels 3 and 4.
- **A Windows game the parent has played starts fresh for the parent.**
  Its Wine prefix, and any save the game keeps inside it, stays in the
  old library, and Steam makes a new prefix in the family folder. Steam
  Cloud brings back saves for games that use it. The old prefix stays on
  disk until the parent removes it. It is not moved so that the parent's
  saves from before the move stay private in their closed home.
- **From then on, the parent's saves for a moved game can be read by
  every account.** The new prefix, and any save the game keeps in its
  own folder, sit in the family folder, where every account can read
  them. ADR-0033 already accepts this for games installed there. No
  child can change them.
- Workshop items stay in the old library. Steam may download them again
  for the family copy. Children's copies have never had them.
- A game on another drive, or on a filesystem that cannot reflink, does
  not move. The parent can still use Steam's own Move, which copies all
  of it, or install it again into the family folder.
- Steam closes for the length of a Save that moves games, and opens
  again afterwards. A game the parent was playing in their own Steam is
  asked to quit with it.
- Two Guardians: the game moves from the Steam of the Guardian whose
  password opened the Save. The other Guardian's Steam lists the family
  folder only after their own first move, or Storage step. A second
  Guardian updating a game the first installed stays untested
  (ADR-0033).
- Cairn writes one of Steam's files now, the Guardian's library list,
  once, besides copying manifests as before. A Steam update could change
  how Steam treats a hand-written entry. The ROADMAP's risk on Steam's
  files now names this write: a game that does not show in the Guardian's Steam
  after a move is still in the family folder, and the parent can add the
  folder in Settings › Storage, as before.
- `docs/TRY.md` loses the Storage and Move steps when the move lands.
- The kids' laptop keeps ADR-0033's steps: it has no Guardian tool.
- Nothing phones home, and there is no new dependency: `cp --reflink`,
  `setpriv`, `systemd-run` and standard-library Python are in the image.

### Steps

1. **The move step.** `cairn-share-games.py` with its tests: the order of
   a move, recovery from each point it can be ended at (among them a
   copy already renamed whose family manifest is still temporary), every
   reason,
   the library-list entry and its marker, and the Steam-open guard.
   It lands alone, as the copy step did, since it writes in a Guardian's
   home.
2. **`cairn-family save-games` runs it,** with the new lines, as a system
   job under the inhibitor lock, with tests in `tst_gamessave`.
3. **The Games page lists the Guardian's own Steam,** asks to close Steam,
   says what moved, and drops the interim note; `docs/TRY.md`,
   `guardian/README.md` and `provision/README.md` change with it.

### Checks in the VM

In cairn-iso, with the Guardian's Steam signed in and two children at
different levels:

1. **Read first.** Both of the Guardian's `libraryfolders.vdf` files and
   the family folder's `libraryfolder.vdf` as Steam's own Storage step
   wrote them, to compare with the entry Cairn writes. `findmnt` and
   `btrfs subvolume show` for the Guardian's home and `/var/lib/cairn`,
   and a test file moved between scratch folders in each, to confirm the
   `EXDEV`.
2. **The first move.** On a Guardian whose list does not name the family
   folder, pick a game in `~/.local/share/Steam` for `ada` with Steam
   open. The window asks, closes Steam, moves the game and copies it to
   `ada`. Disk use before and after is the same, give or take the copy's
   metadata.
3. **The Guardian's Steam afterwards** shows the family folder in
   Settings › Storage and the game installed there, not in its own
   folder, downloads nothing, and starts the game. After two restarts
   the entry is still there. Record what Steam did to both `"apps"`
   maps, and whether it changed the `config/` copy.
4. **Files.** `getfacl` on the moved game: owner the Guardian, group
   `cairn-guardian`, the group in each access ACL, each folder with the
   default ACL and setgid, every account can read. `ada`'s Steam, signed in through a visit, starts her copy with
   no download.
5. **An update.** A game with an update or a public beta branch in the
   Guardian's Steam, after its move: Steam updates it in the family
   folder (ADR-0033, check 13). `getfacl` on a file and a folder the
   update made inside the moved game shows the same as check 4.
6. **An interrupted move.** A large game, with the move step killed
   midway: after the next Save the game is in exactly one library, and
   the Guardian's Steam shows it installed.
7. **A Windows game** moves, the Guardian's Steam makes a new prefix in
   the family folder, and the game starts for the Guardian and, from her
   copy, for `ada`.
8. **Another drive.** A game in a library on a second virtual disk gives
   the other-drive sentence and stays put.
9. **The default library** is still the Guardian's own folder in Steam's
   install dialog.
10. **Both places.** A game in the family folder and in the Guardian's own
    folder shows once and is not moved.
11. **Steam started mid-move,** by hand: the game is in at most one
    library afterwards, and in exactly one after the next Save.
12. **A second move.** After the family entry exists, a second Save
    moves a further game with Steam closed. The Guardian's Steam shows
    it installed in the family folder and downloads nothing. Record both
    `"apps"` maps before Steam starts and after.

## Alternatives considered

- **The interim note** (branch `guardian/games-elsewhere`): the page
  explains Steam's Storage › Move and the parent does it. No code writes
  Steam's files, but it asks a non-technical parent for a step most will
  not find, and a Windows game hits Steam's Move rollback bug (ADR-0033).
- **The family folder as the Guardian's default library.** ADR-0041
  removed ADR-0033's objection to it. Still not chosen: Valve documents
  no way to set the default, a game a parent installs before Cairn sets
  it up would still need a move, and every game the parent installs,
  their own ones too, would sit where every account can read it. Moving
  only the games a parent picks keeps the rest in their closed home.
- **A copy instead of a move.** The parent's own copy would stay where
  it is, untouched. Not chosen: with two installed copies Steam updates
  one, and the family copy could go stale, and the copies' saves
  diverge.
- **Cairn running Steam's own Move.** Steam has no command or link for
  it, only its window, and its Move of a Windows game has an open bug.
- **Moving as root.** Simpler permissions, but root writing in a home
  that holds files the Guardian can change is the kind of step ADR-0033
  avoids by running as the account whose files they are.
- **Asking the parent to close Steam themselves, with no button.** Kept
  as a fallback: a parent who does is fine. The button uses code the
  window already has for visits, and saves a trip to Steam's menu.
