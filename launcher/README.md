# launcher

The tile launcher for future L1 and L2 sessions. **The launcher is the
product** (DESIGN §6.1); everything else is packaging.

This first slice of issue #4 / **P0-5** is a **C++20 / Qt 6 / QML** window
with six tiles, keyboard navigation and mouse focus.
Enter and Space do nothing yet; launching apps is a later slice.
Colours, type, focus rings and radii come from `Cairn.Brand.Tokens`.

## Build and run on the dev PC

From the repository root, with the tools in `docs/DEVELOPMENT.md` available:

```sh
cmake --preset debug && cmake --build --preset debug && ctest --preset debug
./build/debug/launcher/cairn-launcher
```

On this dev PC, gcc's sanitizer runtimes are missing, so select clang on the
first configure with `CC=clang CXX=clang++ cmake --preset debug`.
The presets do not pin a compiler.
Debug enables AddressSanitizer and UndefinedBehaviorSanitizer.
Use `cmake --preset release && cmake --build --preset release` for a build
without sanitizers.
CTest runs the model, compiled brand tokens and QML navigation tests offscreen.

This is a normal window under Plasma, not a kiosk.
There is no `--fullscreen` option.
Arrow keys move between tiles; Tab and Shift-Tab wrap through all six.
Clicking a tile gives it focus.
Atkinson Hyperlegible Next falls back to the system font when not installed.

Refresh the English translation catalogue with
`cmake --build --preset debug --target launcher_lupdate`.

## Requirements carried from the design

- Fullscreen grid of large tiles. No window management, taskbar, or
  filesystem view at L1. L2 adds a files view to the dock.
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
