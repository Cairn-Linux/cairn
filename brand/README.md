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
| `mark-on-ink.svg` | The mark in Sand. For Ink grounds: terminal, login, boot splash. |
| `mark-currentcolor.svg` | The mark filled with `currentColor` for inline use in HTML. |

## Rules that code must keep

- **Colour codes kind, never app.** Ochre = make, moss = practice,
  paper = games, fjord = machine. Use `--cairn-make` etc., not the raw hue
  names, in UI. A games tile is a quiet card, so it carries a hairline edge
  (`--cairn-games-edge`, `--cairn-stroke-hairline`) and no colour of its own
  (ADR-0024).
- **Label colour follows the tokens.** `--cairn-on-practice` is Ink, not
  Sand. Ink on Moss measures 4.01:1; Sand on Moss measures 2.83:1, below
  the project's 3:1 label minimum. The tokens correct the v0.1 sketch.
- **Ochre and Moss are never text.** They are fills for large shapes only.
- **Body text is Ink on Sand** (11.4:1; the guide's 12.9:1 figure is a
  little high). Child-facing text is 18px minimum; Guardian and docs 16px.
- **One family.** Atkinson Hyperlegible Next, weights 400 and 700 only; the
  Mono for the terminal. Fonts must ship in the image, not load from the web.
- **The mark is one ink.** Never outlined, rotated, gradient-filled, or
  given a face. Keep one base-stone height of clear space on all sides.
- **No gradients, ever.**

## Regenerating

```sh
python3 brand/build.py
python3 brand/build.py --check
```

The first command writes `tokens.css`, the QML singleton and module files,
and the three SVGs. The second checks every recorded contrast figure, the
3:1 label policy, reserved QML names, and byte-for-byte generated output.
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
