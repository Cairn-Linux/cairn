# launcher

The tile launcher for future L1 and L2 sessions. **The launcher is the
product** (DESIGN §6.1); everything else is packaging.

Issue #4 / **P0-5**, built in slices as a **C++20 / Qt 6 / QML** window.
Colours, type, focus rings and radii come from `Cairn.Brand.Tokens`.

- **Slice 1 (2026-09-04):** six tiles, keyboard navigation, mouse focus.
- **Slice 2:** tiles launch programs from a manifest; a failed launch shows
  "Something needs a grown-up" instead of whatever the program printed.
- **Slice 3:** the launcher hears from the compositor when any window opens
  or closes, and a window nobody launched shows the grown-up screen until it
  is closed.
- **Slice 4:** fullscreen under labwc, through the kiosk configuration in
  `../session/labwc/`. No launcher code changed.
- **Slice 5 (2026-09-05):** the Terminal tile opens the restricted shell
  inside the window; `open` there launches through the same path and the
  same grown-up screen; `exit` or Escape returns to the tiles.
- **Slice 6 (2026-09-05):** the shell is Footpath from its own repository
  (`../external/footpath`, ADR-0014). `DoorsFromTiles` hands it the tiles
  as doors and `Main.qml` binds its look to the brand tokens.
- **Slice 7 (2026-09-05):** the grid scrolls (ADR-0015). `GridScroller`
  decides which rows are in the window; wheel and arrow keys move the
  focus one row at a time and the window follows by whole rows, the next
  row peeking at the edge.
