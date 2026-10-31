# brand

The Cairn brand as code. `docs/brand-guide/` is the human-readable
guideline (a design canvas, v0.1); this directory is what programs import.

| File | Use |
|---|---|
| `tokens.json` | Source of truth: palette, semantic roles, type scale, radii, mark geometry, measured contrast ratios. Edit this. |
| `tokens.css` | CSS custom properties generated from `tokens.json`, for web surfaces and docs. |
| `qml/Cairn/Brand/Tokens.qml` | QML singleton generated from `tokens.json`. Every first-party Qt/QML surface uses this and nothing else for colour, type, radius or motion: `import Cairn.Brand`, then `Tokens.ink`, `Tokens.makeLabel`, `Tokens.radiusTile`. |
| `qml/Cairn/Brand/qmldir` | Generated module description for tools that import QML from `brand/qml`. |
| `qml/Cairn/Brand/CMakeLists.txt` | Generated `qt_add_qml_module` declaration for compiled applications. |
| `tests/` | Python generator tests and QML tests for both import routes. |
| `build.py` | The generator and validator. Edit `tokens.json`, run it, then run it again with `--check`. |
| `mark.svg` | The mark, variant C "Trail stack", in Ink. For light grounds. |
| `mark-on-ink.svg` | The mark in Sand. For Ink grounds: terminal, login. |
| `mark-currentcolor.svg` | The mark filled with `currentColor` for inline use in HTML. |
| `image/cairn-logo.svg` | The mark in Ink for the image's icon theme, in a square box with one base-stone height of clear space. |
| `image/cairn-logo-on-ink.svg` | The same in Sand. The image's logo for dark grounds: the Guardian's start button, the splash, System Settings' About page. |
| `image/ansi-color` | The colour of the system's name on the console, from `semantic.console`, in the form `os-release` wants. |
| `image/plymouth/` | The boot screen, a Plymouth theme (ADR-0044): one PNG per stone, the password box and its dot, the theme file and the script. Generated; the image installs it and builds its initramfs with it. |
| `boot_screen.py` | Draws `image/plymouth/` for `build.py`: the PNGs with the standard library, and the script's numbers from the tokens. |
| `boot-screen.script` | The boot screen's script in Plymouth's own language, by hand. `build.py` puts the token numbers above it. |

## Rules that code must keep

- **Colour codes kind, never app.** Ochre = make, moss = practice,
  paper = games, fjord = machine. Use `--cairn-make` etc., not the raw hue
  names, in UI. A games tile is a quiet card, so it carries a hairline edge
  (`--cairn-games-edge`, `--cairn-stroke-hairline`) and no colour of its own
  (ADR-0024).
- **Label colour follows the tokens.** `--cairn-on-practice` is Ink, not
  Sand. Ink on Moss measures 4.01:1; Sand on Moss measures 2.83:1, below
  the project's 3:1 label minimum. The tokens correct the v0.1 sketch.
- **The focus ring follows the ground.** On Sand and Paper it is
  `--cairn-focus` (`Tokens.focus`), Fjord: 5.77:1 and 6.29:1. On Ink it
  is `--cairn-focus-on-ink` (`Tokens.focusOnInk`), Sky: 7.45:1.
  In CSS, `.cairn-on-ink` switches the ring for you; in QML, use the one
  for the ground the ring is drawn on. Sky on Sand is 1.52:1 and Fjord on
  Ink 1.97:1, below the 3:1 WCAG 1.4.11 asks of a focus ring. The tokens
  correct the v0.1 guide, which gives Sky for focus everywhere (#216).
- **Ochre and Moss are never text.** They are fills for large shapes only.
- **Body text is Ink on Sand** (11.4:1; the guide's 12.9:1 figure is a
  little high). Child-facing text is 18px minimum; Guardian and docs 16px.
- **One family.** Atkinson Hyperlegible Next, weights 400 and 700 only; the
  Mono for the terminal. Fonts must ship in the image, not load from the web.
- **The mark is one ink.** Never outlined, rotated, gradient-filled, or
  given a face. Keep one base-stone height of clear space on all sides.
- **Icons are square.** Icon loaders draw an SVG into a square and would
  stretch the 120×110 mark, so `image/` uses a square box. The stones
  fill 82 of its 120 units, so the mark keeps its 16 px minimum only in
  an icon of 24 px or more.
- `mark-currentcolor.svg` is for HTML only. Plasma recolours only its own
  colour classes, not `currentColor`, so the image uses the two fixed
  colours.
- **No gradients, ever.**

## Regenerating

```sh
python3 brand/build.py
python3 brand/build.py --check
```

The first command writes `tokens.css`, the QML singleton and module files,
the three mark SVGs, the image's two icons and console colour, and the
boot screen.
The second checks every recorded contrast figure, the 3:1 label policy,
reserved QML names, and byte-for-byte generated output. A PNG is
compared by its pixels, since another zlib may compress them to other
bytes.
Both commands use the Python standard library only.

Applications compile the module in with
`add_subdirectory(brand/qml/Cairn/Brand)` and link `CairnBrand`.
The generated `CMakeLists.txt` sets the singleton flag so tokens can never
load as undefined.
The `brand/qml` import path with `qmldir` is only for tools and tests
(`qmllint`, `qmltestrunner`, and the QML runner).

CSS keeps the `--cairn-on-*` label names. QML puts the ground first so the
names cannot be mistaken for signal handlers: `Tokens.practiceLabel` maps to
`--cairn-on-practice`, with matching `makeLabel`, `machineLabel`, `inkLabel`,
`sandLabel`, and `paperLabel` properties.

## Trademarks

The name **Cairn Linux**, the word **cairn** as the name of this project, and
the **stacked-stones mark** in this directory are project trademarks. The SVG
artwork is covered by CC BY-SA 4.0, but that copyright licence does not grant
trademark rights or permission to imply project endorsement.

The full [`TRADEMARKS.md`](../TRADEMARKS.md) policy explains permitted
references, modified builds, permission requests, and current stewardship.
The shape, contrast, and clear-space rules above apply whenever the policy
allows use of the mark.
