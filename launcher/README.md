# launcher

The fullscreen tile launcher for L1 and L2 sessions. **The launcher is the
product** (DESIGN §6.1); everything else is packaging.

Nothing here yet. Phase 0 task **P0-5** builds v0 as a **C++20 / Qt 6 / QML**
application (ADR-0002). This is the first C++ in the repo, so P0-5 also sets
up CMake, Qt Test, sanitizers and clang-format for everything that follows.
Colours, type and radii come from the `Cairn.Brand.Tokens` QML singleton.

## Requirements carried from the design

- Fullscreen grid of large tiles. No window management, taskbar, or
  filesystem view at L1. L2 adds a files view to the dock.
- No reading required beyond app names, paired with distinct icons.
- **Colour codes kind, never app**: ochre = make, moss = practice,
  fjord = machine. Use `Tokens.make` / `Tokens.onMake` and friends from
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
