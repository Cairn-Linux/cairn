# brand

The Cairn brand as code. `docs/brand-guide/` is the human-readable
guideline (a design canvas, v0.1); this directory is what programs import.

| File | Use |
|---|---|
| `tokens.json` | Source of truth: palette, semantic roles, type scale, radii, mark geometry, measured contrast ratios. Edit this. |
| `tokens.css` | CSS custom properties generated from `tokens.json`, for web surfaces and docs. |
| `qml/Cairn/Brand/Tokens.qml` | QML singleton generated from `tokens.json`. Every first-party Qt/QML surface uses this and nothing else for colour, type or radius: `import Cairn.Brand`, then `Tokens.ink`, `Tokens.makeLabel`, `Tokens.radiusTile`. |
| `qml/Cairn/Brand/qmldir` | Generated module description for tools that import QML from `brand/qml`. |
| `qml/Cairn/Brand/CMakeLists.txt` | Generated `qt_add_qml_module` declaration for compiled applications. |
| `tests/` | Python generator tests and QML tests for both import routes. |
| `build.py` | The generator and validator. Edit `tokens.json`, run it, then run it again with `--check`. |
| `mark.svg` | The mark, variant C "Trail stack", in Ink. For light grounds. |
| `mark-on-ink.svg` | The mark in Sand. For Ink grounds: terminal, login, boot splash. |
| `mark-currentcolor.svg` | The mark filled with `currentColor` for inline use in HTML. |

## Rules that code must keep

- **Colour codes kind, never app.** Ochre = make, moss = practice,
  fjord = machine. Use `--cairn-make` etc., not the raw hue names, in UI.
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
the **stacked-stones mark** in this directory are trademarks of the Cairn
Linux project. They are not covered by the Apache-2.0 or CC BY-SA 4.0 licences
that cover the code and the rest of the brand assets.

You may:

- Use the name and mark to refer to this project, to link to it, or to say
  that something works with it.
- Redistribute unmodified builds and images of Cairn Linux under the name.
- Use the tokens, type scale and layout patterns in this directory for
  anything at all; they are CC BY-SA 4.0.

You may not, without permission:

- Call a modified build, fork or derivative "Cairn Linux" or "Cairn", or use
  the mark on one. Ship it under your own name; the code licence lets you.
- Use the name or mark in a way that suggests the project endorses or
  produced something it did not.
- Alter the mark (colour, proportion, outline, rotation, decoration) and
  present the result as the project's mark.

"Linux" is a registered trademark of Linus Torvalds and is used under a
pending sublicence from the Linux Mark Institute; this policy does not grant
any rights in it.
