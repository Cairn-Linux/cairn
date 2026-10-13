# ADR-0033: One Steam library for every child

**Status:** proposed. It becomes accepted when the checks under
Consequences have run on the kids' laptop and none of them overturns a
bullet of the Decision.
**Date:** 2026-10-12
**Closes:** (none) — addresses #130 and #24's question of where game tiles
live; once accepted, amends ADR-0024

## Context

On 2026-10-07 the family's games reached both children's Steam on the kids'
laptop by hand, over ssh (#130): a group, a shared folder with a default
ACL, a script that rewrote each account's library list with Steam stopped,
and tiles merged into the installed manifest by hand. Native games started
from the children's tiles, and nothing was downloaded. Three things did not
work for any account but the one that owned their files. Steam's runtime
first failed, then took 18 s to start against 2 s for its owner. The one
Proton game's Wine prefix was refused. Mesa's shader cache could not be
written. Tools and the Proton game were copied into each account's own
library by reflink, which got past the first two. The shader cache is
still open. A provisioning re-run puts the stock manifest back, and the
game tiles go.

On 2026-10-12 the maintainer decided that the parent installs a game once,
from their own Steam in their own login, into one library every child's
Steam uses, and that no child's Steam downloads a copy of its own
(ADR-0032, Context; ADR-0032 is still proposed). ADR-0032 leaves this
library to a later ADR: "Provisioning makes the folder and its group and
points each child's Steam at it." It leaves tiles that follow what each
child can play, and taking a game off a child's tiles for a while, to
others.

`docs/research/shared-steam-library.md` collects what is known. Valve
documents no way to share a library between Linux accounts, and nobody has
reported a second account updating a game in a shared folder. Steam has no
command or link that adds a library. Its Storage page may refuse a folder
that already holds games, and its file picker would ask a parent to find a
system folder. Steam rewrites its library list itself, so an edit needs
Steam stopped. Wine refuses a prefix another account owns, and Mesa makes
its cache folders 0700, so neither can be shared by permissions. Any
account that can write the shared folder can uninstall a game for everyone,
and can swap a program in it that another account later runs.

DESIGN §4.6 puts game installs in `/var`. DESIGN §3.2 gives running the
game scanner to the Guardian, and §8.7 describes kidscan. The launcher
reads one manifest, and the Steam-at-login check reads the same one
(ADR-0027).

## Decision

**The games a Guardian installs for the children go into one shared
library, from the Guardian's own Steam, and every account's Steam lists it
beside its own. Steam's tools, and games that run through Proton, stay in
each account's own library, owned by that account.** That is the laptop's
layout since 2026-10-07. Provisioning makes it, instead of a person over
ssh. Later the Guardian tool runs the same steps (#113, P1-5).

- **The group and the folder.** Provisioning makes the system group
  `cairn-steam` and puts every child and every Guardian in it. It makes
  `/var/lib/cairn/steam-library/steamapps/` and, inside it, `common`,
  `downloading`, `temp`, `shadercache`, `compatdata` and `workshop`. Each
  is owned by root and the group, mode 2770. Each gets the group in its
  access ACL, and the default ACL
  `d:u::rwx,d:g::rwx,d:g:cairn-steam:rwx,d:m::rwx,d:o::---`. So what any
  member's Steam makes stays usable by the others, whatever its umask.
  Making Steam's folders in advance means no account's Steam makes one
  with a mode that could shut the others out. The setgid bit goes on these
  directories only: on a file it makes the Steam Linux Runtime abort.
  Provisioning sets this on the folders it made and never walks the games
  inside them.
- **Provisioning points each account's Steam at it.** For every member
  whose Steam has run once, provisioning adds one entry for the shared
  folder to that account's `config/libraryfolders.vdf` and
  `steamapps/libraryfolders.vdf`. Cairn writes this file of Steam's
  because Steam offers no other way that a family can use: no command, no
  link, and a Storage page that wants a system folder found in a file
  picker, once per account, and may refuse a folder that already holds
  games. The step is kept narrow:
  - It runs only while that account's Steam is not running. For a child
    that means no process of the child at all, as the visit's helper
    checks (ADR-0032). For a Guardian it means no Steam process of
    theirs. It never starts or stops anyone's Steam, since
    `steam -shutdown` starts Steam when it is not running (ADR-0032). A
    running account gets a `note:` line and is done at the next run.
  - It runs as that account, never as root, so a link planted in the home
    cannot send the write somewhere else.
  - It adds or repairs only the entry whose path is the shared folder. It
    keeps entry `"0"`, every other entry and every key it does not know,
    and changes nothing when the entry is already right.
  - It writes a new file beside the old one and renames it into place. The
    first time, it keeps the original as `libraryfolders.vdf.cairn-before`.
    A file it cannot parse is left alone, with a sentence saying so.
  - The entry and the shared folder's `libraryfolder.vdf` marker have the
    form the laptop's script wrote, the only form seen working, read from
    the laptop in check 1. If check 5 shows an empty `"apps"` block is
    enough, the entry carries none, so Cairn never lists which games are
    installed.
  - An account whose Steam has never run has no file to change. It is
    pointed at the first run after the visit that signs it in
    (ADR-0032). `docs/TRY.md` gives the steps in that order.
  - The step is a small standard-library Python program in `provision/`
    that shares kidscan's VDF parser, with tests.
- **The Guardian installs into it from their own Steam.** Provisioning
  points the Guardian's Steam at the shared folder the same way. The
  Guardian's own folder stays Steam's default. To install a game for the
  children, the Guardian picks the shared library in Steam's install
  dialog. A native game that went to the Guardian's own folder, by mistake
  or before this, moves with Steam's own Move (Settings › Storage).
  `docs/TRY.md` shows both.
- **The Guardian does not play games from the shared library in their own
  login.** Any child can change those files. A game started from there in
  the Guardian's login would run code a child could have changed, as an
  administrator. To try a game for a child, the Guardian opens a visit
  (ADR-0032), where it runs as the child. `docs/TRY.md` says so. Nothing
  enforces it yet; see Consequences.
- **A Proton game gets a copy per child.** Steam keeps a Proton game's Wine
  prefix in the game's library, and Wine refuses a prefix another account
  owns. So for each such game, provisioning gives each child a copy in
  their own library. It runs `cp --reflink=always` as the child, with no
  process of the child running, from the shared folder's
  `common/<installdir>` and `appmanifest_<appid>.acf` into the child's own
  `steamapps/`. The copy shares the Guardian's download and its disk
  blocks until one of them changes. It leaves out `compatdata/<appid>`, so
  the child's Steam makes a new prefix the child owns at the first start.
  The shared copy stays: it is the Guardian's, the source of the copies,
  and where the tile scan finds the game. Which copy a child's Steam
  starts when both folders hold the game is check 1. Until check 19 finds
  a sign on the machine of which games run through Proton, provisioning
  takes them as a list (`--proton-game APPID`).
- **Steam's tools stay in each account's own library.** Each account's
  Steam fetches and updates its own Steam Linux Runtime, Proton and
  redistributables. `docs/TRY.md` has the Guardian start one game in a
  visit after the child's Steam is pointed at the shared folder, so the
  first fetch happens there, out of the child's sight.
- **Shader caches stay where Steam puts them.** For a shared game that is
  the shared folder's `shadercache/<appid>`, so only the first account to
  start the game gets Mesa's cache. The others run without one. If check
  20 shows that turning off Steam's Shader Pre-Caching stops Steam pointing
  Mesa there, the Guardian turns it off in each child's Steam during a
  visit, and `docs/TRY.md` says so.
- **Children keep write access to the shared folder.** A child's Steam
  updates a game before starting it and rewrites its manifest, as far as
  is known, so it needs to write. Narrowing it waits for check 21.
- **Game tiles come from the shared library and survive provisioning.**
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
    sentence in the journal. Every child can write the folder the file is
    made from, so nothing in it may choose a program.
  - On every run, provisioning runs kidscan with `--no-scummvm` over the
    shared folder only, as root with `python3 -I` and an empty `HOME`, so
    no home's Steam is read. It writes to a new file and renames it into
    `/var/lib/cairn/games.json`, root-owned, mode 0644. With no shared
    folder it leaves the file alone. The manifest in
    `/usr/local/share/cairn/` stays provisioning's, as now.
  - kidscan leaves out a game whose `StateFlags` lacks FullyInstalled (4)
    and an appid that is not a number, and gives Steam games the id
    `steam-<appid>` (#24).
  - Routing every shared game through Steam differs from DESIGN §8.2, which
    runs SCUMM titles under native ScummVM with no Steam client. #130 asks
    for Steam itself, and ScummVM's detector would parse files every child
    can write. The maintainer decides which before acceptance, and §8.2 is
    edited in the same change if Steam wins.

This ADR leaves to later ADRs, as ADR-0032 does: tiles that follow what
each child's Steam can play, and taking a game off a child's tiles for a
while.

## Consequences

- #130's acceptance: provisioning sets up the library and its group, and
  game tiles come from kidscan and survive a re-run. The same steps serve
  all three account layouts in #130, and each is written up in
  `docs/TRY.md` and tried on the laptop. Proton games are checked from two
  children and listed in `docs/TRY.md` as a known gap until check 18
  passes. The laptop's Proton game still quits after 2 s, which is a
  separate fault.
- A native game downloads once. A Proton game downloads once, then each
  child's Steam keeps its own copy up to date, so its updates download
  once per child. Steam's tools also download and update once per account,
  and take disk in each home. The laptop's reflink copies of the
  Guardian's tools saved that. They are not carried over, because a
  child's account cannot read the Guardian's folder, and root writing into
  a child's home is what the pointing step avoids.
- A new child's set-up becomes: provisioning, a visit to sign their Steam
  in, provisioning again to point it at the shared folder, and a visit
  that starts one game so Steam fetches its tools (not tried). If a
  visit's close ran the pointing step, one visit would do. That would
  amend ADR-0032's "A visit never reads or writes Steam's files".
- A shared 3D game loads slower for every child but the first until
  check 20. A 2D game does not notice. Turning off pre-caching would also
  stop Steam fetching ready-made shaders for Proton games.
- **A child-writable folder.** Any child, or any game they run, can change
  a shared game for everyone: an L3 or L4 child from a shell, at any level
  through a game or a mod. A game the Guardian starts from the shared
  library in their own login runs as the Guardian. Until checks 21 and 22
  say whether children can be read-only, or whether the Guardian's Steam
  runs anything from a game's folder, only `docs/TRY.md` stands between
  the two. The tile scan cannot carry it further: the launcher accepts
  only `steam -applaunch <number>` from it. A child who plants a manifest
  can still change what a sibling's tile says, not what it runs.
- At L3 and L4, where the child sees Steam, any child's Steam can uninstall
  a shared game, and its shader cache, for every account (inferred,
  check 17). At L1 and L2 Steam's windows are hidden.
- More than one account can have a session at once (`steam-visit.md` saw
  a child log in during a visit), so two accounts' Steam can update the
  same game. Checks 13 and 14 cover it.
- With one Steam account per child, every L1 and L2 child sees a tile for
  every shared game, as every kiosk child sees every tile today. A game
  that child's Steam account cannot play probably ends on the grown-up
  screen (ADR-0030; unchecked, check 16). Tiles per child are the later
  ADR.
- Tiles change only when provisioning runs, and a child sees them at the
  next login. A child whose Steam is not yet pointed at the shared folder
  gets the tiles anyway, and pressing one ends on the grown-up screen.
- Resetting a child's account (DESIGN §11) clears their Steam, its
  sign-in, tools, Proton copies and library list. The shared folder keeps
  the games. A visit and a provisioning run set the child up again.
  Removing a child leaves the files their Steam wrote in the shared folder,
  owned by a uid no account has, until that number is given to a new one.
- A new member of `cairn-steam` gets its access at their next login.
- Cairn now depends on a file format Valve does not document, and on Steam
  tolerating a library several accounts write. A Steam update can break
  either. Steam's own launch failures end on the grown-up screen, which is
  the fallback the ROADMAP's risk table names.
- No new dependency: `setfacl` and `cp --reflink` are in the image, and the
  step is standard-library Python. Nothing phones home. The only network
  traffic is Steam's own.
- In the same change: DESIGN §4.6 names the shared library among the game
  installs in `/var`, and §8.7 says where kidscan's output lives and who
  writes it. ADR-0024's "Where the manifest lives and who runs the scanner
  stay with #24" is answered for games, and its status becomes "amended by
  ADR-0033". ADR-0027 keeps its words: its check now reads the tiles from
  both files. ADR-0013 already means one list of tiles, since the doors
  come from the tiles. `provision/README.md`, `launcher/README.md`,
  `tools/kidscan/README.md` and `docs/TRY.md` change with it.
- **Checked before acceptance,** by number in the research note: 1, 2 and
  3 (the laptop as it is), 5 and 6 (the entry Cairn writes), 8 (a new game
  reaches every child), 10 and 13 (the Guardian's install, and updates by
  turns), 12 (a child's tools fetched in a visit), 14 and 16 (a running
  child's Steam, and a child-member account), 18 (a Proton copy with a new
  prefix) and 23 (the scan). If check 7 shows Steam's own Add Drive accepts
  the shared folder, from the file picker in a visit, this ADR is amended
  to use it, and the pointing step goes.

## Alternatives considered

- **Steam's Storage page in each account.** The supported route, and
  Cairn would write none of Steam's files. Not chosen yet: the interface
  may refuse a folder that already holds games, or a second library on
  what it takes for the same drive, and a parent would have to find a
  system folder in a file picker, once per child, in a visit. Check 7
  decides.
- **The console command `library_folder_add`.** Undocumented, and no
  report shows it working.
- **The shared library as the Guardian's default.** Simpler for a parent.
  But the Guardian's own games, and their Proton prefixes and saves, would
  land where any child can write, and Steam may put tools beside them.
- **`pam_namespace` over the shared `compatdata/` and `shadercache/`.**
  Proton games would install once too, and every account would get its
  own prefix and Mesa cache. Not chosen: untested on Bazzite and its
  SELinux, it changes the PAM stacks that ADR-0020 and ADR-0032 rely on,
  and a wrong line there stops logins. It is the first thing to try if
  per-child Proton copies cost too much.
- **A bind mount in the unit that runs Steam** (systemd's `BindPaths=`,
  which a user's own service manager allows with `PrivateUsers=true`).
  It reaches only the Steam that Cairn starts, not the Guardian's or an
  L3 or L4 child's, and Steam inside a user namespace is untested.
- **A `STEAM_COMPAT_DATA_PATH` launch option per game per child.** Valve
  says launch options are not used for install scripts, so first-run steps
  would still meet Wine's refusal. It is also one setting per game, per
  child, per Steam account.
- **A link to the active user's `compatdata/`, switched at login.** Right
  for one account at a time, and more than one can be logged in.
- **A library per account with each game linked into the shared one** (the
  layout in Bazzite's docs). Each Steam keeps its own build number, so one
  account's update leaves the others' stale. The steamshare author gave up
  on these layouts after six tries.
- **A reflink copy of every game for every account.** Every file would
  have one owner: no ACLs, no prefix or cache problem, and nothing a child
  can change that the Guardian runs. But each update needs a new copy
  before the children play, or a download per child, and it is no longer
  one library every child's Steam uses. It is the fallback if checks 13,
  14 or 21 go badly.
- **Children read-only in the shared folder.** It would close the path to
  the Guardian, and stop a child uninstalling a game for everyone.
  Untested: Steam writes manifests, `temp/` and caches there, and may drop
  a library it cannot write.
- **A root pass that repairs the ACLs.** Only if check 2 or 13 shows Steam
  narrowing them. Root walking a tree children can write invites link
  races.
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
