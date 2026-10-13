# One Steam library for several accounts

**Date:** 2026-10-12; laptop checks, saves and new checks 2026-10-13
**For:** issue #130, ADR-0032 (proposed) and ADR-0033 (proposed)
**Result:** Desk research, a probe on the development PC, and read-only
checks on the kids' laptop on 2026-10-13. The only change made on the
laptop was a root-only backup of the shared `Saves/` folders, and no Steam
was started for this note. Valve does not document a
way to share one library between Linux accounts. Native games shared from
one folder work on the laptop and in community reports. Three things in a
shared folder fail for every account but the one that owns their files:
Steam's runtime and Proton, a Proton game's Wine prefix, and Mesa's shader
cache. Each fails on a file mode or an owner check in upstream code, not on
a rule of Steam's. Steam has no command or link that adds a library. Its
Storage page is the only supported way, and it may refuse a folder that
already holds games. Nobody has reported a second account updating a game
in a shared folder. A folder every child can write also lets any child
change a game that the Guardian later runs as themselves. The games on
the laptop keep their saves inside the game's own folder, so in a shared
library every child shares one set of saves and can delete a sibling's.
On 2026-10-13 the maintainer chose a copy of each game for every child
instead (ADR-0033). What needs a signed-in Steam on the laptop is listed
at the end.

## Why

On 2026-10-07 the family's games reached the children's Steam on the kids'
laptop by an afternoon of hand work over ssh (#130). No family can do that.
On 2026-10-12 the maintainer decided that the parent installs a game once,
from their own Steam in their own login, and that no child's Steam
downloads a game of its own (ADR-0032, Context). The first plan was one
library that every child's Steam lists. This note collects what is known
for ADR-0033, which gives each child their own copy instead.

## What the laptop has

From #130 and its comments:

- A group, `cairn-steam`, holds the Guardian, both children and the L2
  test account (laptop, 2026-10-13). The folder
  `/var/lib/cairn/steam-library` is setgid, with a default ACL
  `g:cairn-steam:rwX`. The Guardian's games, `compatdata/` and
  `shadercache/` were moved into it.
- A script rewrote each account's `steamapps/libraryfolders.vdf` with that
  account's Steam stopped. The account's own folder is entry `"0"`, and the
  shared folder is entry `"1"`, listing every `appmanifest_*.acf`. The
  script also wrote a `libraryfolder.vdf` marker in the shared folder.
  Steam then logged "Loaded 17 apps from install folder
  /var/lib/cairn/steam-library" and downloaded nothing.
- A native game started from a child's tile through Steam, and the tiles
  came back when it quit. On 2026-10-11 a Steam account that is a child
  member of the family started a game the parent's account had installed
  (ADR-0032, Context).
- Steam's runtime in the shared folder started in 18.3 s for an account
  that did not own its files, against 2.2 to 3.2 s for its owner.
  pressure-vessel tries to `chmod` and set the mtime of about 8,500 files,
  and each try fails for a file another account owns. When the owner ran a
  game, the same `chmod` set the shared files' ACL mask to `r--`.
- So Steam's tools (Steam Linux Runtime 1.0, 2.0 and 4.0, Proton
  Experimental, Steamworks Common Redistributables) now sit in each
  account's own library. They were copied with `cp --reflink=always`, and
  `btrfs filesystem du` shows 0 B exclusive. Only games stay shared.
- The one Proton game, Pep's Birthday Surprise (294700), stopped for every
  child but its owner with
  `wine: '/var/lib/cairn/steam-library/steamapps/compatdata/294700/pfx' is
  not owned by you`. It is now copied into each account's own library with
  its `compatdata/294700`, by reflink. It gets past that check, then quits
  about 2 s after its window appears, for a reason not yet found.
- Mesa's shader cache fails for every child after the first:
  `Failed to create …/shadercache/294690/mesa_shader_cache_sf/…`. That game
  runs without a cache. This is still open.
- A game starts about 7 s after the tap: 4.7 s in Steam, of which 3 s is
  `CheckShaderDepotManifest` (shader pre-caching), 2.1 s for the container
  and about 1 s for the game.
- The tiles were merged into `/usr/local/share/cairn/manifest.json` by
  hand, and a provisioning re-run puts the stock manifest back.
- Not yet seen there: a game update in the shared folder.

## How this was checked

- Valve's own help page, Valve developers' comments on the Steam for Linux
  and Proton trackers, and the source of Proton 10.0, Wine, Mesa and
  steam-runtime-tools. Community threads (Universal Blue, CachyOS,
  Manjaro, the Steam forums), the Arch Wiki and Bazzite's docs. Two
  third-party parsers of Steam's files (steamlocate, GameFinder) and
  Lutris's notes.
- Several community guides say they were written with an AI's help. They
  are marked where cited, and nothing below rests on them alone.
- A probe on the development PC (Fedora 44, tmpfs) tried POSIX default
  ACLs with the create modes below. Steam's files there were read, not
  changed. The probe is in the maintainer's work area, not the repository.
- Every finding was checked a second time against its source. Where the
  second pass corrected the first, this note follows the second.

## How Steam keeps its list of libraries

