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
CTest runs seven suites offscreen: the tile model, the manifest reader, the
app launcher, the grid scroller, the window list, the compiled brand
tokens, and the QML navigation, scrolling, grown-up-screen and terminal
behaviour. Footpath's own six
suites run in its repository.
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
`entries` array of `{title, category, exec}`.
`category` must be `make`, `practice` or `machine`; kidscan's `play` is
refused until the fourth tile kind is decided (issue #20).
`exec` is the program and its arguments as a list; an empty list means
nothing is set up for that tile yet, and launching it shows the grown-up
screen.
Without `--manifest` the six built-in tiles appear, none of which launches
anything.
A manifest that cannot be read is reported in the terminal for the parent,
and the child sees the built-in tiles.
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
A program that quits cleanly, or that quits with an error after the window,
returns to the tiles without comment.
The launcher runs one program at a time; a second Enter while one is
starting or running is ignored.
The program's own terminal output goes to the launcher's terminal, never to
the child's screen.

## Windows that open on their own

Under a Wayland compositor that offers `ext-foreign-toplevel-list-v1`
(labwc 0.8.2 and later), the launcher hears about every window that opens or
closes, with its app id and title.
A window that opens while the tiles are up, and is not the launcher's own,
is one nobody asked for: a Steam update, a sign-in prompt, a stray dialog.
The grown-up screen names it ("Steam opened on its own.") with no Back tile,
because a child cannot dismiss what a grown-up has to see, and the tiles
return when the window closes.
Windows that open while a launched program is starting or running belong to
that program and are left alone.
The launcher's own windows carry the app id `cairn-launcher`, the name of
the executable, which is what Qt reports when no desktop file is set.

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
This passed on the dev PC with labwc 0.9.6 on 2026-09-04.

Known gap (issue #42): `steam -applaunch` returns at once, so a game window
that opens after the process has exited would be treated as unexpected.
The P0-10 spike designs the window-based lifecycle for that case.

## Requirements carried from the design

- Fullscreen grid of large tiles. No window management, taskbar, or
  filesystem view at L1. Flat: no folders. More tiles than fit means the
  grid scrolls by wheel and arrow keys, whole rows at a time, with the next
  row showing at the edge (ADR-0015). L2 adds a files view to the dock.
- No reading required beyond app names, paired with distinct icons.
- **Colour codes kind, never app**: ochre = make, moss = practice,
  fjord = machine. Use `Tokens.make` / `Tokens.makeLabel`,
  `Tokens.practice` / `Tokens.practiceLabel`, and
  `Tokens.machine` / `Tokens.machineLabel` from
  `../brand/qml/Cairn/Brand/Tokens.qml`, never a literal.
- Watches launches. Slow or failed launches show "Something needs a
  grown-up", never whatever the app or Steam decided to display.
- Keyboard and mouse both work fully. A child typing at one character per
  five seconds is expected.
- Nothing animated for its own sake. Launch feedback is the one motion that
  earns its place.

## Not the launcher's job

Session dispatch (`../session/`), the shell (`../shell/`), account and level
management (Guardian tooling, Phase 1).