- **Slice 8 (2026-09-08):** the launcher tracks a launched app by its
  window, not its process (#42). `flatpak run` and `steam -applaunch`
  return within a second while the app runs on, so `AppLauncher` waits for
  the app's window, stays Running while it is up, and returns to the tiles
  when it closes. A launch that opens no window before a grace timer ends
  goes back to the tiles quietly (ADR-0030 later made that the grown-up
  screen); a window that opens when nothing was
  launched is still an interruption.
- **Slice 9 (2026-09-29):** a close request is one step back (ADR-0019).
  The child's leave key, Super+Q, asks the window in front to close, and
  when that is the launcher the request arrives at its window.
  `CloseRequest` answers it as Escape would on the screen that is showing:
  the Terminal goes back to the tiles, a failed launch goes back to where
  the child was, and at the tiles nothing happens. A fullscreen launcher
  never closes, because the kiosk session ends with it; a windowed one,
  under Plasma, closes as usual.
- **Slice 10 (2026-09-30):** a child logs themselves out (ADR-0021, #44).
  `LogOut` runs the program the kiosk wrapper names with `--log-out`
  (`../session/bin/cairn-log-out`); without it there is no Log out. The
  button sits above the tiles, Up from the top row reaches it, and
  `LogOutScreen.qml` asks once with the focus on Back.
- **Slice 11 (2026-10-07):** a pressed tile answers at once (ADR-0028,
  #129). `StartingScreen.qml` stands in front of the tiles, or the
  Terminal, while `AppLauncher` is Starting: the Cairn mark builds itself
  a stone at a time from the brand tokens, over "Putt-Putt Joins the Circus
  is starting.", and the screen goes when the app's window has stayed up
  for three seconds, so a game's splash does not take it away.
- **Slice 12 (2026-10-18):** the battery on the top bar (ADR-0034, #43).
  `UPowerDevice` reads UPower's `DisplayDevice` on the system bus;
  `BatteryLook` decides; `Battery.qml` draws the fill, a plug on the
  charger, and one sentence when UPower says it is low. No battery,
  nothing.
- **Slice 13 (2026-10-19):** game tiles from the shared folder, each with
  the game's picture (ADR-0033, ADR-0035, #138). `--games` names the games
  file provisioning makes. `TileFiles` reads it and the manifest, for
  `TileModel` and for the Steam-at-login check; `GamesFile` holds the
  games file's rules; `TilePicture` checks a `picture`. `Tile.qml` draws
  the picture inside the card in place of the title, and the title comes
  back when there is no picture or Qt cannot draw it.
- **Footprint (2026-09-08):** about 145 MB proportional idle in the VM,
  first frame 0.2 s after exec; the table is in
  `../docs/research/launcher-footprint.md`.

## Build and run on the dev PC

From the repository root, with the tools in `docs/DEVELOPMENT.md` available:

```sh
cmake --preset debug && cmake --build --preset debug && ctest --preset debug
./build/debug/launcher/cairn-launcher --manifest launcher/manifests/dev-pc.json
```

On this dev PC, gcc's sanitizer runtimes are missing, so select clang on the
first configure with `CC=clang CXX=clang++ cmake --preset debug`.
The presets do not pin a compiler.
Debug enables AddressSanitizer and UndefinedBehaviorSanitizer.
Use `cmake --preset release && cmake --build --preset release` for a build
without sanitizers.
In the image, `cmake --install` puts the launcher in `/usr/libexec/cairn/`,
`libCairnBrand.so` in `/usr/lib64/cairn/`, which the launcher finds through
its rpath, and a wrapper made from `bin/cairn-launcher.in` in `/usr/bin/`
(ADR-0038). The session runs the wrapper, which names the logged-in
child's own games list (ADR-0041).
The launcher has twenty-three CTest tests, run offscreen: the wrapper the
kiosk session starts, run with stand-ins, the tile model, the
manifest reader, the manifest and the games file read together (the tile
files), kidscan's manifest and games file written and read back (two
tests), the app launcher, the windows of a launch, the wait for a Steam
game, Steam's launch log, Steam starting or updating itself, the give-up
watch, Steam at login, the app scope, the grid
scroller, the close request, Log out, the battery's look, the UPower
reader (against a stand-in UPower on a bus of its own, under
`dbus-run-session`), the window list, the compiled brand tokens, the
Terminal's doors, and the QML navigation, scrolling, starting-screen,
grown-up-screen, terminal, close-request, Log out, battery,
tile-picture and plain-text behaviour. Footpath's own six suites run in
its repository.
Qt on Fedora sends `qWarning` and `qInfo` lines to the journal when stderr
is not a terminal; set `QT_FORCE_STDERR_LOGGING=1` to see them in a pipe.

This is a normal window under Plasma, not a kiosk.
There is no `--fullscreen` option: fullscreen is the compositor's decision,
made by the window rule in `../session/labwc/rc.xml`, so the same program
is a window under Plasma and the whole screen under labwc.
To see that on the dev PC, run it inside nested labwc with that
configuration:

```sh
export QT_FORCE_STDERR_LOGGING=1
labwc -C session/labwc -S './build/debug/launcher/cairn-launcher \
  --manifest launcher/manifests/dev-pc.json'
```

The compositor ends when the launcher does.
Arrow keys move between tiles; Tab and Shift-Tab wrap through all of them
(six by default).
With more than six tiles the grid shows two rows and the top of a third, and
slides by whole rows to keep the focused tile in view; a wheel notch moves
the focus one row. Try it with
`--manifest launcher/tests/fixtures/manifest-many.json` (fourteen tiles).
Add `--games launcher/tests/fixtures/games.json` for three made-up games,
with no pictures, after the manifest's tiles.
Enter, Space or a click launches the tile's program.
Escape or the Back tile leaves the grown-up screen.
The Terminal tile is the launcher's own, always last, and opens Footpath
(`../external/footpath/README.md`) over the tiles; a manifest never names
it.
Atkinson Hyperlegible Next falls back to the system font when not installed.

Refresh the English translation catalogue with
`cmake --build --preset debug --target launcher_lupdate`.

## The manifest

`--manifest <file>` reads the version-1 JSON that `tools/kidscan` writes: an
`entries` array of `{title, category, exec}`, each with an optional
`picture`.
`category` is `make`, `practice`, `games` or `machine` (ADR-0024); kidscan
writes `games`.
An entry with any other category, no title or no `exec` list is left out,
with one sentence in the terminal for the parent, and the rest still load.
`exec` is the program and its arguments as a list; an empty list means
nothing is set up for that tile yet, and launching it says so: "Music is
coming soon. It is not ready yet. Try something else.", with a Back tile.
That is not a grown-up's job, so it does not say one is needed.
Without `--manifest` the six built-in tiles appear, none of which launches
anything.
A manifest that cannot be read, or has no entry the launcher can use, is
reported for the parent, and the child sees the built-in tiles.
Under a terminal that report is on its screen; in the kiosk, everything the
launcher reports is in the journal, `journalctl -t cairn-launcher` (#122).
kidscan's entries can still be copied into the manifest by hand; the round
trip is tested (`tst_kidscanmanifest`, #97).
On a provisioned machine the games come from the games file instead.

A `picture` is the full path of a picture file on this computer, such as
`/var/lib/cairn/game-pictures/294660.jpg` (ADR-0035).
The tile then shows the picture inside the card in place of its title;
the screen reader still reads the title.
A `picture` that is not text, is empty, is relative, is a URL, names
something in the launcher's own resources, or is not a file that exists,
keeps the tile without a picture, with one sentence for the parent.
A picture Qt cannot draw shows the title.

`--games <file>` reads a games file.
Provisioning's wrapper names the one it makes from the shared Steam
folder, `/var/lib/cairn/games.json` (ADR-0033).
The image's wrapper, `bin/cairn-launcher.in`, names the child's own list,
`/var/lib/cairn/games/<login>.json`, which the Guardian tool's Save
writes (ADR-0041); it takes the login from `id -un`, and passes no
`--games` for a login outside `cairn-family`'s rule, so a name can never
lead out of that folder.
A child with no list yet has no game tiles and, with no Steam tile in the
manifest, no Steam at login.
It is the same version-1 format, and its tiles come after the manifest's
and before the Terminal.
Without a manifest, or with one that cannot be used, they follow the
built-in tiles.
No file, or no entries, means no game tiles and is not an error.
A file that cannot be opened, is not valid JSON or is not version 1 costs
only the game tiles, with one sentence; the manifest's tiles still show.
An entry is taken only when its category is `games`, its title is not
empty, and its `exec` is exactly `steam`, `-applaunch` and an app id with
no leading zero.
Any other entry is left out with one sentence, so nothing in this file can
choose a program.
A game the manifest already starts with `steam -applaunch` (or
`/usr/bin/steam -applaunch`) keeps the manifest's tile, and the games
file's is left out without a word.
So is a second entry for a game the file has already given a tile.
Such an entry is left out before it is checked, so the parent never reads
a sentence about a tile that is not there.
Every title is shown as plain text: a game's name comes from Steam, and
markup in it is shown as written, never read, so it cannot load a picture
from the network.
`TileFiles` reads both files for the tiles and for the Steam-at-login
check, so the two always agree.
`manifests/dev-pc.json` names Tux Paint for Draw and GCompris for Practice
only; Build, Music and Story are empty until the catalogue survey (issue
#26) picks a program for each. GCompris is a practice-folder app, never
the front door (DESIGN §5.1).

## What a launch does

The launcher starts the program and watches it for a settle window (five
seconds).
A program that cannot be started, or that quits with an error inside that
window, is a failed launch: the grown-up screen says "Something needs a
grown-up" and names the tile, with one thing to do, Back.
A launch that has put nothing on the screen when its wait ends (15 s), or
whose program runs past the settle window and quits without a window, is a
failed launch too (ADR-0030, #139): the grown-up screen says the tile did
not start. Back at the tiles a child would press it again, and again.
Until Back, a window that comes late is still the app's and takes over.
A program whose window came and went returns to the tiles without comment.
The launcher runs one program at a time; a second Enter while one is
starting or running is ignored.
From the press until the app's window is on the screen, the starting
screen says so: the Cairn mark builds itself, bottom stone first, over one
sentence naming the tile (ADR-0028). A Steam game can take several seconds
to come up, and tiles that look the same after a press read as broken.
With `--start-steam`, which the kiosk wrapper also passes, the launcher
starts the Steam client in the background at login when a tile, from the
manifest or the games file, runs `steam -applaunch`, as a service of the
child's own systemd called `cairn-steam` (ADR-0027). The first game then
starts without waiting for the client, and a child with no Steam tile
never pays its memory.
A Steam game pressed while that client is still starting, or one Steam is
still bringing up (its `reaper` runs), is waited for past the usual 15 s,
up to two minutes; `SteamGameWait` decides (ADR-0029, #132).
While Steam's own log says it is working on the game, downloading an
update or running its install script, the wait runs up to ten minutes and
the starting screen adds "Getting it ready. This can take a few minutes."
`SteamLaunchLog` reads only the lines Steam adds after the tap (ADR-0036).
Steam starting, or installing an update of itself, is work too: `SteamStartup`
reads the last part of Steam's `bootstrap_log.txt` and `console_log.txt`. A
tap made during an update is lost when Steam restarts, so once a client
started after the tap has run its own start command without taking it, the
launcher runs the tile's command again, once (ADR-0037).
While Steam updates itself, at login or after a tap, its updater puts up
windows of its own, such as "Progress" with a Cancel button. `SteamStartup`
says when Steam is updating or has just restarted after an update, and
those windows are hidden, not the grown-up screen (below, #183).
A Steam that cannot sign in never takes a tap up. `SteamSignIn` reads
Steam's `LogonFailure` lines: while Steam's log says nothing about the
game, a refused sign-in such as `Access Denied` ends the wait at the next
grace, and a Steam that cannot reach its servers gets a minute. Neither says "Getting
it ready", and the journal says which it was (ADR-0036, #182).
With `--scope-apps`, which the kiosk wrapper passes, every program starts
in a systemd user scope of its own, `cairn-app-<launcher pid>-<n>.scope`,
so the grown-up's give-up key can end it and whatever it started
(ADR-0025). The launcher still watches the program's own process.
The program's own terminal output goes to the launcher's terminal, never to
the child's screen.

## Windows that open on their own

Under a Wayland compositor that offers
`wlr-foreign-toplevel-management-unstable-v1` (labwc does), the launcher
hears about every window that opens, closes, is hidden or shown, with its
app id, its title, and the window it belongs to (ADR-0026).
`LaunchWindows` decides whose each window is, from what the compositor
says and never from a window's name:

- A hidden window never counts. labwc hides Steam's own windows the moment
  they open; one of those must not keep the tiles waiting or bring up the
  grown-up screen (#99).
- While a launch is opening, every window on the screen is the app's: a
  game may show a splash, or a window it replaces at once, before its own.
  The app is open once one of its windows has stayed up for three seconds
  (ADR-0028); until then the launcher is Starting, and a window that comes
  and goes does not end the launch. After that, a window that belongs to
  one of the app's, such as a dialog, is the app's too. While one is on the
  screen the launcher is Running; when the last one leaves, the tiles are
  back.
- While Steam is updating itself, a window that comes on the screen is its
  updater's, unless a program other than a Steam game is launched (opening
  or open), or the window belongs to one of the app's, such as its dialog;
  those are decided as below, so an app's own window is never hidden. It keeps
  no launch waiting and brings up no grown-up screen, and the launcher asks
  the compositor to minimise it, again if it comes back, so its Cancel
  button is out of the child's reach (ADR-0037, #183). The journal says
  "Steam is updating itself; its window … is hidden from the child."
- Any other window on the screen opened on its own: a stray dialog, a
  browser a game opened. The grown-up screen names it ("Steam opened on its
  own.") with no Back tile, and the tiles return when it closes.
- When the program the launcher started has run and then exits, the app is
  over. Its windows get two seconds to close; any still up count as opened
  on their own.

The grown-up's give-up key always brings the tiles back: `cairn-give-up`
writes `$XDG_RUNTIME_DIR/cairn/give-up` after it has ended the child's
programs, and `GiveUpWatch` hears it.
Each launch has its own process, so a program left from an earlier one,
such as a game that hid its window, never stops the next launch.
The launcher's own windows carry the app id `cairn-launcher`, the name of
the executable, which is what Qt reports when no desktop file is set; that
is the one place an app id decides anything.

KWin does not offer the protocol, so under Plasma the launcher says so once
in the terminal and only watches processes.
To check the window path by hand, run the launcher inside nested labwc:

```sh
export QT_FORCE_STDERR_LOGGING=1
labwc -C session/labwc -S './build/debug/launcher/cairn-launcher \
  --manifest launcher/manifests/dev-pc.json'
```

Then, from another terminal, open a window in that session
(`WAYLAND_DISPLAY=wayland-1 foot`, for instance): the grown-up screen should
name it and go away when the window is closed.
This passed on the dev PC with labwc 0.9.6 on 2026-09-04, with the earlier
protocol; the window rules of ADR-0026 were checked in the VM on
2026-10-01 (`docs/research/steam-containment.md`).

## The battery

When the computer has a battery, the bar above the tiles shows it at the
left, with Log out at the right (ADR-0034).
`UPowerDevice` reads `IsPresent`, `Type`, `Percentage`, `State` and
`WarningLevel` from UPower's `DisplayDevice` on the system bus and follows
their changes; `BatteryLook` decides what to show; `Battery.qml` draws it.
The fill shows how full the battery is, a plug shows while the charger is
in, and one sentence shows when UPower's warning level is low, critical or
action.
UPower decides what low is, from `/etc/UPower/UPower.conf`; the launcher
never changes it.
A computer with no battery, or on a UPS, shows nothing.
When UPower cannot be read, the journal says so in one sentence and the
child sees no battery until UPower's next change, which is read in full.
The bar goes with the tiles, so nothing shows over a game.
To compare what the launcher shows with UPower's own reading, run
`upower -i /org/freedesktop/UPower/devices/DisplayDevice`.

## Requirements carried from the design

- Fullscreen grid of large tiles. No window management, taskbar, or
  filesystem view at L1. Flat: no folders. More tiles than fit means the
  grid scrolls by wheel and arrow keys, whole rows at a time, with the next
  row showing at the edge (ADR-0015). L2 adds a files view to the dock.
- No reading required beyond app names, paired with distinct icons. A
  game's tile shows the game's own picture (ADR-0035); other tiles have
  none yet (DESIGN §14 Q3).
- The battery above the tiles when there is one (ADR-0034): a fill a child
  who cannot read can follow, and words only for children who read and
  for grown-ups. UPower decides when it is low.
- **Colour codes kind, never app**: ochre = make, moss = practice,
  paper with a hairline edge = games, fjord = machine. Use `Tokens.make` /
  `Tokens.makeLabel`, `Tokens.practice` / `Tokens.practiceLabel`,
  `Tokens.games` / `Tokens.gamesLabel` / `Tokens.gamesEdge`, and
  `Tokens.machine` / `Tokens.machineLabel` from
  `../brand/qml/Cairn/Brand/Tokens.qml`, never a literal.
- Watches launches. Slow or failed launches show "Something needs a
  grown-up", never whatever the app or Steam decided to display.
- Keyboard and mouse both work fully. A child typing at one character per
  five seconds is expected.
- Motion only says something true (ADR-0028): the rows sliding into view,
  the mark building while a tile starts. Never to decorate, distract,
  reward or hold attention.

## Not the launcher's job

Session dispatch (`../session/`), the shell (`../shell/`), account and level
management (Guardian tooling, Phase 1).