- Each Linux account's Steam keeps its own list, and no account's Steam
  reads another's. The list is in two files,
  `~/.local/share/Steam/config/libraryfolders.vdf` and
  `~/.local/share/Steam/steamapps/libraryfolders.vdf`. On the development
  PC they are separate files with the same content. A Valve developer
  asked for both when debugging a lost library (steam-for-linux#8552).
  kidscan reads only the `steamapps/` copy.
- The file is Valve's KeyValues text. A root `"libraryfolders"` block holds
  numbered entries, and `"0"` is Steam's own folder. Each entry has
  `"path"`, `"label"`, `"contentid"`, `"totalsize"`,
  `"update_clean_bytes_tally"`, `"time_last_update_verified"` and an
  `"apps"` block from appid to bytes. Valve does not document it. The
  meanings come from community parsers and real files. `totalsize` can be
  `"0"` on Linux. `contentid` looks like a random 64-bit number, and what
  Steam does with it is unknown.
- Steam probably finds a library's games from its `appmanifest_*.acf`
  files, not from the `"apps"` block. The Arch Wiki says a game is known by
  its manifest in `LIBRARY/steamapps/`, whose `installdir` names its folder
  under `common/`. steamlocate reads the manifests itself because "the
  values in libraryfolders.vdf may be stale". Bazzite's docs and a family
  on Proton#4820 copy a game's `.acf` to make it show up. Whether an entry
  with an empty `"apps"` block loads the same games is untested. The
  laptop's script filled it.
- Steam writes the list itself ("Triggered async write of Steam library
  folders configuration"), so an edit has to be made with Steam stopped.
  In 2021 edits to the `config/` copy were overwritten at start, fixed in
  a client beta (steam-for-linux#7954). Later reports conflict on which
  copy wins. On the laptop, editing only the `steamapps/` copy was enough.
  Whether the laptop's script also touched `config/` is not recorded.
- steamcmd shares the client's per-user configuration and overwrites
  `libraryfolders.vdf` when run as the same user (Valve staff, Steam
  client beta discussion, 2024-09-30).
- Steam drops a library when it is told the library's drive was removed:
  "Drive removed", then "Unmounted library folder … (68 apps)". For the
  2023 reports, a Valve developer traced it to partitions unmounted at
  shutdown while Steam ran, and said a beta of 2023-08-18 fixed it. Later
  reports name other causes, one with a clean exit. A child's own Log out
  runs `steam -shutdown` first (ADR-0021). On the laptop the children
  still had the shared library four days after set-up.
- The singular `libraryfolder.vdf` at a library's root holds a
  `"libraryfolder"` block with `"contentid"` and `"label"`. Its purpose is
  undocumented. One user said Steam would not re-add a drive until that
  folder structure was back (low confidence, not re-checked).

## Adding a library the supported way

- Valve's help page: Settings › Storage, then the "+" button, after which
  "all future installations can be installed there". Moving games needs a
  second library already: pick the drive, tick the games, then Move. The
  page is generic and uses Windows paths.
- Steam's install dialog lets the player pick a library. Storage › (drive)
  › "…" › Make Default changes the default. Properties › Installed Files ›
  Move install folder moves one game. Where Steam stores the default is
  unknown. Searching the development PC's `config/` found no key.
- The client's own error strings for adding a folder: "Selected Steam
  library folder doesn't exist", "New Steam library folder must be
  empty", "… must be writeable", "… must be on a filesystem mounted with
  execute permissions", "… can't be added while downloading" and "Selected
  drive already has a Steam library folder. Steam will manage multiple
  game installs in a single folder for you." The Arch Wiki adds that the
  execute error also comes when the folder has no lowercase `steamapps`.
- One community user says the interface allows one library per
  filesystem. On Bazzite `/var` and `/var/home` are separate btrfs
  subvolumes (#130). Whether Steam counts them as one drive is untested.
- "Must be empty" may matter most. A shared folder that already holds
  games is not empty, and in 2021 a user reported Steam refusing to re-add
  a folder for that reason (steam-for-linux#7954).
- Against that, three reports describe a second Linux account adding a
  library another account had filled, through the interface: a Universal
  Blue thread in 2025-05, the reproduction steps of steam-for-linux#12030,
  and an AI-assisted guide in 2026-08 that may have added the folder while
  it was empty. A 2026-03 reply says to choose the folder in the file
  picker, not from Steam's own list. On Linux that picker needs an XDG
  desktop portal (Arch Wiki). Whether it opens inside a visit's cage is
  untested.
- There is no documented `steam://` link or command-line flag that adds a
  library. Community posts suggest the console command
  `library_folder_add <path>`. No cited post shows it working. In one
  thread it did not solve the asker's problem, and in another, in 2025, it
  printed "failed mounting that path". The Settings page calls an internal
  JavaScript function, `SteamClient.InstallFolder.AddInstallFolder`, that
  homebrew projects found. It is not a public interface.

## Whether Valve supports it

- No Valve document supports sharing one library between Linux users, and
  none forbids it in those words. So any Steam update can change what
  Cairn would rely on (ROADMAP §4, "Steam changes client behaviour under
  us").
- The quote most often cited against it, "You're not intended to share
  Steam Library folders between users", is from a CodeWeavers Proton
  developer, not Valve, in 2021, and opens "I'm pretty sure this is a Steam
  client limitation" (Proton#4820). In the same thread a Valve member
  said only that Wine's ownership rule is upstream behaviour.
- The release-notes line "Application files installed by Steam for Linux
  should not be shared (e.g. between Windows and Linux)" dates from 2013.
  A Valve member applied it in 2025 to a library shared between operating
  systems (steam-for-linux#10358).
- steam-for-linux#1983 ("not promising multi-user support", 2013) is about
  two users running Steam at the same time.
- Requests are open: duplicate downloads per user (#3942, 2015),
  per-user `compatdata` (#6958, 2020) and #12030 (2025). A 2025 report
  says Repair Library does nothing on Linux, because it calls
  `bin\steamservice.exe` (#12058, open).

## File modes in a shared folder

The probe, under a directory with a default ACL for a group:

| What made the file or folder | Group's effective access |
|---|---|
| `open()` with 0644 | `r--` (mask `r--`) |
| `open()` with 0600 | none (mask `---`) |
| `mkdir()` with 0700 | none (mask `---`) |
| `open()` with 0666 under umask 0077 | `rw-` |
| `chmod()` to 0755 afterwards | `r-x`, and setgid cleared |
| A file renamed in from outside the tree | 0644, no ACL, creator's group |
| Python `copy2` | `r--` |
| A subfolder under one that lost setgid | creator's group, but the named-group entry kept |

- Under a default ACL the umask does not matter, but the mode a program
  asks for does. On Fedora 44 the session umask is 0022. Steam's launcher
  sets 0077 only while it unpacks its bootstrap, then puts it back
  (`bin_steam.sh`, `steam.sh`). Bazzite is not checked.
- No source says which modes Steam's content system uses for game files
  and folders. Bazzite's docs show a Steam-written `.acf` at 0755, and
  Steam's `.vdf` files on the development PC are 0755. If Steam creates or
  sets data files to 0755, their mask is `r-x`. Other accounts could then
  not write such a file in place, but could replace it, since they can
  write its folder.
- Valve says SteamPipe "builds the new version alongside the old
  version… then commits the update by deleting old files and moving the
  new files in". That paragraph is about pack files. That every update
  works this way is an inference. A file made inside the tree and renamed
  within it keeps its inherited ACL (probe).
- Never `chmod -R g+s`. steam-runtime-tools treats a process as setuid when
  the kernel sets `AT_SECURE`, or when its effective and real group
  differ. Running a file that has setgid and a group other than the
  caller's sets `AT_SECURE`, and the runtime aborts in
  `_srt_check_not_setuid`. One family hit it (Universal Blue thread, post
  8, AI-assisted). The mechanism is in the source. setgid goes on
  directories only.
- The community's recipe for native games is a group, setgid on
  directories and a default ACL. No cited source shows a second account
  updating a game the first installed. The post that says "user A
  installs, user B updates" was written with an AI's help. The CachyOS
  poster who proposed the ACL said "I haven't tried with Steam". A Manjaro
  user's updates broke on file permissions, and that thread closed with no
  fix.

## Who writes into a shared library

- A child's Steam probably writes there. A Valve employee wrote "Steam
  will always ensure a game is up to date before launching", and "Only
  update this game when I launch it" only delays an update. That quote is
  second-hand, from about 2014, reposted in 2019. So when the Guardian's
  Steam is not running, the first child to start an updated game likely
  downloads the update into the shared folder as that child. That is
  inferred, not seen.
- An appmanifest's `LastOwner` is a SteamID, "usually the last account
  that installed and launched the app". So a launch probably rewrites the
  shared manifest. No source shows Steam refusing a game whose
  `LastOwner` is another account.
- `StateFlags` is a bitfield: 1 Uninstalled, 2 UpdateRequired, 4
  FullyInstalled, 32 FilesMissing, 64 AppRunning, 256 UpdateRunning, 1024
  UpdateStarted, 1048576 Downloading, and others. 6 is installed with an
  update pending. `AutoUpdateBehavior` is 0 keep up to date, 1 only on
  launch, 2 high priority.
- More than one account can have a session at once: `steam-visit.md` saw
  a child log in during a visit. So two accounts' Steam can run against
  the shared folder together. Nothing has been seen of that.
- Uninstalling a game also deletes its `compatdata/<appid>` and
  `shadercache/<appid>` (steam-for-linux#13640, log lines). So any account
  whose Steam can write the shared folder can uninstall a shared game for
  everyone. That last step is inferred: the report is from one account. At
  L1 and L2 Steam's windows are hidden. At L3 and L4 the child sees Steam.
  The same open report has Steam then deleting most of a `steamapps/` tree
  and crashing.
- One CachyOS family deletes the numbered folders in `steamapps/temp`
  whenever a different user logs in, "Otherwise, Steam will get all
  confused again". The "missing file permissions" error often quoted with
  it comes from a Windows and Linux dual-boot (steam-for-linux#10358),
  closed as unsupported sharing between operating systems.
- `steamapps/workshop/appworkshop_<appid>.acf` is one file per game, and
  each item in it records the SteamID that subscribed. One shared file
  would mix several accounts' items. No source says what one account's
  Steam does to another's.
- Some games write settings, and sometimes saves, into their own folder.
  In a shared folder every child then shares them. A file one child
  creates with 0644 is read-only to the others.

## Steam Families

- Steam Families shares licences, not files. A child member plays the
  eligible games of other members, with their own saves, achievements and
  workshop files. With one copy in the family, one member plays it at a
  time. Adults choose which games each child can access. Family-library
  games play offline. Games that need a third-party key or account, or
  whose developer opted out, are left out (Valve, 2024-09-11).
- Valve says nothing about the files. A native game installed once played
  from another member's account in community reports (only Proton broke,
  on prefix ownership), and on the laptop on 2026-10-11.

## Steam's tools in a shared folder

- pressure-vessel's `chmod` and mtime failures on another account's files
  are warnings ("assuming R_OK is close enough"). They make the start
  slow, as on the laptop, but do not stop it.
- Proton is worse. After each Proton install or update, its
  `steampipe_fixups` step strips write bits from a list of its own files
  with `chmod`. Only the owner may `chmod`, the error is not caught, and
  Proton stops with "PermissionError: [Errno 1] Operation not permitted".
  It records success only after a full run. So for every account but the
  owner, every Proton game fails until the owner, or root, has started
  one after that update (Proton 10.0 source; Universal Blue thread, posts
  8 and 9, with the same pressure-vessel warnings).
- Steam may install a missing tool into the library of the game that needs
  it, not the default library. One user reported it in 2022 and 2023. If
  so, a game in the shared folder could pull a runtime in after it, with
  the 18 s start. Low confidence.
- This supports the laptop's choice: tools per account.

## Proton games

- Steam sets `STEAM_COMPAT_DATA_PATH` to
  `<library>/steamapps/compatdata/<appid>`, the library the game is in.
  Proton puts the Wine prefix in its `pfx` folder, and keeps `version`,
  `config_info`, `tracked_files` and `pfx.lock` beside it. Valve's
  compat-tool document defines the variable and `pfx`. That it follows
  the game's library comes from steam-for-linux#12030 and the laptop's
  error.
- Wine refuses a prefix another account owns. In
  `dlls/ntdll/unix/server.c` it changes into the prefix, runs `stat(".")`
  and stops with "'%s' is not owned by you" when the owner is not the
  caller. With no prefix yet, it checks the parent folder instead. Because
  it checks after changing into it, a symlink to a folder the caller owns
  passes. The game simply does not start, which on the laptop was a tile
  that did nothing.
- No Steam setting moves `compatdata`. Requests for per-user `compatdata`
  are open (#6958, #12030), and a Proton pull request for per-user
  prefixes (#4861, 2021) is unmerged.
- Steam's Move of a Proton game has an open bug: it ends with "content
  file locked" and is rolled back. Reports run to 2026-10-06, one on
  Bazzite in 2026-09.
- A per-game launch option, `STEAM_COMPAT_DATA_PATH=<folder> %command%`,
  should move the prefix, since `%command%` includes pressure-vessel,
  which mounts that path. Valve says launch options are not used for
  install scripts, so a Windows game's first-run steps would still use the
  shared `compatdata/<appid>`. Launch options are per Steam account, kept
  in each home's `localconfig.vdf`. The one family that tried it reported
  failure with a malformed path. Steam runs launch options with `/bin/sh`
  (Arch Wiki), so `$HOME` should expand. Untested.
- systemd can mount one folder over another for one unit's processes
  (`BindPaths=`). In a user's own service manager that needs
  `PrivateUsers=true` (`systemd.exec(5)`, systemd 259). Nobody has
  reported trying it with Steam.
- A Steam whose library holds a game's files but no manifest for it can
  adopt them. Pressing Install makes Steam verify the files in `common/`
  and write its own manifest, with no download (two families, Proton#4820
  and the Universal Blue thread).
- On the laptop, a copy per account, owned by that account, got past
  Wine's check.

## Shader caches

- Mesa cannot share a cache folder between accounts. `disk_cache_os.c`
  makes cache folders with `mkdir(path, 0700)`, which masks any
  default-ACL group entry. It makes cache files with 0644 and opens the
  single-file cache with 0644. So even a folder made in advance with group
  access leaves the other accounts unable to write. That matches the
  laptop.
- The folder name `mesa_shader_cache_sf` means Mesa was in single-file
  mode with `MESA_SHADER_CACHE_DIR` at the library's
  `shadercache/<appid>`. For a native game that can only come from the
  environment Steam gives it, as part of its shader pre-caching, which
  gained single-file support in a 2021 client beta (inferred). Mesa does
  not read `STEAM_COMPAT_SHADER_PATH`, which is for compat tools. Reading
  a game's `/proc/<pid>/environ` on the laptop would confirm it.
- A Universal Blue guide says shader caches "share fine", and Bazzite's
  docs link `shadercache/` to the first user's library. The guide was
  written with an AI's help. Both conflict with Mesa's code and the
  laptop.
- Untested: whether turning off Shader Pre-Caching in a child's Steam stops
  Steam setting those variables, and whether a launch option that sets
  `MESA_SHADER_CACHE_DIR` wins inside the container.

## What other families do

| Approach | How | What was reported |
|---|---|---|
| Switch a link at login | A script points the shared `compatdata/` at the active user's own | Works for one active user at a time. A broken link makes Proton fail on `pfx.lock`. The systemd timer version is an AI-assisted forum post. |
| `pam_namespace` | An init script bind-mounts each user's `compatdata/` over the shared one in that session | Targets Debian. No word on SELinux, shader caches, or two users at once. |
| A library per user with links | Each user's own library has a real `compatdata/`; `common/` and `shadercache/` link to the first user's; manifests copied or linked (Bazzite docs) | A community member's troubleshooting note, not a tested Bazzite layout. Linking `shadercache/` keeps Mesa's problem. |
| Share only `common/` | Each user's library links `common/` to a shared one; Install only verifies (Proton#4820) | One user saw a full download after files went missing and came back. Sharing the runtime and Proton "doesn't work". |
| Full copies, deduplicated | Each user installs in a btrfs subvolume; bees deduplicates ("509 GiB apparent, 208 GiB on disk") | The author tried six linked layouts first. Linked manifests let users' state interfere, copied ones lost games, and linking all of `common/` pulled the runtime and Proton into shared storage. |
| Give up | One Linux account for games, switching Steam accounts | Several authors call the linked layouts brittle. |

## A folder every child can write

- Write and search permission on a folder are enough to replace any file
  in it by rename, whatever the file's own mode (`rename(2)`,
  `unlink(2)`). In the shared tree every folder gives the group `rwx`.
- So any process of a child can swap a game's program or library: a shell
  at L3 or L4, or a game or mod the child runs. When the Guardian next
  starts that game from their own login, the swapped code runs as the
  Guardian, an administrator. ADR-0032 took care to close another way
  from a child's account to the Guardian's.
- The same lets one child change a game a sibling runs.
- `d:o::---` does not help, because the children are in the group.
- Whether the Guardian's Steam runs any program from a native game's
  folder while it installs or updates it is not known.
- The tile scan reads the same tree. kidscan copies a manifest's `name`
  into a tile's title and its `appid` into `steam -applaunch <appid>`,
  with no check that the appid is a number. A manifest a child plants in
  the shared folder therefore reaches every child's tiles.

## Where the games keep their saves

Found on 2026-10-13, after the maintainer asked whether one child could
delete another's saves, and read on the laptop the same day without
changing anything.

- Steam's app data gives all 12 games on the laptop the same cloud-save
  rule: root `gameinstall`, path `Saves`, pattern `*.*`. That is a `Saves`
  folder inside the game's own folder. No rule puts a Steam account in the
  path, which is how Valve tells developers to keep each Steam user's
  files apart on one computer.
- The 11 native games are Humongous games that Steam runs through a
  ScummVM bundled with each game. On Linux, Steam starts
  `ScummVM_Linux/launch_<name>.sh` with no arguments. On Windows and Mac
  it passes `-c scummvm-<name>.ini <target>`. Their Linux depots ship an
  empty `Saves` folder beside `ScummVM_Linux`. PCGamingWiki gives
  `<game>\Saves` for the Windows saves. The publishers' updates of 2018
  and 2019 promise cloud saves on "Windows, MacOS or GNU/Linux".
- On the laptop three games were read: Joins the Parade, Saves the Zoo
  and Big Thinkers Kindergarten. Each one's Linux script runs the bundled
  ScummVM with `-c scummvm-<name>.ini <target>`, and the ini, in the
  game's folder, says `savepath=../Saves/`. Their bundled ScummVM is
  1.7.0, or 2.0.0 for Saves the Zoo. The other eight share the cloud rule
  and the 2014 build, so they are expected to match.
- On 2026-10-13 the shared `Saves` folders of three games held one
  child's saves, mode 0664 with group `cairn-steam`. The other child could
  replace or delete them. The Steam Cloud log of the child who made them
  showed the same files in sync with the Steam account both children use.
  The children's homes held no saves, only ScummVM's logs.
- In the shared library that is one folder per game for every child. The
  games save through their own save screens, into files named
  `<target>-<file>`, such as `puttputt-puttputt.sg7`. The names are the
  same for every child, so a sibling's save replaces the file. ScummVM's
  own menu cannot load, save or delete in these games (HE version 62 and
  later). Any child's process can still delete or replace the files, since
  the folder gives the group write. Whether the games' own screens can
  delete a save is not known.
- Pep's Birthday Surprise, the Windows game, saves in `<game>/SaveGames`
  (PCGamingWiki, and the L2 test account's own copy on the laptop). Its
  cloud rule names `Saves`, where Steam found no files on exit, so its
  saves never reach the cloud. A child's own copy keeps its saves in that
  child's home, with no other copy anywhere.
- Steam Cloud follows the Steam account. Steam syncs a game's cloud files
  before it starts and after it ends. With one Steam account for several
  children (layouts 1 and 2 in #130), the children have one set of saves
  wherever the files are. A child's Steam downloads a sibling's newer
  saves over its own before the game starts. Steam asks only when both
  sides changed between syncs (Valve's Steam Cloud FAQ), and the kiosk
  hides that dialog like the others (ADR-0030; inferred).
- With a Steam account per child and one shared `Saves` folder, Steam
  marks the files with the account that wrote them
  (`steam_autocloud.vdf`). When another account starts the game, Steam
  moves the earlier account's files aside into the starting account's
  `userdata/` (Valve staff in a Steam Deck thread, 2023 to 2026). In
  August 2025 they said Steam would do the same for files in the game's
  folder; whether that reached the stable client is not known. On Cairn
  that is in the starting child's home, where the first child's Steam
  cannot reach them. Untested between Linux accounts.
- Turning Steam Cloud off does not part saves that share a folder. It only
  removes the copy kept off the machine. The switch belongs to the Steam
  account, not the computer (`sharedconfig.vdf`, which Steam syncs). In a
  2023 report Steam still moved one account's saves out of a shared folder
  with the cloud off, and community reports say the switch can turn
  itself back on after a restart.
- ScummVM takes its save folder from `SCUMMVM_SAVEPATH`, which beats the
  ini's `savepath` and loses to `--savepath`. A folder that is missing or
  not writable is ignored with a warning, and the ini's folder is used
  instead (ScummVM 1.7.0 source). Steam passes a `VAR=value %command%`
  launch option through to the game, and the runtime's container shares
  the home. Saves moved out of `Saves` leave Steam Cloud, and with it
  Steam's handling of accounts.
- Valve documents nothing about what an uninstall removes. On Linux it has
  deleted a whole game folder, with the user's own files in it
  (steam-for-linux #8215). Windows reports differ: several found saves in
  the game's folder still there after an uninstall. The client has a
  string for deleting everything when it cannot load a game's depots,
  which may explain both. Saves inside a game's folder may go with it.

## Tiles and provisioning

- Tiles vanish because `install_launcher` installs the repository's
  `provision/manifest.json` over `/usr/local/share/cairn/manifest.json` on
  every run. `install_file` replaces any target whose bytes differ.
  `provision/README.md` says so.
- The launcher reads one manifest. The wrapper provisioning writes passes
  `--manifest /usr/local/share/cairn/manifest.json`, so every kiosk child
  gets the same tiles today. Per-child tiles do not exist yet.
- The decision to start Steam at login (ADR-0027) reads the same one file,
  separately from the tiles.
- An empty or unreadable manifest is an error today, and the launcher
  falls back to its six built-in tiles.
- kidscan's output loads as a manifest unchanged. Every entry carries
  `source.appid`. Footpath's doors come from the tiles, so tiles merged in
  the launcher reach the Terminal too.
- kidscan always adds the home Steam roots to any `--steam-root`, from
  `HOME`. Run with the Guardian's `HOME`, it would add the Guardian's own
  games to every child's tiles.
- kidscan never reads `StateFlags`, so a game still downloading gets a
  tile (#24).
- The launcher reads its manifest once, at start. A file written while a
  child logs in must be replaced whole, by writing a new file and renaming
  it. `install_file` does that. kidscan's `-o` does not (#24).
- On Bazzite `/usr/local` is a link to `/var/usrlocal`, so the manifest
  already lives in `/var`. The overwrite comes from provisioning owning
  the file, not from the image.
- Tiles exist only at L1 and L2. L3 and L4 children get Plasma and see
  Steam itself (`session/bin/cairn-session`).

## What must be tested on the laptop

Most checks need a signed-in Steam. Use a small game nobody minds losing
where a check changes or removes one. Stop the account's Steam where a
check says so, and copy any file before changing it.

Read first, changing nothing:

1. Each account's `config/` and `steamapps/libraryfolders.vdf`, and the
   shared marker. Whether the shared folder still has
   `appmanifest_294700.acf` beside the children's own copies, and which
   copy each child's Steam starts. This also shows what Steam does with
   one appid in two libraries.
2. `getfacl -p` on a sample of the shared tree: folders and files in a
   game, its manifest, `downloading/`, `temp/`. Owner, group and mask.
3. `grep Umask /proc/<pid>/status` for each account's Steam, and
   `/proc/<pid>/environ` of a running native game and of the Proton game:
   `MESA_SHADER_CACHE_DIR`, `MESA_DISK_CACHE_SINGLE_FILE`,
   `STEAM_COMPAT_DATA_PATH`.
4. Which of the 12 games write into their own folder: files in the
   shared `common/` owned by a child, or newer than the install. Each
   game's `Saves/` and `steam_autocloud.vdf`, one launch script and its
   ini (`savepath`), the bundled ScummVM's version, each home's
   `~/.scummvm`, each account's cloud copy
   (`userdata/<id>/<appid>/remote`) and its cloud switch.

Read on 2026-10-13, with one child's session left logged in:

- Check 1. Every account, the Guardian's, both children's and the L2 test
  account's, lists the shared folder as entry `"1"` in both copies of
  `libraryfolders.vdf`. The entry has the `contentid` of the marker
  `libraryfolder.vdf` at the top of the shared folder, and an `"apps"`
  block of the 11 native games. The Proton game is no longer in the shared
  folder: each account has its own copy. A child's Steam rewrites a shared
  game's manifest when it runs the game. A child owns 10 of the 11, and
  the one rewrite with a time came as the game quit.
- Check 2. Every folder looked at is owned by the Guardian, group
  `cairn-steam`, setgid, with `group:cairn-steam:rwx` in its access and
  default ACLs. A file a child's game writes gets read and write for the
  group. `downloading/` and `temp/` do not exist.
- Check 3. The child's Steam runs with umask 0022. No game was running.
- Check 4. In the games' own folders, only `Saves/` had files a child
  owns: see "Where the games keep their saves". The shared `compatdata/`
  and `shadercache/` were not looked at.

Then, with the maintainer's go, every shared game's `Saves/` folder was
copied with its owners and modes to
`/var/lib/cairn/saves-backup-2026-10-13`, which only root can open. A
compare found the copies the same.

Before 2026-10-13 the plan was for every child's Steam to list the shared
folder. The checks below replace the ones written for that plan.
Each one changes the laptop, so each waits for the maintainer's go and for
the children to be logged out.

Moving the laptop over:

5. With an account's Steam stopped, take the shared entry out of both
   copies of its `libraryfolders.vdf`, for every account but the
   Guardian's, the L2 test account's included. Does Steam start cleanly,
   keep its own entry, and leave the shared folder out after a restart?
6. Those accounts leave `cairn-steam`. The two top folders go to root
   and `cairn-guardian`, and everything below to the Guardian and
   `cairn-guardian`, as ADR-0033 says. The children's leftovers in the
   shared `compatdata/` and `shadercache/` go. Afterwards
   `find /var/lib/cairn/steam-library/steamapps -mindepth 1 ! -user
   <Guardian>` prints nothing, and `getfacl -R` names no `cairn-steam`.
7. As a child, with their Steam stopped, copy a native game into their
   own library by reflink: the game's folder, then its manifest. Does the
   child's Steam show the game as installed and start it from its tile,
   with no download? `btrfs filesystem du` on the copy. A save made then
   lands in the copy's `Saves/`.
8. After the move, each child's copies hold the saves the shared folder
   had, the same as the backup.
9. After check 14, the Proton game in the L2 test account. Set that
   account's own copy and its `compatdata/294700` aside first, keeping
   `SaveGames/`, then run the copy step. Does the account's Steam make a
   new prefix in its own library and reach the same point as the hand
   copy?

Steam Cloud and accounts:

10. Two children on one Steam account, as on the laptop. Does a save made
    in one child's copy reach the other's before the game starts, with a
    dialog or without? With Steam Cloud off for that game, and then for
    the account, does it stay put, and does the switch stay off after a
    restart?
11. The family has one licence for a game the Guardian's account
    installed. Does a child-member account's copy start while another
    member plays it? For a game outside that account's allow-list, does
    it show as installed, and what does `steam -applaunch` do?

The Guardian:

12. Steam's Settings › Storage in the Guardian's Steam, on a folder made
    exactly as provisioning makes it, holding an empty `steamapps/`.
    Accepted? If not, on a folder with no `steamapps/`: what modes and
    ACLs does the `steamapps/` Steam makes get? Does the file picker reach
    `/var/lib/cairn/`?
13. Install a small native game into the shared folder from Steam's
    install dialog. `getfacl` on what Steam made: can every child read
    it? Then the copy step gives every child a copy. Then set one child's
    copy aside, have the Guardian's Steam download a change to the game
    (a switch to a beta branch and back), and run the copy step during the
    download. The copy should never start, or be thrown away.
14. Move a native game, and then the Windows game, from the Guardian's own
    library into the shared folder with Steam's Move. Does the Windows
    game hit the rollback bug? `getfacl` on the result, and the copy step
    after it.
15. An update in a child's copy, with a small free game that has a public
    beta branch. In a visit, switch the child's copy to that branch, and
    close the visit before the download ends. At the child's next login,
    leave the tiles alone for 5 minutes: did Steam finish the update?
    Repeat, tapping the tile at once. What does the tile show, do the
    files in the copy's `Saves/` survive, and which manifest fields
    change?
16. In a visit, start a game whose runtime the account lacks. Does Steam
    fetch it into the account's own library?

The child:

17. A tile for a game the child has no copy of yet. What does the child's
    Steam do, and does it download anything?
18. Offline, with an update waiting for the child's copy, made as in check
    15. Does the game start?
19. Uninstall a copy from a child's Steam, in a visit. Only that copy
    goes, and the shared folder and the other children's copies stay. Do
    the files in its `Saves/` survive? Then run provisioning, and record
    what comes back.

Tiles:

20. kidscan as root with `python3 -I`, `--no-scummvm` and an empty `HOME`,
    over the shared folder only. One entry per shared game and nothing
    from any home. Pep's Birthday Surprise has one only once it is in the
    shared folder.

## Sources

Valve:

- Moving a Steam installation and games:
  https://help.steampowered.com/en/faqs/view/4BD4-4528-6B2E-8327
- "Steam Families is here", 2024-09-11, via
  https://api.steampowered.com/ISteamNews/GetNewsForApp/v2/?appid=593110
- SteamPipe: https://partner.steamgames.com/doc/sdk/uploading
- Steam for Linux release notes:
  https://github.com/ValveSoftware/steam-for-linux/blob/master/RelNotes.md
- Steam for Linux issues 1983, 3942, 6958, 7954, 8215, 8274, 8552,
  10358, 12030, 12058, 12139, 13640:
  https://github.com/ValveSoftware/steam-for-linux/issues/
- Proton issues 4820 and 9363, pull request 4861, and the `proton` and
  `steampipe_fixups.py` files of `proton_10.0`:
  https://github.com/ValveSoftware/Proton
- The compat-tool interface, `pressure-vessel/wrap-setup.c` and
  `steam-runtime-tools/utils.c`:
  https://gitlab.steamos.cloud/steamrt/steam-runtime-tools
- Steam client strings, as extracted by SteamTracking:
  https://github.com/SteamTracking/SteamTracking/blob/master/ClientExtracted/public/steamui_english.txt
- Steam client beta discussion on steamcmd and `libraryfolders.vdf`
  (2024-09-30), steamcommunity.com, SteamClientBeta discussion
  4840896974232786377.
- Steam Cloud for developers:
  https://partner.steamgames.com/doc/features/cloud
- Steam Cloud FAQ:
  https://help.steampowered.com/en/faqs/view/68D2-35AB-09A9-7678
- Steam Families FAQ:
  https://help.steampowered.com/en/faqs/view/054C-3167-DD7F-49D4
- Valve staff on Auto-Cloud and several accounts, Steam Deck thread
  4038102936190852716 (2023 to 2026):
  https://steamcommunity.com/app/1675200/discussions/1/4038102936190852716/
- Each game's app info, from a public mirror of Steam's data, for example
  https://api.steamcmd.net/v1/info/283920, and its update notes through
  https://api.steampowered.com/ISteamNews/GetNewsForApp/v2/

Upstream code and manuals:

- Wine, `dlls/ntdll/unix/server.c`: https://gitlab.winehq.org/wine/wine
- ScummVM 1.7.0, `backends/saves/posix/posix-saves.cpp`,
  `engines/scumm/saveload.cpp` and `gui/saveload-dialog.cpp`:
  https://github.com/scummvm/scummvm/tree/v1.7.0
- Mesa, `src/util/disk_cache_os.c` and `disk_cache.c`:
  https://gitlab.freedesktop.org/mesa/mesa
- Linux man pages `acl(5)`, `umask(2)`, `rename(2)`, `unlink(2)` and
  `ld.so(8)`, and `systemd.exec(5)` from systemd 259.

Distributions and wikis:

- Arch Wiki, Steam and Steam/Troubleshooting:
  https://wiki.archlinux.org/title/Steam
- PCGamingWiki's pages for the 12 games, for example
  https://www.pcgamingwiki.com/wiki/Putt-Putt_Saves_the_Zoo
- SteamDB's depot lists, as archived, for example
  https://web.archive.org/web/20250821001922/https://steamdb.info/depot/283923/
- Bazzite, "Multi-user WINE quirks":
  https://docs.bazzite.gg/Gaming/Common_gaming_issues/#multi-user-wine-quirks

Community (the AI-assisted ones are marked in the text):

- https://universal-blue.discourse.group/t/help-shared-local-steamlibrary-multi-user-setup/8388
- https://discuss.cachyos.org/t/multiple-user-profiles-on-same-pc-with-steam/27411
- https://discuss.cachyos.org/t/has-anyone-found-a-straightforward-way-to-share-a-steam-library-between-two-users/17739
- https://forum.manjaro.org/t/share-steam-library-folder-between-user-accounts/155855
- Steam forum threads on steamcommunity.com, forum 1: 3812910660674002199,
  3815159289442658016, 3175526477769628326, 3395164262984453533,
  3395163747110198261 and 3877096256098495653; forum 0:
  4406291470270383660; forum 24: 3094515496014435286 (not re-fetched).
  On saves: forum 1, 3814039462154488700 (saves moved with the cloud
  off) and 1850323802572223463 (the cloud switch).
- https://github.com/thojo0/steam-multiuser-compatdata-fix
- https://github.com/kookiebiz/steamshare
- Parsers: https://github.com/WilliamVenner/steamlocate-rs,
  https://github.com/erri120/GameFinder,
  https://github.com/aitiotekt/steamos-mount, and
  https://github.com/lutris/lutris/blob/master/docs/steam.rst
- https://github.com/SteamDeckHomebrew/decky-frontend-lib
- Phoronix on Steam's single-file Mesa cache:
  https://www.phoronix.com/news/Steam-Mesa-Single-File-Cache

Cairn: issue #130 and its comments, #24, ADR-0021, ADR-0027, ADR-0032,
`docs/research/steam-visit.md`, `provision/cairn-provision.sh`,
`tools/kidscan/kidscan.py`, `launcher/src/main.cpp`,
`session/bin/cairn-session`.
