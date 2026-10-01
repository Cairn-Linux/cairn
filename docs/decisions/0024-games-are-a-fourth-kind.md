# ADR-0024: Games are a fourth kind

**Status:** accepted
**Date:** 2026-09-30

## Context

[DESIGN §6.1](../DESIGN.md#61-the-launcher) colours tiles by kind (make,
practice, machine) and lets the Guardian put approved Steam titles in the
grid, but no kind fits a game.
`tools/kidscan` wrote every title as `play`, which the launcher's manifest
reader did not know, so one game tile refused the whole manifest (#97).
In the VM the Putt-Putt tile was tagged `make` by hand to get past that, and
the Terminal then listed it under `make` beside Draw (#76).
The kind is more than a colour: Footpath files the doors into one folder per
kind (ADR-0013), so the kind's name is a word a child types.

The brand guide's launcher sketch already draws a game: "Freddi Fish" as a
Paper card with a Line hairline and an Ink label, quiet beside the ochre
make tiles.
On the Terminal's Ink ground every chip is a colour, and Paper is already a
note's, Sky a folder's.

## Decision

Games are a fourth kind, `games`: the manifest's category, the launcher's
`TileModel::Kind::Games` and Footpath's door kind and folder, between
`practice` and `machine`.
The maintainer chose the noun over kidscan's `play`; `cd games` reads as a
place.
A games tile is a Paper card with an Ink label (12.37:1) and a Line hairline
edge, as the sketch draws it: `semantic.kind.games`, `semantic.on.games`,
`semantic.edge.games` and `stroke.hairline` in `brand/tokens.json`.
No colour is added, and making stays the loudest thing in the grid
(DESIGN §2, principle 1).
In the Terminal a games chip is an outline in the games colour, so a game
never looks like a note.
A games tile behaves like any other: the launcher watches for its window,
shows the grown-up screen if none comes, and Super+Q leaves it (ADR-0019).
A tile and a door are shortcuts; a game's files stay where its store put
them.
kidscan writes `games` unless a Guardian's category map says otherwise, and
a manifest entry the launcher cannot use now costs only its own tile.

## Consequences

- Footpath 0.2.0 carries the kind (Cairn-Linux/footpath#5); Cairn's pin
  moves to that tag.
- Checked in the VM on 2026-09-30: kidscan, run as the child against her
  Steam library, found Putt-Putt Joins the Parade and routed it to native
  ScummVM. Its tile was a Paper card, opened the game fullscreen with no
  Steam client, and Super+Q returned to the tiles. In the Terminal, `ls`
  showed `make practice games home`, `cd games` and `ls` listed the game
  with an outlined chip, and `open` started it.
- The grid tile's edge is faint on Sand. That is the sketch's choice, and
  the child test (P0-9) shows whether a five-year-old sees it as a button.
- Store titles make long names to type, such as
  `putt-putt-joins-the-parade`. Short names stay with #76.
- A game reaches the tiles by copying kidscan's entries into the manifest
  by hand. Where the manifest lives and who runs the scanner stay with #24.
- The fourth-kind questions in #20 and #24, and #76's kind-or-folder
  question, are answered here.
