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
./kidscan.py --categories categories.json   # {"294660": "play"}
```

Exit status 2 means no Steam installation was found.

## What it does

1. **Finds Steam roots.** `~/.steam/steam`, `~/.steam/root`,
   `~/.local/share/Steam`, and the Flatpak data path, plus any `--steam-root`.
   Symlinked duplicates collapse to one.
2. **Follows every library folder.** Parses `steamapps/libraryfolders.vdf`
   for secondary drives, handling both the modern nested format and the old
   bare-string one.
3. **Reads every `appmanifest_*.acf`.** Keeps only apps whose install
   directory actually exists. Drops Steamworks Redistributables, the Steam
   Linux Runtimes and Proton by appid and by name.
4. **Runs `scummvm --detect` per install directory.** Titles ScummVM
   recognises get a native launch command, one tile per detected game (a
   Steam "pack" can hold several). Everything else gets
   `steam -applaunch <appid>`.
5. **Writes the manifest**, sorted by title, and optionally `.desktop` files.

## Manifest format (version 1)

```json
{
  "version": 1,
  "steam_roots": ["/home/nora/.local/share/Steam"],
  "scummvm": "/usr/bin/scummvm",
  "entries": [
    {
      "id": "freddi-fish-scumm-freddi",
      "title": "Freddi Fish and the Case of the Missing Kelp Seeds",
      "category": "play",
      "engine": "scummvm",
      "exec": ["/usr/bin/scummvm", "--fullscreen", "--no-console", "--path=/…/Freddi Fish", "scumm:freddi"],
      "source": {"store": "steam", "appid": "294660"},
      "needs_steam_running": false
    }
  ]
}
```

`engine` is `scummvm` or `steam`. `needs_steam_running` tells the launcher
whether the silent Steam client (DESIGN §8.3) must be up before this tile can
launch. `category` defaults to `play`; the launcher maps it to a tile colour
by kind (games are neutral Paper tiles in the brand sketch).

`.desktop` files are named `kid-<id>.desktop` and carry `X-Kid-Category` and
`X-Kid-Engine` keys.

## Tests

```sh
python3 tools/kidscan/tests/test_kidscan.py
```

The tests build a synthetic Steam library in a temp directory: two library
folders, a denylisted redistributable, a Proton build, a manifest with no
install directory, a title present in both libraries, and an escaped quote in
a VDF value. ScummVM detection is exercised with a fake `scummvm` that prints
a detect table. No real Steam or ScummVM install is required.

## Known limitations (tracked in the issue tracker)

- **Flatpak Steam.** The Flatpak root is scanned, but the launch command is a
  bare `steam -applaunch`, which does not exist on a Flatpak-only machine. It
  should become `flatpak run com.valvesoftware.Steam -applaunch <appid>` when
  the root came from `~/.var/app/`.
- **Flatpak ScummVM.** `shutil.which("scummvm")` will not find
  `org.scummvm.ScummVM`; pass `--scummvm` with a wrapper for now.
- **Detect output parsing** has not been verified against a real ScummVM
  build in this repo; the test uses a fake that prints the documented
  three-column table.
- **Naming.** `kid-` and `X-Kid-` predate the Cairn name; rename to
  `cairn-`/`X-Cairn-` when the launcher's manifest reader lands, in one
  change.
- **Not yet built (Phase 3):** category-assignment GUI, icon sourcing
  (DESIGN §14 Q3). Launch-timeout handling lives in the launcher, not here.
