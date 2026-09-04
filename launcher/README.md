# launcher

The fullscreen tile launcher for L1 and L2 sessions. **The launcher is the
product** (DESIGN §6.1); everything else is packaging.

Nothing here yet. Phase 0 task **P0-5** builds v0. The technology decision is
**ROADMAP D2**: prototype as web content in an embedded WebKitGTK view hosted
by a small Python + GTK4 program, measure memory on the 2 GB tier, and fall
back to Qt/QML if it's too heavy.

## Requirements carried from the design

- Fullscreen grid of large tiles. No window management, taskbar, or
  filesystem view at L1. L2 adds a files view to the dock.
- No reading required beyond app names, paired with distinct icons.
- **Colour codes kind, never app**: ochre = make, moss = practice,
  fjord = machine. Tokens come from `../brand/tokens.css` only.
- Watches launches. Slow or failed launches show "Something needs a
  grown-up", never whatever the app or Steam decided to display.
- Keyboard and mouse both work fully. A child typing at one character per
  five seconds is expected.
- Nothing animated for its own sake. Launch feedback is the one motion that
  earns its place.

## Not the launcher's job

Session dispatch (`../session/`), the shell (`../shell/`), account and level
management (Guardian tooling, Phase 1).
