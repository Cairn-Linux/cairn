# brand

The Cairn brand as code. `docs/brand-guide/` is the human-readable
guideline (a design canvas, v0.1); this directory is what programs import.

| File | Use |
|---|---|
| `tokens.json` | Source of truth: palette, semantic roles, type scale, radii, mark geometry, measured contrast ratios. Edit this. |
| `tokens.css` | CSS custom properties generated from `tokens.json`, for web surfaces and docs. |
| `qml/Cairn/Brand/Tokens.qml` | QML singleton generated from `tokens.json`. Every first-party Qt/QML surface uses this and nothing else for colour, type or radius: `import Cairn.Brand` with `brand/qml` on the QML import path, then `Tokens.ink`, `Tokens.onMake`, `Tokens.radiusTile`. |
| `build.py` | The generator. Edit `tokens.json`, run `python3 brand/build.py`, commit everything it touched. |
| `mark.svg` | The mark, variant C "Trail stack", in Ink. For light grounds. |
| `mark-on-ink.svg` | The mark in Sand. For Ink grounds: terminal, login, boot splash. |
| `mark-currentcolor.svg` | The mark filled with `currentColor` for inline use in HTML. |

## Rules that code must keep

- **Colour codes kind, never app.** Ochre = make, moss = practice,
  fjord = machine. Use `--cairn-make` etc., not the raw hue names, in UI.
- **Label colour follows the tokens.** `--cairn-on-practice` is Ink, not
  Sand: Sand on Moss measures 2.83:1 and fails WCAG AA even for large text.
  The v0.1 launcher sketch gets this wrong; the tokens are the correction.
- **Ochre and Moss are never text.** They are fills for large shapes only.
- **Body text is Ink on Sand** (11.4:1; the guide's 12.9:1 figure is a
  little high). Child-facing text is 18px minimum; Guardian and docs 16px.
- **One family.** Atkinson Hyperlegible, weights 400 and 700 only; the Mono
  for the terminal. Fonts must ship in the image, not load from the web.
- **The mark is one ink.** Never outlined, rotated, gradient-filled, or
  given a face. Keep one base-stone height of clear space on all sides.
- **No gradients, ever.**

## Regenerating

```sh
python3 brand/build.py
```

Writes `tokens.css`, `qml/Cairn/Brand/Tokens.qml`, `qmldir` and the three
SVGs. Standard library only. The QML singleton exposes the same semantic
names as the CSS in camelCase (`onPractice` for `--cairn-on-practice`).

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
