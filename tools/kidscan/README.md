# kidscan

Game library scanner (DESIGN §8.7). Finds installed Steam titles, works out
which can run under native ScummVM with no Proton and no Steam client UI, and
which need Steam itself, then writes a launcher-agnostic JSON manifest.
Optionally emits `.desktop` files.

Standard library only, on purpose: it runs during image build and on parent
machines where nothing else is guaranteed to be installed.

Prototype imported 2026-09-03 (issue #7). No GUI yet; that is Phase 3.

## Usage

```sh
# Print the manifest to stdout
./kidscan.py

# Write it somewhere, plus one .desktop file per tile
./kidscan.py -o ~/.local/share/cairn/games.json --desktop-dir ~/.local/share/applications/cairn

# Extra Steam root (e.g. a second drive), explicit ScummVM binary
./kidscan.py --steam-root /mnt/games/Steam --scummvm /usr/bin/scummvm

# Skip ScummVM detection entirely; everything routes through Steam
./kidscan.py --no-scummvm

# Assign categories by appid (a parent never does this by hand; the Phase 3 GUI will)
./kidscan.py --categories categories.json   # {"294660": "games"}

# Only these library folders, each holding steamapps/; no other Steam is read
./kidscan.py --library /var/lib/cairn/steam-library

# Name a picture for each game that has one in this folder
./kidscan.py --library /var/lib/cairn/steam-library \
    --pictures /var/lib/cairn/game-pictures
```

Exit status 2 means no Steam installation was found, or the options were
wrong. `--library` and `--steam-root` cannot be used together.

Provisioning runs it as root, with an empty `HOME`, to make the children's
game tiles (ADR-0033, ADR-0035):

```sh
env HOME= python3 -I -B /usr/local/libexec/cairn/kidscan.py --no-scummvm \
    --library /var/lib/cairn/steam-library \
    --pictures /var/lib/cairn/game-pictures
```

It writes the output to `/var/lib/cairn/games.json`, which the launcher
reads with `--games`.

## What it does

1. **Finds Steam roots.** `~/.steam/steam`, `~/.steam/root`,
   `~/.local/share/Steam`, and the Flatpak data path, plus any `--steam-root`.
   Symlinked duplicates collapse to one.
   With `--library`, only the folders named, and none of the usual places.
2. **Follows every library folder.** Parses `steamapps/libraryfolders.vdf`
   for secondary drives, handling both the modern nested format and the old
   bare-string one.
   With `--library` it follows none, so a file in a shared folder cannot
   send it into someone's home. A `--library` whose `steamapps` is a link
   is not read either: kidscan warns, lists no games and exits 2.
3. **Reads every `appmanifest_*.acf`.** Only a plain file of at most
   1 MiB is read; a link, a FIFO or a larger file is skipped with a
   warning, and so is a file with a string that never closes. Keeps only
   apps whose install directory actually exists. Drops Steamworks Redistributables, the Steam
   Linux Runtimes and Proton by appid and by name.
   It also leaves a game out, in every mode (ADR-0033), when:
   - its `appid` is not a number from 1 with no leading zero, or is not the
     number in the file's own name, `appmanifest_<appid>.acf`;
   - its `installdir` is not one plain folder name;
   - its `StateFlags` lacks FullyInstalled (4); a game with an update
     waiting has other bits too, and keeps its tile;
   - it is on a beta branch, in `UserConfig` or `MountedConfig`;
   - any of these values is of the wrong kind, such as a block where text
     belongs. That game is left out and the scan goes on.

   The copy step in `provision/` takes `APPID`, `plain_folder_name` and
   `on_beta_branch` from here, so those rules cannot drift apart. Its own
   `StateFlags` rule is stricter: it copies only a game with nothing
   pending. On the image the tools step and the copy step take only the
   apps `is_steam_tool` calls Steam's tools, the same rule that leaves
   them out of the tiles, and read Steam's files with `read_plain_file`
   (ADR-0046). Both take what a tool build is from here too, so the two
   cannot drift apart: `tool_installdir` gives a tool's folder name when
   `is_steam_tool` holds, its `installdir` is one plain folder name, and
   its folder is a real folder with a plain `toolmanifest.vdf`, neither a
   link, and `TOOL_BUILD` is the `<appid>-<buildid>` name of a build in
   the tools folder.
4. **Runs `scummvm --detect` per install directory.** Titles ScummVM
   recognises get a native launch command, one tile per detected game (a
   Steam "pack" can hold several). Everything else gets
   `steam -applaunch <appid>`. The detector runs with SDL's dummy video and
   audio drivers, because ScummVM opens a window before it reads a file and
   there is no display during image build or over ssh. A detector that
   exits non-zero is reported on stderr, not silently routed through Steam.
5. **Names each game's picture** with `--pictures DIR`: when
   `DIR/<appid>.jpg` is a plain file, not a link, the game's entry gets
   its full path as `picture`. kidscan never opens the picture.
   Provisioning copies the pictures there (ADR-0035).
6. **Writes the manifest**, sorted by title, and optionally `.desktop` files.
   `-o FILE` writes a new file beside it, mode 0644, puts it on the disk,
   then renames it over the old one, so a launcher starting meanwhile
   reads the old file or the new one, never half of one. A link at `FILE`
   is replaced, not written through. `-o -` prints it.

## Manifest format (version 1)

```json
{
  "version": 1,
  "steam_roots": ["/home/nora/.local/share/Steam"],
  "scummvm": "/usr/bin/scummvm",
  "entries": [
    {
      "id": "steam-294660-scumm-freddi",
      "title": "Freddi Fish and the Case of the Missing Kelp Seeds",
      "category": "games",
      "engine": "scummvm",
      "exec": ["/usr/bin/scummvm", "--fullscreen", "--path=/…/Freddi Fish", "scumm:freddi"],
      "source": {"store": "steam", "appid": "294660"},
      "needs_steam_running": false,
      "picture": "/var/lib/cairn/game-pictures/294660.jpg"
    }
  ]
}
```

`engine` is `scummvm` or `steam`. `needs_steam_running` tells the launcher
whether the silent Steam client (DESIGN §8.3) must be up before this tile can
launch. `category` defaults to `games`, the tile kind the launcher draws as a
quiet Paper card and the Terminal lists under `games` (ADR-0024). A category
map may say `make`, `practice`, `games` or `machine`; the launcher leaves out
an entry with any other value and keeps the rest.

`.desktop` files are named `kid-<id>.desktop` and carry `X-Kid-Category` and
`X-Kid-Engine` keys.

## Contract

What a reader of the manifest can count on (#24):

- **`version`** is 1. A reader refuses a file with any other version as a
  whole. A new key that a reader may ignore keeps version 1, as `picture`
  did; a key whose meaning changes makes version 2.
- **`id`** comes from the source, never from the title: `steam-<appid>`
  for a game that starts through Steam, and `steam-<appid>-<target>` for a
  game native ScummVM starts, with the target in lowercase letters,
  digits and hyphens. It is unique within one file, because each appid is
  scanned once and each ScummVM game in it has its own target. It stays
  the same when a game's name changes.
- **`title`** is the name a child sees on the tile and a screen reader
  says: Steam's name for the game, its folder's name if Steam has none, or
  ScummVM's description when one Steam app holds several games. A
  launcher leaves out an entry with no title.
- **`category`** is the tile's kind: `make`, `practice`, `games` or
  `machine` (ADR-0024). kidscan writes `games` unless `--categories` says
  otherwise. A launcher leaves out an entry with any other value.
- **`exec`** is the program and its arguments, as a list, run as it is,
  never through a shell. It is `steam -applaunch <appid>`, or ScummVM
  with `--fullscreen`, `--path=<the game's folder>` and the target. The
  launcher's games file takes only the first form.
- **`picture`**, when present, is the full path of a plain file that was
  there when kidscan ran. kidscan never opens it. With no `--pictures`,
  or no file for that game, the key is absent.
- `scummvm` is the ScummVM program used, or `null` when none was.
  `steam_roots` lists what was scanned. `engine`, `source` and
  `needs_steam_running` describe the entry and choose nothing.

## Tests

```sh
python3 tools/kidscan/tests/test_kidscan.py
```

The tests build a synthetic Steam library in a temp directory: two library
folders, a denylisted redistributable, a Proton build, a manifest with no
install directory, a title present in both libraries, and an escaped quote in
a VDF value. ScummVM detection is exercised with a fake `scummvm` that prints
a detect table. No real Steam or ScummVM install is required.
They also cover the ids, `--library`, `--pictures` and the atomic `-o`,
and `ManifestRulesTests` gives each rule in step 3 a manifest that breaks
it.

CTest also runs the round trip (#97): `tests/roundtrip_manifest.py` writes a
manifest from the same synthetic library, and the launcher's
`tst_kidscanmanifest` reads it, so the two halves cannot drift apart.
Given a second path, it also writes a games file as provisioning does,
with stand-in pictures beside it that are a few bytes and never real game
art, and the launcher reads that as its games file.

## Known limitations (tracked in the issue tracker)

- **Flatpak Steam.** The Flatpak root is scanned, but the launch command is a
  bare `steam -applaunch`, which does not exist on a Flatpak-only machine. It
  should become `flatpak run com.valvesoftware.Steam -applaunch <appid>` when
  the root came from `~/.var/app/`.
- **Flatpak ScummVM.** `shutil.which("scummvm")` will not find
  `org.scummvm.ScummVM`; pass `--scummvm` with a wrapper for now.
- **Detect output parsing** was verified on 2026-09-08 against ScummVM
  2.9.1 on Fedora 44 with four Humongous titles from a real Steam library;
  the test still uses a fake that prints the same three-column table.
- **Naming.** `kid-` and `X-Kid-` predate the Cairn name; rename to
  `cairn-`/`X-Cairn-` when the launcher's manifest reader lands, in one
  change.
- **Not yet built (Phase 3):** category-assignment GUI, pictures for tiles
  other than games (DESIGN §14 Q3; games have theirs, ADR-0035).
  Launch-timeout handling lives in the launcher, not here.
