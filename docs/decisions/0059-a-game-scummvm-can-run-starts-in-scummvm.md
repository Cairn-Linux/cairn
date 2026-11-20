# ADR-0059: A game ScummVM can run starts in ScummVM

**Status:** proposed. It becomes accepted when checks V1 to V12 under
"Checks" have run in the cairn-min VM, on an image built with every
slice under "Slices", and checks K1 to K5 have run on the kids' laptop,
and none of them overturns a bullet of the Decision.
**Date:** 2026-11-20
**Closes:** (none) — settles the choice ADR-0033 left to the maintainer,
Steam or native ScummVM (ADR-0033, Decision, lines 222-228): ScummVM.
Part of ROADMAP P1-13. Addresses #215 in part.
**Amends:** ADR-0033 (a games entry may carry a ScummVM key beside its
Steam command, and kidscan runs ScummVM's detection on the image);
ADR-0041 (a ScummVM game is given without a copy, and every Save, Look
and first start after an image update brings each child's list up to
date); ADR-0027 (a note only: the rule stands, and the launcher knows
two launch forms). Adds a build check to ADR-0038's image. DESIGN §8.2,
§8.3, §8.4, §8.7 and §11 change in the same change.

## Context

The maintainer decided on 2026-11-20 (spelling tidied):

> I wanted to push our steam capabilities and make sure we can try the
> more difficult implementation. We should probably default to scummvm
> since it has better compatibility and just use other steam only games
> to test our proton functionality. The goal is to make it as easy as
> possible for parents who come from windows or mac and don't know how
> to troubleshoot these types of issues.

DESIGN §8.2 has said it from the start: a SCUMM game becomes a tile
that starts native ScummVM, with no Steam client on the child's side.
The build does not follow it. Every game starts through the child's own
Steam:

- The launcher takes a games entry only when its command is
  `steam -applaunch <appid>` (ADR-0033, lines 207-210;
  `launcher/src/GamesFile.cpp:24-34`).
- Cairn runs kidscan with `--no-scummvm` (ADR-0033, line 211;
  `guardian/src/CatalogueRefresh.cpp:49`;
  `provision/cairn-provision.sh:902`).
- ADR-0033 left the choice to the maintainer before acceptance.

### What Steam for every game costs

Steam stands between a child's tap and every game. Much of the
launcher exists only to wait for it or to explain it:

- Steam must start at login, and takes about a gigabyte of memory
  (ADR-0027). Its own start took 10 to 14 s in the VM (ADR-0057,
  Record).
- The launcher waits up to two minutes while Steam starts (ADR-0029),
  and up to ten while Steam gets a game ready (ADR-0036). It starts a
  stalled Steam again on a new child's first game (ADR-0036, #206). It
  sends a tap again after Steam updates itself, and hides Steam's
  updater (ADR-0037). It ends a launch from Steam's log (ADR-0057).
- The grown-up screen has lines only Steam needs: log out and back in
  (ADR-0045), Steam's cloud question while offline (ADR-0045, #211),
  and a sign-in that was turned down (ADR-0049).
- A new child's Steam must be signed in once, from a grown-up's login,
  before any game can be copied to them (ADR-0032, ADR-0049; the copy
  step's `no-library`, ADR-0041). Steam's tools are copied with each
  child's games (ADR-0046). A grown-up updates a child's Steam out of
  sight (ADR-0048).
- Siblings who share one Steam account share their saves: Steam Cloud
  carries one child's newest save into the other's copy (ADR-0033,
  Consequences).
- Some storybook games open in a small window because Steam starts
  them from the wrong folder (#184).

When any of this goes wrong, the fix is in Steam's own windows. The
kiosk hides those from the child, and a parent who comes from Windows
or a Mac does not know them.

For the 11 Humongous games, Steam adds nothing the game needs. Steam
starts a copy of ScummVM bundled inside each game, 1.7.0, or 2.0.0 for
Saves the Zoo (`docs/research/shared-steam-library.md:441-446`).

### What is already known to work

- On 2026-09-30, in the VM, kidscan sent Putt-Putt Joins the Parade to
  native ScummVM; which version was not written down. Its tile opened
  the game fullscreen with no Steam client, and Super+Q went back to the
  tiles (ADR-0024, Consequences). Ctrl-Alt-Home ended it in its own
  scope (ADR-0025).
- The launcher already takes its plain path for any command that is
  not `steam -applaunch`, as it does for Tux Paint
  (`launcher/src/SteamGameWait.cpp:22-43`).
- The image installs Fedora's ScummVM, 2.9.1-5 on Fedora 45, at
  `/usr/bin/scummvm` (`build_files/packages.sh:14,30`; sources at the
  end).
- kidscan already writes a native ScummVM entry when ScummVM recognises
  a game (`tools/kidscan/kidscan.py:393-447`).

### Which of the family's 25 games ScummVM can run

From ScummVM's compatibility list and source (2.9.1 and 2026.4.0) and
Steam's store data, read on 2026-11-20 (Sources, at the end):

- **11 Humongous games** use ScummVM's SCUMM engine, rated Excellent:
  the nine Putt-Putt games other than Pep's Birthday Surprise, and the
  two Big Thinkers. Only Joins the Parade has been seen in native
  ScummVM on Cairn.
- **13 storybook games** (the Living Books and D.W.). ScummVM's Mohawk
  engine knows nine of them, rated Good. But it recognises such a game
  only by the exact checksums of its files. Whether 2.9.1 knows the
  Steam editions' files is unconfirmed. The Tortoise and the Hare's
  Steam files are known only from ScummVM 2026.1.0. ScummVM marks
  Reading Race, Computer Adventure and Maggie's Farmyard Adventure
  unstable, and the 2.0 editions of Arthur's Birthday and Just Grandma
  and Me too. D.W. The Picky Eater needs an engine 2.9.1 does not have.
- **Pep's Birthday Surprise** uses another engine. ScummVM cannot run
  it.
- Steam's store data lists eight of the storybook games, and Pep's, as
  Windows-only: Pep's Birthday Surprise (294700), Arthur's Birthday
  (2471140), Arthur's Reading Race (2471160), Arthur's Computer
  Adventure (2471170), D.W. The Picky Eater (2742020), The Tortoise and
  the Hare (2785120), The Berenstain Bears In the Dark (3319650), Just
  Grandma and Me (3853780) and Maggie's Farmyard Adventure (3996530).
  So Steam runs nine of the 25 through Proton today. The other five
  storybook games run a ScummVM that ZOOM bundled with them.

ScummVM asks "Start anyway / Cancel" before a game it marks unstable,
unless a setting turns the question off (2.9.1
`engines/advancedDetector.cpp:1153-1165`). Its `--detect` lists those
games like any other and prints no support level (2.9.1
`base/commandLine.cpp:1517-1544`). No SCUMM game is marked so today.
For a version of a game it does not know, SCUMM only writes a line to
its log (2026.4.0 `engines/scumm/metaengine.cpp:359-372`). But 2026.4.0
can mark a SCUMM game unstable or testing
(`engines/scumm/detection.cpp:244-247`), though no entry uses it yet.

`--detect` exits 0 when it finds no game (2.9.1
`base/commandLine.cpp:2006-2009`). That happens, for example, while a
Guardian's Steam is part-way through updating a game's folder.

SCUMM game ids are not all lowercase: `PuttsFunShop`, `BluesBirthday`,
`SoccerMLS` and `Baseball2003` are in 2.9.1's tables
(`engines/scumm/detection_tables.h:86-122`). Four SCUMM games have
network play: Backyard Football, Backyard Baseball 2001 and Backyard
Football 2002 since 2.8.0, and Moonbase Commander, whose multiplayer
2.9.1 fixed (release notes). None is among the family's 25.

### Where the children's progress is

- In each of the 11 Humongous games, each child's progress is in their
  own Steam copy, in the game's `Saves` folder. The games save through
  their own save screens, into files whose names start with ScummVM's
  name for the game, for example `puttputt-puttputt.sg7`
  (`docs/research/shared-steam-library.md:452-458`; 2.9.1
  `engines/scumm/he/script_v60he.cpp:145-154`). ScummVM's own menu
  cannot save in these games (HE version 62 and later).
- In the shared era, the family folder's own `Saves` held one child's
  saves (`shared-steam-library.md:447-451`). Each child's copy was made
  from that folder, and the copy step leaves out only
  `steam_autocloud.vdf`. So a child's copy may hold a sibling's old
  save.
- The game writes those files, not ScummVM. So a file written by native
  ScummVM 2.9.1 probably loads in the bundled 1.7.0 too, and the other
  way round. This is unconfirmed; check V6 tries both directions.
  (ScummVM's own save states, versions 96, 98 and 121 in
  `engines/scumm/saveload.cpp`, do go one way only, but these games do
  not use them.)
- ScummVM's storybook engine saves nothing. Pep's saves in its own
  `SaveGames` folder, and Pep's stays on Steam.
- If ScummVM's settings already hold a game of the same name, a game
  started from the command line gets the name `<id>-1`, and its old
  saves are not found (2.9.1 `base/commandLine.cpp`,
  `base/plugins.cpp:728-735`).
- `SCUMMVM_SAVEPATH` already wins over a game's own save folder in
  2.9.1: ScummVM puts it in its transient settings, which come before a
  game's section (2.9.1 `backends/saves/posix/posix-saves.cpp:110-120`).
- ScummVM's default save folder is `~/.scummvm` when that folder
  exists, and otherwise `~/.local/share/scummvm/saves` (2.9.1
  `posix-saves.cpp:58-95`). Steam's bundled 1.7.0 left its logs in these
  homes (`shared-steam-library.md:451`); whether it made `~/.scummvm` is
  unconfirmed.

### What ScummVM's own windows reach

F5, Ctrl+F5, a controller's Start button and a controller's left
shoulder open ScummVM's menu. Ctrl+F5 and Start are in ScummVM's global
key map (2.9.1 `backends/events/default/default-events.cpp:335-338`).
F5 and the left shoulder are in each game's default key map
(`engines/metaengine.cpp:85-88`). Emptying that key map's setting takes
away the left shoulder but not F5 in a SCUMM game: the SCUMM engine
reads the raw F5 key itself (`engines/scumm/input.cpp:1359-1363`). The
kiosk already keeps Ctrl+F5 from every app, and plain F5 still reaches
the menu (#215, `docs/research/kiosk-containment.md`).

The menu offers Resume, Options (the game's sound, keys and a few game
settings), About, Help, Return to Launcher and Quit (2.9.1
`engines/dialogs.cpp:75-140`, 286-381). No setting removes Return to
Launcher (2026.4.0 `engines/dialogs.cpp:86-91`).

Return to Launcher shows ScummVM's own game list (#215; ADR-0041,
lines 457-470). The same list comes, with no key pressed, after ScummVM
shows "Error running game" or "Could not find any engine capable of
running the selected game" (2.9.1 `base/main.cpp:818-881`). Fedora's
build is made with `--enable-cloud --enable-libcurl --enable-plugins
--default-dynamic --enable-text-console`, and needs `gtk3-devel` to
build (Fedora's `scummvm.spec` for f45, lines 26-34 and 97-108). From
that list a child can:

- start another game ScummVM knows, or add one with Add Game, which
  opens GTK's file chooser (2.9.1 `gui/browser.cpp:85-100`). Whether
  that chooser reaches network places through GVfs is unconfirmed.
- connect a cloud store under Options, which opens
  `https://cloud.scummvm.org/` in a browser through `xdg-open` (2.9.1
  `gui/cloudconnectionwizard.cpp:614`,
  `backends/platform/sdl/posix/posix.cpp:419-433`). What `xdg-open`
  opens inside the kiosk is unconfirmed.
- press Download Icons or Download Shaders under Options, which fetch
  from scummvm.org inside ScummVM's own window (2.9.1
  `gui/options.cpp:1699-1700, 2728-2729`). The kiosk sees no new window.

ScummVM's LAN file server needs SDL2_net, which is not among the
spec's build dependencies, so it is probably not built (unconfirmed).

ScummVM sets its window title back to its own name,
`gScummVMFullVersion`, before it shows its list (2.9.1
`base/main.cpp:369, 878-880`). It sets the game's name as the title only
once the game has been found (`base/main.cpp:246-253`). An error before
then leaves its own name as the title throughout. The launcher already
learns each window's title through foreign-toplevel
(`launcher/src/ForeignToplevel.cpp:40-42`), but passes it on only when
the window opens (`launcher/src/ForeignToplevelList.cpp:32-38`).

Today ScummVM's settings live inside each game's Steam folder. With
native ScummVM, Cairn writes them.

### What a rollback would meet

DESIGN §10 calls rollback "a documented, supported, one-command parent
operation". An older launcher takes only `steam -applaunch` entries,
and an older Family finds a game's app id only there
(`guardian/src/GameCatalogue.cpp:32-43`). A Save in an older Family
drops an entry without an app id and does not count it as removed
(`guardian/src/ChildGameList.cpp:47-48`). So an entry whose command is
anything else would lose its tile after a rollback, and the next Save
would take it off the child's list for good.

## Decision

**A game that the image's ScummVM recognises, and starts without
stopping to ask, starts in that ScummVM straight from its tile. It
reads the game from the family folder, and Steam plays no part. Every
other game starts through the child's own Steam, as today. The
parent's steps do not change, and nothing new needs pressing after an
update. Each child's progress in a ScummVM game is kept in their own
home. What they made in their Steam copy is copied there the first
time, and never moved or deleted. When ScummVM leaves the game for its
own windows, the launcher ends it.**

### Which games start in ScummVM

- kidscan decides, when the family's full games list is made: at each
  Save and each Look for new games on Family's Games page
  (`cairn-family save-games`, ADR-0041), at the first start after an
  image update (below), and in the family's set-up (`make_game_tiles`).
- It runs the image's ScummVM, `/usr/bin/scummvm --detect --path=<game
  folder>`, once for each game in
  `/var/lib/cairn/steam-library/steamapps/common`, as it does today.
- A game gets a ScummVM entry when all of these hold:
  - ScummVM names exactly one game at the top of its folder. Two or
    more, such as a demo beside the game, give the Steam entry.
  - ScummVM starts that game without a question. That is every game of
    the SCUMM engine, and a storybook game only when its ScummVM id is
    on kidscan's short list of games whose Steam edition was seen
    starting cleanly in the image's ScummVM. The list starts empty. A
    game joins it in its own change, after a check (What comes later).
  - Its id is not on kidscan's list of games that stay on Steam. That
    list starts with the four SCUMM games with network play:
    `football`, `baseball2001`, `football2002` and `moonbase`. A game
    leaves it only after a look at its network play under #36.
  - Its ScummVM id and folder name pass the rules under "What the
    games file says".
  - Neither `steamapps/common` nor the game's folder is a link.
- Any other game gets the Steam entry it gets today.
- So of the family's 25 games, the 11 Humongous games are expected to
  start in ScummVM; check K3 confirms which. The 13 storybook games and
  Pep's stay on Steam, nine of them through Proton.

### A game never goes back to Steam by itself

A detection can fail or find nothing for reasons that say nothing
about the game. A game that went back to Steam would put the child back
on their old Steam progress, and anything saved there would be left
behind when it came back. So, in the run Cairn makes:

- kidscan reads the list it wrote last. A game that had a ScummVM entry
  there keeps it, with the same id and folder, while its manifest and
  folder are still there and its id is not on the stay-on-Steam list,
  whatever this run's detection said: found nothing, timed out, crashed
  or exited non-zero.
- A game new to the list whose detection fails gets the Steam entry,
  with a warning. It has no ScummVM progress to leave behind, and a
  later run moves it to ScummVM once the detection works, which copies
  its Steam progress over.
- When the last list is there but cannot be read, kidscan ends with an
  error. So does a run that passes its time limit. Family then keeps
  the full games list it had, as it already does when kidscan fails
  (`guardian/src/CatalogueRefresh.cpp:46-57`), and its window says the
  list could not be made (#227). Provisioning keeps its file too
  (`provision/cairn-provision.sh:902-907`).
- One game that makes ScummVM fail at every Look therefore never stops
  the list for the other games.
- There is no fallback to `--no-scummvm` on a computer where this runs.
  If `systemd-run` cannot start the detection, the run fails, and the
  old list stays.
- A game leaves ScummVM only when its id joins the stay-on-Steam list,
  in an image change of its own that also says what happens to its
  saves.
- Family never turns a child's ScummVM entry into a Steam entry. If the
  full list ever asks for that, the child keeps the ScummVM entry and
  the journal gets a line.

### The detection runs as a throwaway user

- kidscan runs through `systemd-run --wait --pipe` with
  `DynamicUser=yes`, no network, no home, a private `/tmp`, `HOME=/tmp`
  and a time limit. ScummVM parses game files that any Guardian's Steam
  can write, and parsing them as root would be new risk.
- kidscan names `/usr/bin/scummvm`; it does not look ScummVM up on
  `PATH`. It runs ScummVM from `/`, never from a game's folder: Fedora's
  build loads `lib*.so` files from its working folder and `./plugins`
  (2.9.1 `base/plugins.cpp:186-203`).
- Fedora's build loads every engine before each detection
  (`base/main.cpp:498-500`), so a Look takes one ScummVM start per
  game. The run gets its own time limit, longer than the two minutes
  Family gives other tools (`guardian/src/SystemTool.cpp:10`). Check K3
  times it over the laptop's 25 games before the limit is set.
- The Games page's list of games in the Guardian's own Steam is not
  kidscan. It is Family's own code (`guardian/src/OwnSteamGames.cpp`),
  runs no ScummVM, and does not change.

### What the games file says

A ScummVM entry keeps the Steam command, so that an older launcher and
an older Family, after a rollback, still see the Steam game it was. Its
`engine` is `scummvm`, as kidscan already writes for a game ScummVM
runs; no reader looks at `engine` today. It adds one key:

```json
"engine": "scummvm",
"exec": ["steam", "-applaunch", "283920"],
"source": {"store": "steam", "appid": "283920"},
"scummvm": {"game": "scumm:puttputt", "folder": "Putt-Putt 1"}
```

- `game` is ScummVM's id: an engine id, a colon and a game id. The
  engine id is lowercase letters and digits, 1 to 32 long. The game id
  is letters and digits, 1 to 64 long, and starts with a letter. So it
  never begins with `-`, which ScummVM would read as an option.
- `folder` is one folder name inside `steamapps/common`, by kidscan's
  rule (`tools/kidscan/kidscan.py:176-184`). It also has no control
  character and no space at either end, and does not start with `xn--`
  in any case. It is written into ScummVM's settings file, where a line
  break would add a setting and ScummVM trims spaces. ScummVM decodes a
  path part that starts with `xn--` as punycode (2.9.1
  `common/path.cpp:1140-1165`), so it could open a folder other than
  the one checked.
- The app id comes from `exec`, as today. No reader's app-id rule
  changes.
- The new launcher makes a ScummVM tile from an entry with a `scummvm`
  key that passes, `engine` `scummvm`, and the Steam command. It never
  takes a program or an option from the file. An entry whose key does
  not pass is left out with a sentence in the journal.
- The file stays version 1. A reader that does not know the key sees a
  Steam game.
- Readers: the launcher's `GamesFile` and Family read the key, through
  one rule in a small library both link. The picture step needs no
  change. kidscan writes the key only for an entry that passes. The
  round-trip test feeds kidscan's output to each of them.

### How a ScummVM tile starts

From a ScummVM entry the launcher builds the tile's command itself:

```
/usr/libexec/cairn/cairn-scummvm scumm:puttputt "Putt-Putt 1"
```

The program's path is compiled into the launcher. On a computer set up
by hand it is `/usr/local/libexec/cairn`, and provisioning installs the
program there beside the launcher. `cairn-scummvm` (a proposed name) is
a small C++ program that runs as the child, inside the tile's scope
(ADR-0025). It:

1. Changes to `/`, so ScummVM never loads code from a folder someone
   else can write.
2. Checks both words again, by the same rules.
3. Finds the game's folder under the family folder's `steamapps`, a
   path compiled in, one part at a time: `steamapps`, `common` and the
   game's folder must each be a folder and not a link.
4. Takes the child's home from the account database, never from
   `$HOME`, and makes the child's save folder for the game (below).
5. The first time, copies the child's progress from their Steam copy
   (below). If that copy fails, it stops here.
6. Writes the child's ScummVM settings file afresh.
7. Takes `SCUMMVM_SAVEPATH` out of its environment, since it would win
   over the save folder in the settings (Context), and replaces itself
   with `/usr/bin/scummvm --config=<settings file> puttputt`.

ScummVM is started by the plain game id, and the settings file has a
section of that name. So the name never drifts to `<id>-1`, and the
old saves' names match: Steam's settings use the same name for Joins
the Parade, and check K1 reads the others. Because the file is written
afresh, nothing a child changed in ScummVM's own windows lasts to the
next start. The file is `~/.local/share/cairn/scummvm.ini` (a proposed
name). For ada on the image, whose home is `/var/home/ada`:

```ini
[scummvm]
fullscreen=true
confirm_exit=false
gui_return_to_launcher_at_exit=false
gui_browser_native=false

[keymapper]
keymap_global_MENU=
keymap_global_DEBUGGER=
keymap_global_MUTE=
keymap_sdl-graphics_FULS=
keymap_sdl-graphics_STCH=
keymap_sdl-graphics_ASPT=
keymap_sdl-graphics_FILT=

[puttputt]
gameid=puttputt
engineid=scumm
path=/var/lib/cairn/steam-library/steamapps/common/Putt-Putt 1
savepath=/var/home/ada/.local/share/cairn/saves/puttputt
keymap_engine-default_MENU=
```

- The key names are ScummVM 2.9.1's (`default-events.cpp:335-380`;
  `backends/graphics/sdl/sdl-graphics.cpp:482-525`). Alt+Enter would
  leave fullscreen and leave a small window, as in #184. Check V7 tries
  each key.
- When a check fails, `cairn-scummvm` writes one sentence for the
  journal and ends. No window came, so the launcher shows the grown-up
  screen, as for any launch that shows nothing (ADR-0030).
- After that the launcher treats ScummVM as it treats Tux Paint: the
  same grace, the same window rules (ADR-0026), Super+Q and
  Ctrl-Alt-Home. None of Steam's waits apply.
- At login the launcher leaves out a ScummVM tile whose folder is not
  there, with a sentence in the journal. A child never sees a tile for
  a game that is no longer on the computer. A folder that goes during
  the session ends on the grown-up screen at the tap.
- The Terminal's `open` starts the same command, since its doors come
  from the tiles.

### The launcher ends ScummVM when it leaves the game

- The launcher watches the title of a ScummVM tile's window. When the
  title goes back to ScummVM's own name after it was the game's, ScummVM
  has left the game for its own list: Return to Launcher, or an error
  while the game ran. The launcher ends the tile's scope and shows the
  grown-up screen, as for any launch that went wrong.
- When the title is still ScummVM's own name a few seconds after the
  window opened, ScummVM stopped before the game: an error box, then
  its list. The launcher does the same. Slice 0 measures the wait.
- So a child never stays in ScummVM's game list, its Add Game, its
  Options or its downloads. They may see ScummVM's error box, or its
  list, for a moment first.
- Whether the compositor passes a title change for ScummVM's window,
  X11 or Wayland, is checked in slice 0.

### Where the game is read from

- From the family folder, as it is. Native ScummVM writes nothing next
  to a game's files. The SCUMM engine writes only to the save folder,
  and the storybook engine writes nothing. Its other files, settings
  and logs, go to the child's home (ScummVM 2.9.1's source).
- Every child can already read every file there (ADR-0042, lines
  182-183). No child can write there
  (`system_files/usr/lib/tmpfiles.d/cairn.conf:14-17`). So no child can
  read anything new.
- A ScummVM game needs no copy in the child's Steam, and Save makes
  none.

### Where a child's progress goes

- To `~/.local/share/cairn/saves/<game id>/` (a proposed name), one
  folder per game, mode 0700, in the child's home. `cairn-scummvm`
  makes it, as the child.
- Not ScummVM's own default, which mixes every game's files, and may be
  an old `~/.scummvm` (Context).
- One folder per game keeps the once-only copy simple. It also makes
  "start this game over" possible later.
- `/home` is not rolled back (DESIGN §4.6). The progress is not in
  Steam Cloud. A reset of a child's account keeps it, as it keeps their
  creations (DESIGN §11, changed in this change).

### Progress made in Steam comes along

The first time a child starts a game this way, `cairn-scummvm` copies
the files in their own Steam copy's `Saves` folder:

- From `~/.local/share/Steam/steamapps/common/<folder>/Saves`, the
  child's own library (`provision/cairn-copy-games.py:96`), only when
  it is a real folder, not a link.
- Only regular files directly inside it, found without following a
  link. Not a name that starts with `.`, not `steam_autocloud.vdf`,
  which is Steam Cloud's marker and not a save (ADR-0033, #250), and
  nothing over 64 MiB.
- It never overwrites a file in the save folder. Each file goes to a
  temporary name with the source's modification time, then into place
  without replacing anything.
- It never changes, moves or deletes anything in the Steam copy.
- When every file is copied or already there, it writes a marker,
  `.from-steam`, saying where the files came from and how many. With
  the marker there it never copies again. So files that Steam Cloud
  brings into the old copy later, possibly a sibling's, are never
  pulled in.
- **A copy that fails stops the game.** On a full disk, or with a file
  it cannot read, `cairn-scummvm` leaves no marker, writes one journal
  sentence naming the Steam folder, and ends, so the grown-up screen
  shows. Otherwise a child who saw no progress would start again, the
  new save would take the old file's name, and every later copy would
  skip the old file. The next start tries again and skips the files
  already there.
- The names stay as they are. `cairn-scummvm` uses ScummVM's game id,
  and Steam's settings for Joins the Parade use the same name. Check
  K1 reads the other ten before the switch. If one differs, the copy
  renames that game's files, in a change of its own, before the switch.
- Nothing deletes a child's Steam copy of a ScummVM game. It is the
  backup of their old progress. A reflink copy takes no extra disk
  while it matches the family folder's (ADR-0033). Once the Guardian's
  Steam updates the family folder's copy, the child's old copy takes
  its own space.

### Steam at login

The rule stays: Steam starts at login only for a child with a Steam
tile (ADR-0027). A ScummVM tile is not one, and
`launcher/tests/tst_steamatlogin.cpp:32-35` already checks it. A child
whose games all start in ScummVM never starts Steam.

### The parent's steps

- They stay the same: install the game in their own Steam, pick it for
  a child on the Games page, and Save (ADR-0041, ADR-0042). Save still
  moves a picked game into the family folder.
- For a ScummVM game, Save copies nothing to the child. So it is given
  even when the child's Steam was never signed in. The page's rules for
  a logged-in child stay as they are.
- The Games page says it: a ScummVM game's card says it plays without
  Steam and that its progress is kept on this computer only, and the
  Save's window says such a game was given, not copied (slice 8 has the
  proposed sentences).
- Pictures still come from the Guardian's Steam by app id, for both
  kinds of game (ADR-0035, ADR-0047).

### Lists catch up by themselves

- Each child's list keeps the form each game had when it was saved. A
  Save rewrites only the children it names, and a Look writes no list
  (ADR-0041).
- From this change, every Save and every Look also brings each child's
  list up to date. Each game already on it takes its entry from the
  full games list as it is now. No game is added and none is taken
  away.
- After an image update the computer does the same once by itself, at
  its first start, with no parent: the root side of a Look, without
  pictures, which keeps the pictures already there. A stamp in
  `/var/lib/cairn` names the image it last ran for (proposal: the
  booted image's digest, since `IMAGE_VERSION` is set only when the
  build is given one, ROADMAP P1-3). It takes the Save's lock, so it
  never runs beside a Save. A child already logged in sees the change
  at their next login.
- So a parent never presses Look because of an update, now or when a
  later change moves a storybook game to ScummVM.
- On the image, a child reads their own list
  (`launcher/bin/cairn-launcher.in:13-15`). On a computer set up by
  hand, every child reads the full list
  (`provision/cairn-provision.sh:449`), so the next provisioning run or
  Look changes every child at once. The kids' laptop has run the image,
  with Family, since 2026-11-04. Its move followed the steps in
  `provision/README.md` (lines 402-473), which remove provisioning's
  launcher and give each child their own list. Check K1 confirms the
  lists are there.

### Where it applies

The image, and a computer set up by hand once provisioning's
`make_game_tiles` changes. It covers the tiles at levels 1 and 2.
Levels 3 and 4 reach games another way (#107).

### Slices

Each slice is one pull request with its own tests and bookkeeping.
Three rules set the order:

- The launcher and Family read the `scummvm` key before anything Cairn
  runs writes it. An older reader still sees a Steam game, so no Save
  can drop it.
- The progress copy, and the end of ScummVM's own list, come before the
  switch.
- The lists catch up by themselves after an image update before the
  switch turns on, so no parent step is ever needed.

So slices 1 to 9 change nothing a child or a parent sees. Slice 10 is
the switch. Names marked "proposed" stay proposals until this ADR is
accepted.

0. **Checks before any code** (not a pull request). On the kids'
   laptop, K1 to K3. In cairn-min, by hand as ada: whether ScummVM
   starts a game by a section that holds only `gameid`, `engineid`,
   `path` and `savepath`; which window Fedora's build opens, X11 or
   Wayland, and whether labwc's `scummvm` rule matches it; whether the
   compositor passes the title change when ScummVM goes from the game
   to its own list; how long the title keeps ScummVM's own name before
   the game's, and whether it ever changes with a data file moved
   aside; and whether the emptied keys in the settings parse and take
   the keys away. The results, with what was read for this ADR, go in
   a research note (proposed `docs/research/native-scummvm.md`).
1. **The decision, in words.** This ADR as proposed, DESIGN §8.2, §8.3,
   §8.4, §8.7 and §11, the notes in ADR-0027, ADR-0033 and ADR-0041, and
   ROADMAP. Nothing is built.
2. **The image checks its ScummVM.** A build check stops the image when
   `/usr/bin/scummvm` is missing or not the `scummvm` package's, is
   older than 2.9.1, or `--list-engines` does not name `scumm` and
   `mohawk`. It runs ScummVM with SDL's dummy drivers
   (`tools/kidscan/kidscan.py:338-342`) and `HOME=/tmp`, which the
   checks' `RUN` mounts as a tmpfs. If slice 0 found a Wayland window,
   labwc's `scummvm` rule gains a match for it.
3. **The shared rule, and `cairn-scummvm`.** A new top-level folder,
   `scummvm/` (proposed), as `accounts/` is for accounts. It holds a
   static library with the rule for the `scummvm` key, which the
   launcher and Family both link, and the program a ScummVM tile runs:
   steps 1 to 4, 6 and 7 under "How a ScummVM tile starts", without the
   progress copy. Proposed classes: `ScummVMKey`, `GameFolder`,
   `SaveFolder` and `ScummVMSettings`. Provisioning installs the program
   beside the launcher (`provision/cairn-provision.sh:434-458`). Its
   test runs it against a stand-in ScummVM that prints its arguments,
   its environment and its working folder.
4. **Progress made in Steam comes along.** Step 5, by the rules under
   "Progress made in Steam comes along", in a proposed `SaveCarryOver`
   class. If K1 found a game whose save names differ from ScummVM's id,
   this slice also renames that game's files as it copies them.
5. **The launcher reads the key.** `GamesFile` builds a ScummVM tile's
   command from a key that passes. It leaves out, with a journal
   sentence, a key that does not pass and a folder that is not there at
   login.
6. **The launcher ends ScummVM when it leaves the game.**
   `ForeignToplevel` and `ForeignToplevelList` pass on a title that
   changes after the window opened. A proposed `ScummVMWatch` holds the
   rule, as a pure function of the titles seen and the time since the
   window opened. ScummVM's own name starts with `ScummVM ` (2.9.1
   `base/version.cpp:80`). Only a ScummVM tile is watched.
7. **kidscan writes the key, and Family reads it.** kidscan gains
   Cairn's rules behind one option (proposed `--cairn`), and reads its
   last list with `--previous`, so its output for other uses does not
   change. With `--no-scummvm` its output stays byte for byte what it
   is today. It runs ScummVM from `/`. Family learns which entries are
   ScummVM games through the shared rule. The round-trip test feeds a
   `--cairn` run to the launcher, Family and the picture step.
8. **Save gives a ScummVM game with no copy, and the page says so.**
   `ChildGameList` gives a newly ticked ScummVM game without a copy, and
   never turns a child's ScummVM entry into a Steam entry.
   `cairn-family save-games` brings every child's list up to date at
   each Save and Look. Proposed words, each with `qsTr()`, the `.ts`
   file and an `Accessible.name`:
   - In the Save's window: "Gave %1 to %2. It plays without Steam, so
     nothing was copied."
   - When a child's Steam has not run yet: "%1's Steam has not run yet,
     so the games that need Steam could not be copied. The games that
     play without Steam were given. Open Steam for %1 and sign in, then
     press Save again."
   - On a ScummVM game's card: "Plays without Steam. Progress is kept on
     this computer only."
   - On its card when the family folder no longer has it: "On %1's
     list, no longer in the shared folder. %1 cannot play it until you
     install it again in your own Steam."
9. **Lists catch up by themselves after an image update.** A system
   unit (proposed `cairn-games-catch-up.service`) runs once after each
   image update, after the login screen is up. It runs a new
   `cairn-family` subcommand (proposed `catch-up-games`) that only root
   may run, without `pkexec`. It takes the Save's lock
   (`/run/lock/cairn-save-games`) and writes the stamp last. Whether
   systemd's `ConditionNeedsUpdate=` works on bootc is unconfirmed.
10. **The switch.** Family's Look and Save, and provisioning's
    `make_game_tiles`, run kidscan as a throwaway user
    (`guardian/src/CatalogueRefresh.cpp:46-66`;
    `provision/cairn-provision.sh:902`). The command, a proposal for
    review:

    ```
    systemd-run --wait --pipe --quiet --collect \
      -p DynamicUser=yes -p PrivateNetwork=yes -p ProtectHome=yes \
      -p PrivateTmp=yes -p RuntimeMaxSec=<limit> -E HOME=/tmp \
      -- /usr/bin/python3 -I -B /usr/libexec/cairn/kidscan.py --cairn \
         --scummvm /usr/bin/scummvm --previous /var/lib/cairn/games.json \
         --library /var/lib/cairn/steam-library
    ```

    `<limit>` comes from K3's time. Family's call gets the same limit of
    its own, since `SystemTool` ends any tool after two minutes
    (`guardian/src/SystemTool.cpp:10, 23-30`). `docs/TRY.md` and the
    parent help say which games use Steam, that ScummVM progress is on
    this computer only, that uninstalling a game in a parent's own
    Steam takes it from every child, and what a rollback does to these
    games. `docs/research/network-activity.md` gains what native
    ScummVM can reach on a click (Context), and
    `docs/research/child-test-02.md` says when a game in ScummVM itself
    reaches the laptop. V1 to V12 run on an image built with slices 2
    to 10.
11. **On the kids' laptop.** With the maintainer's go: first the
    ADR-0057 run that needs a Putt-Putt game to start through Steam,
    then the image with slice 10 and its first start, then K4 and K5,
    with no Look pressed. Then this ADR can be accepted, if nothing
    overturned a bullet of the Decision.

## Consequences

**Easier:**

- A ScummVM game starts with no wait for Steam to start, update itself,
  sign in or ask about cloud saves. It starts offline too. None of the
  waits or grown-up lines listed in Context apply to it.
- A new child whose games all start in ScummVM needs no Steam sign-in,
  no copy, no Steam tools and no Steam at login. That keeps about a
  gigabyte of memory free (ADR-0027). On the laptop, each child probably
  also has a storybook game, so none of them gains this yet
  (unconfirmed).
- Each child's progress is their own, even when siblings share a Steam
  account.
- A Steam update of a ScummVM game reaches every child at once. They
  read the family folder, which the Guardian's Steam updates. A child's
  own Steam never updates these games.
- No step for a parent after an update.
- The build now does what DESIGN §8.2 says.
- Cairn owns ScummVM's settings, so it can take keys away that it could
  not before.

**Harder, or given up:**

- **No Steam Cloud for ScummVM progress.** Nothing is kept off the
  computer. A lost disk loses it. Parent documents say so plainly.
- **Steam's family controls do not cover a ScummVM game.** Steam
  Families' allow-list and playtime schedules (DESIGN §8.4) do not see
  it. Cairn's list for each child is the only gate (ADR-0041), as it
  already was for a family that shares one Steam account. Cairn has no
  playtime limit of its own yet. Whether the family uses Steam's limits
  is unconfirmed.
- **A game taken out of the family folder stops playing for every
  child at once.** A parent who uninstalls Putt-Putt in their own Steam
  takes it from every child. A Steam game still played from each
  child's copy. The Games page and parent help say so.
- **Two ways to start a game, and two kinds of failure.** The grown-up
  lines that name Steam's fixes (ADR-0045, ADR-0049) do not apply to a
  ScummVM game; its failures show the plain grown-up screen. Help for
  parents must say which games use Steam.
- **ScummVM updates come from Fedora, with the image.** Fedora 45 has
  2.9.1, and the request for 2026.4.0 is open (Red Hat bug 2430646).
  The Tortoise and the Hare stays on Steam until then. A Fedora update
  could change which games ScummVM recognises, and 2026.4.0 can mark a
  SCUMM game as one it asks about. The build check and the VM checks
  catch a missing engine, not every game, so each ScummVM update gets
  V4 and V7 again on one Humongous game.
- **The first copy brings what the Steam copy holds.** Where siblings
  share one Steam account, Steam Cloud may have left a sibling's newest
  save there under the same name. That is what the child last saw, and
  from then on each child's progress stays apart.
- **After a rollback** to an image without this change, every tile is
  still there, and a Save keeps every game on the list, because the
  entries keep their Steam command. A child with a Steam copy plays it
  through Steam, with the progress from before the switch; what they
  saved in native ScummVM is not seen until the image comes forward
  again. What they save in Steam meanwhile is not brought over then,
  because the marker is already there. A child who got the game after
  the switch has no Steam copy, and the tile is expected to end on the
  grown-up screen (V10).
- **Each Save and Look takes longer**, by one ScummVM start per game.
  K3 measures it.
- What happens when the Guardian's Steam updates a game while a child
  plays it from the family folder is not known.
- More code: `cairn-scummvm` and its small classes, a shared rule for
  the key, the progress copy, the title watch, the boot-time catch-up,
  and kidscan's rules.
- Dependencies: none new. ScummVM is already in the image.

**Known limits:**

- F5 in a Humongous game still opens ScummVM's menu. Resume, Options,
  About and Help are harmless, and what Options changes is gone at the
  next start. Return to Launcher and Quit end the game.
- A child may see ScummVM's error box or its game list for a moment
  before the launcher ends it.
- Every storybook game stays on Steam until it is checked.

**Changes to other ADRs:**

- **ADR-0033:** a games entry may carry a `scummvm` key beside its
  Steam command, and the launcher makes a ScummVM tile from it. On the
  image kidscan runs ScummVM's detection, as a throwaway user. Its open
  choice is settled for ScummVM, and §8.2 changes in this change. Its
  alternative "One shared copy, with each child's saves sent to their
  own home" (lines 378-383) is close to this one. Its objections are
  met: Steam keeps the games ScummVM cannot run; leaving Steam Cloud is
  accepted; and `cairn-scummvm` never starts ScummVM until the child's
  save folder is there, and never through `SCUMMVM_SAVEPATH`.
- **ADR-0041:** a ScummVM game is given without a copy; every Save,
  Look and first start after an image update brings each child's list
  up to date. Its known limit about ScummVM's own list is closed for a
  game that starts in native ScummVM: the launcher ends ScummVM when it
  leaves the game. A game Steam starts in its bundled ScummVM keeps the
  limit.
- **ADR-0027:** a note. The launcher knows two launch forms now (its
  line 55 says one). Its rule does not change.
- **ADR-0038:** the image build checks ScummVM (Checks, V1).
- **ADR-0057** is not changed. On 2026-11-20 the maintainer named the
  laptop runs its acceptance waits on: Arthur's Birthday and Pep's
  Birthday Surprise through Proton, a quick quit of one Putt-Putt game
  the children play, started by Steam, and Harry and the Haunted House
  only if it behaves differently. Arthur's, Pep's and Harry stay on
  Steam under this decision. The Putt-Putt game starts through Steam on
  the laptop only until slice 11, so its run comes first.

## Alternatives considered

- **Steam for every game,** as today. Not chosen: the maintainer's
  decision, and the costs in Context.
- **Native ScummVM running each child's own Steam copy,** with the
  saves left where they are. No progress to copy. But the copy step
  and a Steam sign-in stay for every new child, the launcher would
  build a path into each home, and the storybook copies may have no
  `Saves` folder.
- **ScummVM from Flathub,** 2026.4.0 today, which runs The Tortoise and
  the Hare. Not chosen: it is installed outside the image and updates
  on its own schedule, it may use the network and the whole home
  folder, it needs the family folder added to its sandbox, and it still
  has no engine for D.W. DESIGN §4.2 keeps ScummVM in the image.
- **Cairn building a newer ScummVM itself,** in the build stage or a
  COPR. Cairn would maintain a package of another project.
- **Every game ScummVM recognises, with its question turned off.** A
  child would meet storybook games ScummVM marks unstable, with pages
  that may not work.
- **A switch per game on the Games page, Steam or ScummVM.** It puts
  the troubleshooting on the parent, which is what this decision is
  for removing.
- **ScummVM's own save folder.** It mixes every game's files, and may
  be an old `~/.scummvm`.
- **Moving the saves, or removing the Steam copies.** A move that fails
  part-way can lose progress.
- **Taking the command from the entry's `exec`.** The games file is
  made from files a Guardian's Steam writes. It must never choose a
  program or an option.
- **A ScummVM entry with ScummVM's own command in `exec`,** as kidscan
  writes outside Cairn. After a rollback, an older launcher and Family
  would drop the game, and a Save would take it off the list.
- **Sending a game to Steam when its detection fails or finds
  nothing.** It flips a game between ScummVM and Steam and leaves
  progress behind each time (Decision).
- **Detection as root,** as kidscan runs today with `--no-scummvm`.
  ScummVM would parse game files as root.
- **A Look a parent presses after each update.** A parent from Windows
  or a Mac would not know to.
- **The kiosk taking plain F5 from every program.** With the launcher
  ending ScummVM when it leaves the game, F5's menu leads nowhere new,
  and F5 stays for other programs.

## Checks

### In the cairn-min VM

On an image built with every slice, with the test children ada (L1)
and ben (L2), a Guardian, and in the family folder Putt-Putt Joins the
Parade, Putt-Putt Saves the Zoo and one game that stays on Steam.

- **V1.** The image build passes its new check: `/usr/bin/scummvm`
  belongs to the `scummvm` package, is 2.9.1 or later, and
  `--list-engines` names `scumm` and `mohawk`.
- **V2.** A Look as the Guardian: the full games list has ScummVM
  entries for the two Putt-Putt games, each with its Steam command
  still in `exec`, and a Steam entry for the other. No ScummVM process
  runs as root meanwhile, and the journal shows the throwaway unit.
  ben's existing list changes form for the two games and keeps the
  same games. The Look's time is written down.
- **V3.** A Save gives ada, whose Steam was never signed in, Joins the
  Parade: no copy runs, the answer is `child ada added 1 removed 0`, and
  the Save's window says the game was given. At her login Steam does
  not start (`systemctl --user is-active cairn-steam` says inactive),
  and the tile shows its picture.
- **V4.** The tile starts ScummVM, and its window fills the screen: the
  labwc rule matches the window Fedora's build opens, or a new rule is
  added. Super+Q brings back the tiles. Ctrl-Alt-Home ends the scope.
- **V5.** A save from the game's own save screen lands in
  `~/.local/share/cairn/saves/puttputt/`, and ScummVM's log shows no
  write error. A new start loads the save.
- **V6.** ben's Steam copies hold a save from the bundled 1.7.0 (Joins
  the Parade) and from 2.0.0 (Saves the Zoo). The first start copies
  them, and each game's own load screen lists and loads them. The
  copies' `Saves` folders have the same checksums as before. A second
  start copies nothing. A file already in the save folder is not
  overwritten. A `Saves` folder that is a link copies nothing. With an
  unreadable file in `Saves`, the tap ends on the grown-up screen and no
  marker is written. In a throwaway copy, a save native ScummVM wrote,
  put by hand into a Steam copy, loads in Steam's version.
- **V7.** In the game, these do nothing: Ctrl+F5, Ctrl+Alt+D, Ctrl+U,
  Alt+Enter, Ctrl+Alt+S, Ctrl+Alt+A, Ctrl+Alt+F and Ctrl+F; and, with a
  controller passed to the VM, Start and the left shoulder (otherwise
  on the laptop in K4). F5 opens ScummVM's menu. Its Return to Launcher
  ends the game and shows the grown-up screen. Something changed in
  the menu's Options is gone at the next start from the tile.
- **V8.** With the game's folder renamed by root: at ada's next login
  there is no tile for it, and one journal sentence. Renamed during her
  session, her tap ends on the grown-up screen. With one data file of
  Saves the Zoo moved aside, its tap ends on the grown-up screen.
- **V9.** ben, with one ScummVM game and one Steam game: Steam starts
  at login, and both games start.
- **V10.** After `bootc rollback` to an image without this change,
  every tile is still there. ben's Joins the Parade starts through
  Steam. ada's, who has no Steam copy, ends on the grown-up screen. A
  Save in the older Family keeps every game on both lists. After
  coming forward again, both tiles start ScummVM with the progress in
  the save folder.
- **V11.** With one data file of Saves the Zoo moved aside, a Look
  keeps its ScummVM entry, and ben's list is unchanged. (A ScummVM that
  hangs on one game, or on all, is tried with a stand-in in slice 7's
  and slice 10's tests.)
- **V12.** After an image update that changes nothing for games, the
  first start brings no list a change and writes the stamp. After one
  from a build without the switch to one with it, the first start gives
  ben's list its ScummVM entries with no Look. A second start does
  nothing.

### On the kids' laptop

With the maintainer's go, and the children logged out. The laptop runs
the image built from 57bd89a on 2026-11-20, and moves to one from
52845f0 or later. K1 to K3 run on whichever it has then.

- **K1.** Read-only, before the switch. Which launcher the children's
  sessions run: no `/usr/local/bin/cairn-launcher`, and a list for each
  child in `/var/lib/cairn/games/`. Each child's `Saves` folders in the
  11 Humongous games, and the family folder's own. The section names in
  each game's `ScummVM_Linux/*.ini`, which must match ScummVM's game
  ids. Whether `~/.scummvm` is in each home.
- **K2.** A copy of every child's `steamapps/common/*/Saves`, Pep's
  `SaveGames` and the family folder's `Saves`, readable only by root,
  kept until this ADR is accepted.
- **K3.** Read-only: slice 10's `systemd-run` command, run by hand with
  the image's kidscan and ScummVM, printing to the terminal only. Which
  of the 25 games get a ScummVM entry, with which id, and how long the
  run takes.
- **K4.** After the image update, with no Look: with each child, two
  Humongous games they have played show their progress on the game's
  own load screen. Their Steam copies' `Saves` are unchanged.
- **K5.** Pep's Birthday Surprise and Arthur's Birthday (Proton) and
  Harry and the Haunted House (ZOOM's bundled ScummVM) still start
  through Steam.

## What comes later

- **Storybook games, one at a time,** each in its own change once K3 and
  a start in the VM show the image's ScummVM runs it cleanly. First the
  five with ZOOM's Linux build, which also ends #184 for them. Some may
  need detection inside a subfolder, and the entry would then name it.
- **The Tortoise and the Hare,** when Fedora's ScummVM is 2026.1.0 or
  later.
- **D.W. The Picky Eater** needs ScummVM's Media Station engine, which
  neither Fedora's build nor Flathub's has. It stays on Steam.
- **A way back to Steam for one game,** if one is ever needed: its id
  joins the stay-on-Steam list, and the same change copies the child's
  saves back into their Steam copy, if V6 shows they load there.
- **An option upstream in ScummVM to hide Return to Launcher,** offered
  to ScummVM.
- **"Start this game over"** on the Games page.
- **A copy of progress off the computer,** chosen by the parent, with
  no network unless they ask.
- **Levels 3 and 4** (#107).

## Sources

- ScummVM 2.9.1 source, tag `v2.9.1`:
  https://github.com/scummvm/scummvm/tree/v2.9.1 (files named above).
- ScummVM 2026.4.0 source, for the lines marked 2026.4.0.
- Fedora's package: https://packages.fedoraproject.org/pkgs/scummvm/scummvm/
  and its spec, https://src.fedoraproject.org/rpms/scummvm (f45).
- ScummVM's compatibility list: https://www.scummvm.org/compatibility/
- ScummVM 2.8.0 and 2.9.1 release notes:
  https://downloads.scummvm.org/frs/scummvm/2.8.0/ReleaseNotes.html,
  https://downloads.scummvm.org/frs/scummvm/2.9.1/ReleaseNotes.html
- Red Hat bug 2430646, "scummvm-2026.4.0 is available".
- Steam's store data for each of the 25 games, read on 2026-11-20 for
  the platforms it lists:
  `https://store.steampowered.com/api/appdetails?appids=<appid>`.
