# ADR-0033: Each child gets their own copy of every game

**Status:** proposed. It becomes accepted when the checks under
Consequences have run on the kids' laptop and none of them overturns a
bullet of the Decision.
**Date:** 2026-10-12; rewritten 2026-10-13
**Closes:** (none) — addresses #130 and #24's question of where game tiles
live; once accepted, amends ADR-0024

## Context

On 2026-10-07 the family's games reached both children's Steam on the kids'
laptop by hand, over ssh (#130). The steps were a group, a shared folder
with a default ACL, a script that rewrote each account's library list with
Steam stopped, and tiles merged into the installed manifest by hand. Native
games started from the children's tiles, and nothing was downloaded. Three
things did not work for any account but the one that owned their files.
Steam's runtime first failed, then took 18 s to start against 2 s for its
owner. The one Proton game's Wine prefix was refused. Mesa's shader cache
could not be written. Tools and the Proton game were copied into each
account's own library by reflink, which got past the first two. A
provisioning re-run puts the stock manifest back, and the game tiles go.

On 2026-10-12 the maintainer decided that the parent installs a game once,
from their own Steam in their own login, and that no child's Steam
downloads a game of its own (ADR-0032, Context; ADR-0032 is still
proposed). The first draft of this ADR then had every child's Steam list
the one shared folder, as on the laptop.

On 2026-10-13 the maintainer asked whether one child could delete
another's saves. They can (`docs/research/shared-steam-library.md`, "Where
the games keep their saves"). The games keep their saves inside their own
folder. Each ScummVM game's own start-up script runs its bundled ScummVM
with a settings file that says `savepath=../Saves/`. The three read on the
laptop do, and the other eight share their cloud rule and their build. The
Windows game saves in `SaveGames/`. The games use the same file names for
every child, so in a shared folder one child's save replaces another's,
and any child can delete the files. On the laptop one child's saves sat
there, and Steam Cloud backed them up for the one Steam account both
children use. The same day the maintainer chose a copy of each game for
every child.

The research note collects what else is known. Valve documents no way to
share a library between Linux accounts. Steam has no command or link that
adds a library. Wine refuses a prefix another account owns, and Mesa makes
its cache folders 0700. Any account that can write a shared folder can
uninstall a game for everyone, and can swap a program in it that another
account later runs. A child's Steam rewrites a game's manifest in a
library it lists when it runs the game: on the laptop a child owns 10 of
the 11 shared manifests. A reflink copy on btrfs shares the original's
disk blocks until one side changes: the laptop's copies of Steam's tools
took 0 B.

DESIGN §4.6 puts game installs in `/var`. DESIGN §3.2 gives running the
game scanner to the Guardian, and §8.7 describes kidscan. The launcher
reads one manifest, and the Steam-at-login check reads the same one
(ADR-0027).

## Decision

**The Guardian installs each game once, from their own Steam, into a
folder only Guardians can write. Provisioning gives every child their own
copy of each game in it, in the child's own Steam library, by reflink, so
a copy costs no disk until it changes. A child's Steam never lists the
shared folder. A game's saves, Wine prefix and shader cache are then in
the child's own home, which no other child can open.**

- **The shared folder.** Provisioning makes
  `/var/lib/cairn/steam-library/` and `steamapps/` inside it. Each is
  owned by root and the `cairn-guardian` group (ADR-0032), mode 2775,
  with the group in its access ACL and the default ACL
  `d:u::rwx,d:g::rwx,d:g:cairn-guardian:rwx,d:m::rwx,d:o::r-x`. Whatever
  the umask, files a Guardian's Steam makes there then give the group what
  Steam's own modes ask for, and every account can read them where those
  modes let others read (check 13). No child is in the group, so no child
  can change, add or remove anything there. The setgid bit goes on
  folders, never on files: on a file it makes the Steam Linux Runtime
  abort. Folders Steam makes inside inherit it, which is harmless.
  Provisioning sets this on the folders it made and never walks the games
  inside them.
- **The Guardian adds the folder to their own Steam once,** in Steam's
  Settings › Storage, before any game is in it. That is Steam's own
  supported way, and Cairn writes none of Steam's files for it.
  `docs/TRY.md` shows it, with the path to pick. If check 12 shows Steam
  refusing a folder that holds an empty `steamapps/`, provisioning makes
  only the top folder and leaves `steamapps/` to Steam. If Steam refuses
  that too, this ADR is amended so that provisioning adds the entry to the
  Guardian's own library list, as the Guardian, with their Steam stopped.
- **The Guardian installs into it.** To install a game for the children,
  the Guardian picks the shared folder in Steam's install dialog. A game
  already in the Guardian's own folder moves there with Steam's own Move
  (Settings › Storage). Steam's Move of a Windows game has an open bug
  that rolls it back, so such a game is uninstalled and installed again
  into the shared folder, at the cost of one download. `docs/TRY.md` shows
  each.
- **Provisioning copies each game into each child's own library.** This
  happens on every run, for every child whose Steam has run once, one
  child at a time.
  - **The lock.** Provisioning takes the lock a visit takes (ADR-0032),
    with the visit's helper's own code. It names the child in
    `/run/cairn/steam-visits`, which names one child at a time for the
    whole computer, with the label PAM can read. Only then does it check
    that no process of the child runs. It removes the file on every exit,
    whether the copy finished or not. So the child cannot log in during a
    copy, and the login screen says to try again soon.
  - While a visit is open, no child gets copies. A child who is logged in
    or on a visit gets a `note:` line in provisioning's output, and their
    copies at the next run. A visit tried during a copy is refused, and
    the Guardian's window says games are being copied, not that a visit is
    open.
  - **Which games.** The step reads each shared manifest once and decides
    from that one read. It leaves a game out, with one sentence, when its
    appid is not a number, its `installdir` is not one plain folder name,
    it names a beta branch the child's Steam account may not have, its
    `StateFlags` is not exactly 4 (fully installed, nothing pending), or
    the shared `downloading/` has something for it.
  - **Never over the child's own files.** If the child's library already
    has a manifest for the game, nothing changes. The child's copy holds
    their saves, so Cairn never replaces, updates or deletes it. A
    `common/<installdir>` there without a manifest is left alone too,
    with one sentence, since it may hold the child's saves. `docs/TRY.md`
    says how to set it aside. Before anything else, the step removes any
    temporary folder of its own left by an earlier run.
  - **The copy.** The step copies `common/<installdir>` under a
    temporary name in the child's `steamapps/common/`, by reflink only
    (`cp --reflink=always`), keeping each file's mode. If the filesystem
    cannot reflink, it copies nothing and says so in one sentence, rather
    than fill the disk with full copies. If the shared manifest changed
    meanwhile, the copy is thrown away with one sentence (check 13).
    Otherwise the folder is renamed into place. Then the manifest it read
    is written under a temporary name and renamed into place, so the
    child's Steam never sees a manifest for a half-copied game, or half a
    manifest.
  - **As the child.** The step runs as the child, never as root, so a
    link planted in the home cannot send a write anywhere the child could
    not write already. Provisioning installs it, with the part of kidscan
    it shares, in `$PREFIX/libexec/cairn/`, which every account can read
    and only root can change. The checkout itself sits in the Guardian's
    closed home. Provisioning drops to the child as the visit's helper
    does, with `setpriv` and `env -i`, and runs `python3 -I` with stdin
    from `/dev/null`. `-I` keeps the child's files off Python's path, so
    the step finds the shared parser by its folder's own path, and the
    child's home from the password database. Provisioning reads the
    step's output through a pipe and prints its own sentences, so nothing
    from the child's home reaches the Guardian's terminal.
  - A child whose Steam has never run has no library yet, and gets their
    copies at the first run after the visit that signs their Steam in
    (ADR-0032).
  - The step is a small standard-library Python program in `provision/`
    that shares kidscan's VDF parser, with tests.
- **Updates are each Steam's own.** The Guardian's Steam keeps the shared
  copy up to date, and each child's Steam keeps the child's copy up to
  date before it starts the game. Cairn never refreshes a child's copy
  from the shared one, because it cannot tell a game's saves from its
  other files.
- **Windows games need nothing more.** Steam keeps a game's Wine prefix in
  the library the game is in, so a child's copy gets a prefix in the
  child's own library, owned by the child, at its first start. The same
  holds for Mesa's shader cache. No list of Proton games is needed.
- **Steam's tools stay in each account's own library.** Each account's
  Steam fetches and updates its own Steam Linux Runtime, Proton and
  redistributables. `docs/TRY.md` has the Guardian start one game in a
  visit once a child's copies are made, so the first fetch happens there,
  out of the child's sight.
- **The Guardian tries a game in a visit** (ADR-0032), which starts the
  child's own copy. A game played from the shared folder in the
  Guardian's own login keeps its saves there. The Guardian's Steam also
  downloads the Guardian account's Steam Cloud saves into the shared
  folder whenever it starts, played or not (laptop, 2026-10-13). So a
  copy made later starts with those saves. A Windows game played there
  also leaves its Wine prefix there, where every account can read it.
  `docs/TRY.md` says so.
- **Only games every child may play go in the shared folder.** Every
  child gets a copy of each one. A game for one child is installed into
  that child's own library in a visit.
- **Game tiles come from the shared folder and survive provisioning.**
  - The launcher takes a second manifest, `--games <file>`. The kiosk
    wrapper passes `/var/lib/cairn/games.json`. One small class reads both
    files. TileModel and the Steam-at-login check both use it, so a child
    whose only Steam tiles are games still gets Steam at login (ADR-0027).
    Game tiles come after the manifest's and before the Terminal's. The
    Terminal's doors follow, since they come from the tiles.
  - A missing games file, or one with no entries, means no game tiles. It
    is not an error. A games file the launcher cannot read is left out,
    with one sentence in the journal, and the other tiles still show.
  - From the games file the launcher takes only entries whose command is
    `steam -applaunch` and a number. It leaves out any other, with a
    sentence in the journal. The file is made from Steam's files, so
    nothing in it may choose a program.
  - On every run, provisioning runs kidscan with `--no-scummvm` over the
    shared folder only, as root with `python3 -I` and an empty `HOME`, so
    no home's Steam is read. It writes to a new file and renames it into
    `/var/lib/cairn/games.json`, root-owned, mode 0644. With no shared
    folder it leaves the file alone. The manifest in
    `/usr/local/share/cairn/` stays provisioning's, as now.
  - kidscan leaves out a game whose `StateFlags` lacks FullyInstalled (4)
    and an appid that is not a number, and gives Steam games the id
    `steam-<appid>` (#24). It also leaves out what the copy step always
    leaves out: an `installdir` that is not one plain folder name, and a
    game on a beta branch. A game with an update pending keeps its tile.
  - Routing every shared game through Steam differs from DESIGN §8.2, which
    runs SCUMM titles under native ScummVM with no Steam client. #130 asks
    for Steam itself. Through Steam each child's copy keeps its saves in
    the Steam Cloud of the child's Steam account, which is the child's
    own only with one Steam account per child. The maintainer decides
    which before acceptance, and §8.2 is edited in the same change if
    Steam wins.

This ADR leaves to later ADRs, as ADR-0032 does: tiles that follow what
each child's Steam can play, and taking a game off a child's tiles for a
while.

## Consequences

- #130's acceptance: provisioning makes the shared folder, copies its
  games into each child's library, and writes game tiles from kidscan that
  survive a re-run. The same steps serve all three account layouts in
  #130, and each is written up in `docs/TRY.md` and tried on the laptop.
  The laptop's Proton game still quits after 2 s, which is a separate
  fault.
- **Saves stay with the child.** A game that saves in its own folder saves
  in the child's copy, in the child's home. No other child can open it.
  A child's Steam can uninstall only that child's copy. Uninstalling a
  copy may delete the saves in it: Valve documents nothing, one Linux
  report saw the whole folder go, and Windows reports differ (check 19).
  Resetting the child's account (DESIGN §11) deletes them. Steam Cloud
  keeps a copy for games that use it.
- **An uninstalled copy comes back** at the next provisioning run, with
  whatever saves the shared folder holds, if the uninstall removed the
  game's whole folder. If it left the folder behind, with the saves in
  it, the step leaves that game out for that child, and its tile ends on
  the grown-up screen until the Guardian sets the folder aside, as
  `docs/TRY.md` shows (check 19). Taking a game away from one child
  belongs to the later ADR.
- **Steam Cloud follows the Steam account.** With a Steam account per child,
  each child's saves go to their own account's cloud. With one Steam account
  for several children (layouts 1 and 2 in #130), Steam Cloud carries the
  newest save into every child's copy before the game starts, with no
  dialog, so those children still share their saves (laptop, 2026-10-13,
  check 10). A save deleted from one copy leaves the cloud but stays in the
  others. `docs/TRY.md` says so. It says that turning Steam Cloud off for
  those games, or for the whole Steam account, should stop it, at the cost
  of the only copy kept off the computer. The switch belongs to the Steam
  account, so with the parent's own account (layout 1) it also goes off on
  the parent's other computers. Community reports say it can turn itself
  back on. The two children on the laptop share one Steam account.
- **Updates.** A new game downloads once, into the shared folder. An
  update downloads once for the Guardian's Steam and once for each
  child's. If a child taps a game before their Steam has its update, Steam
  starts no reaper until the update is done. The starting screen then
  gives way to "Something needs a grown-up" at 15 s. Within a minute of
  login it can wait up to about two minutes (ADR-0029). The game takes
  over if it comes before the child presses Back (ADR-0030). Steam,
  started at login, may often fetch the update in the background first
  (not tried; check 15). What happens offline is check 18. Steam's tools
  also download once per account, as before.
- **Disk.** A copy takes no disk until it changes: on the laptop, three
  accounts' copies of 11 games, 2 GiB each, took 0 B. Then each child's
  updates, Wine prefixes and shader caches take their own disk.
- **Safety.** No child can write the shared folder. So no child can change
  a game for another child or for the Guardian, or uninstall a game for
  everyone. A child's copy is the child's to change, as anything in their
  home is, and it only ever runs as that child.
- **Steam Family's allow-list does not hold for a copy.** Every child can
  read the shared folder and has a copy of every game in it. At L3 and L4
  a child can start a DRM-free game's own program without Steam, as the
  ScummVM games' start-up scripts are. Hence the rule that only games
  every child may play go in the shared folder. A per-child list, when it
  comes, has to decide which copies are made, not only which tiles show.
- **A new game reaches a child at the next provisioning run with that
  child logged out.** Until then the child's tile for it starts a game
  their Steam has no copy of. That opens Steam's Install dialog, which
  waits and downloads nothing (check 17). Behind the kiosk it ends on the
  grown-up screen (ADR-0030).
  Later the Guardian tool runs the copy (#113, P1-5). If a visit's close
  ran it, that would amend ADR-0032's "A visit never reads or writes
  Steam's files".
- A new child's set-up becomes: provisioning, a visit to sign their Steam
  in, provisioning again to copy the games, and a visit that starts one
  game so Steam fetches its tools (not tried).
- With one Steam account per child, every L1 and L2 child sees a tile for
  every shared game, as every kiosk child sees every tile today. A game
  that child's Steam account cannot play probably ends on the grown-up
  screen (ADR-0030; check 11). Tiles per child are the later ADR.
- **One copy in the family is one player at a time.** While one Steam
  account plays a game the family has one copy of, another account's
  Steam will not start it, the Guardian's included, and says "Shared
  library locked". The game already running carries on, and a different
  game starts. In the kiosk the tile ends on the grown-up screen (laptop,
  2026-10-13, check 11).
- Tiles change only when provisioning runs, and a child sees them at the
  next login.
- Resetting a child's account (DESIGN §11) clears their Steam, its
  sign-in, tools, copies and saves. The shared folder keeps the games. A
  visit and a provisioning run set the child up again. Removing a child
  removes their copies with their home.
- A new member of `cairn-guardian` gets write access to the shared folder
  at their next login. A Windows game played from the shared folder works
  only for the Guardian whose Steam first started it, since Wine refuses a
  prefix another account owns. A second Guardian updating a game the first
  installed is untested.
- Cairn reads Steam's manifests, which Valve does not document, and copies
  them. It writes none of Steam's files of its own. A Steam update could
  change how Steam treats a copied manifest. Steam's own launch failures
  end on the grown-up screen, which is the fallback the ROADMAP's risk
  table names.
- No new dependency: `setfacl`, `setpriv` and `cp --reflink` are in the
  image, and the step is standard-library Python. Nothing phones home.
  The only network traffic is Steam's own.
- **The kids' laptop moved over on 2026-10-13, by hand,** with the
  maintainer's go and the children logged out. The shared folder came out
  of the library list of every account but the Guardian's, the L2 test
  account's included, as that account with its Steam stopped (check 5).
  Those accounts left the hand-made `cairn-steam` group. The two top
  folders went to root and `cairn-guardian`, as the Decision says.
  Everything below went to the Guardian and `cairn-guardian`, with the
  group in each access ACL, the default ACL and setgid on folders, and no
  `cairn-steam` entry left. The children's leftovers in the shared
  `compatdata/` and `shadercache/` went, and then the group (check 6). A
  hand-run copy step gave each account the 11 games, and each child's
  copies started with the saves the shared folder had (check 8). The
  shared `Saves/` folders were then emptied, and the Guardian's Steam
  filled one again from its cloud at its next start. The Windows game has
  no tile until it is in the shared folder (checks 14 and 20). A reflink
  backup of the shared folder and of each account's Steam settings is
  kept on the laptop, beside a script that undoes the move.
- When this ADR is accepted, the same change edits DESIGN §4.6: the
  shared folder among the game installs in `/var`, and each child's
  copies, with their saves, in `/home`, gone with a reset. It edits §8.7:
  where kidscan's output lives and who writes it. ADR-0024's "Where the
  manifest lives and who runs the scanner stay with #24" is answered for
  games, and its status becomes "amended by ADR-0033". ADR-0027 keeps its
  words: its check now reads the tiles from both files. ADR-0013 already
  means one list of tiles, since the doors come from the tiles.
  `provision/README.md`, `launcher/README.md`, `tools/kidscan/README.md`
  and `docs/TRY.md` change with it.
- **Checked before acceptance,** by number in the research note: 5 to 20,
  on the laptop. Passed on 2026-10-13: 5, 6, 7, 8, 10, 17 and 20, and the
  first half of 11. Still to run: 9, the second half of 11, 12 to 16, 18
  and 19, which need the Windows game in the shared folder, a game outside
  the children's allow-list, clicks in Steam's own windows, a visit, or an
  update waiting.
  Check 12 decides which of the second bullet's ways adds the shared
  folder to the Guardian's Steam.

## Alternatives considered

- **One shared folder that every child's Steam lists** (this ADR's first
  draft, and the laptop since 2026-10-07). One copy on disk, and an update
  downloads once. Not chosen. Every child shares one set of saves in each
  game's folder and can replace or delete a sibling's (laptop,
  2026-10-13). Every child can change or uninstall a game for the others,
  and change a program the Guardian later runs. Wine refuses a shared
  prefix and Mesa a shared cache, so Windows games needed a copy per child
  anyway. And Cairn had to write Steam's library list in every child's
  home.
- **One shared copy, with each child's saves sent to their own home.**
  ScummVM reads a save folder from `SCUMMVM_SAVEPATH` before the one in
  its settings file. It covers only ScummVM games, takes their saves out
  of Steam Cloud, and ScummVM falls back to the shared folder without a
  word if the child's folder is missing. It leaves the first draft's
  other problems.
- **Children read-only in a shared folder their Steam lists.** The games
  write their saves into their own folder, and a child's Steam rewrites a
  game's manifest there when it runs the game (laptop, 2026-10-13).
- **One shared copy with parts per account:** `pam_namespace` or a bind
  mount over the shared `compatdata/` and `shadercache/`, a
  `STEAM_COMPAT_DATA_PATH` launch option, a link switched at login, or a
  library per account with each game linked into the shared one (the
  layout in Bazzite's docs). Each answers Wine's prefix. None answers
  saves inside the game's own folder.
- **Each child's Steam downloads its own copy.** Cairn would need no copy
  step, but every game would download once per child, against the
  maintainer's decision of 2026-10-12.
- **Refreshing a child's copy from the shared one after an update,** so
  that an update downloads once. Cairn cannot tell a game's saves from its
  other files, so a refresh could overwrite them. It may come later for
  games whose save folder is known, such as those Steam Cloud's rules
  name.
- **Full copies, deduplicated afterwards** (bees, in the steamshare
  write-up). A reflink copy shares its blocks from the start, with no
  daemon.
- **Cairn writing the Guardian's library list.** Steam's Storage page is
  supported and needs nothing of Cairn's. Kept as the last fallback if
  check 12 fails.
- **The shared folder as the Guardian's default library.** Simpler for a
  parent, but the Guardian's own games, and their saves, would be copied
  into every child, and Steam may put tools beside them.
- **One Linux account for games.** It gives up each child's own account
  (DESIGN §4.3).
- **For tiles:** merging kidscan's entries into the one installed manifest
  needs no launcher change, but cannot carry into Phase 1, where the image
  cannot scan a machine's games. Keeping a hand-edited manifest that
  provisioning leaves alone (RPM's `noreplace`) stops the repository's
  tile changes reaching that machine. A scan at each child's login, as the
  child, belongs with tiles per child, and changes DESIGN §3.2's "the
  Guardian runs the scanner". Desktop entries would replace the manifest
  format at a trust boundary.
