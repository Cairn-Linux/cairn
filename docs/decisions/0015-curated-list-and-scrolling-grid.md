# ADR-0015: Nothing is bundled beyond the frame; the front door is a flat grid that scrolls

**Status:** accepted
**Date:** 2026-09-05
**Amends:** ADR-0009 (the catalogue survey becomes the list itself)

## Context

[DESIGN §5.1](../DESIGN.md#51-the-makepractice-split) keeps GCompris and the
Tux suite "in a folder (practice)", bundled, "not the identity of the
system". Run on the dev PC, a Practice tile that opens GCompris still reads
as a native part of Cairn, and a Build tile that opened the whole of it read
as the themed activity pack the design set out to avoid.
[DESIGN §3.2](../DESIGN.md#32-the-guardian-role) already gives Guardians a
per-child app allowlist;
[§4.2](../DESIGN.md#42-application-delivery-flatpak) delivers apps by
Flatpak;
[§9.1](../DESIGN.md#91-two-complementary-control-layers) gates them with
malcontent; the `kidscan` manifest is how a program becomes a tile.
[DESIGN §6.1](../DESIGN.md#61-the-launcher) says L1 is about six tiles and
"nothing to get lost in", and that the grid grows only at L2, which leaves no
room for the Steam titles a parent has approved
([§8.6](../DESIGN.md#86-bring-your-own-library)).
Principle 4 bans a storefront in front of a child; it says nothing against
one in front of a parent.

## Decision

**Nothing is bundled beyond the frame.** The image ships the launcher,
Footpath, the session and the Guardian tooling. Every other program a child
can open is one a Guardian added on purpose, from a **curated list** kept in
this repository: one entry per app with its Flatpak id, kind, level fit and
one honest sentence. The Guardian tool installs from the list, allows the app
for a child, and adds the tile to that child's manifest. Nothing on the list
is maintained by this project. The catalogue survey (issue #26) becomes the
list's first contents. GCompris is on the list, not in the image.

Tux Paint is on the list too, not in the image. So that Draw works on day
one, the first-boot wizard (DESIGN §4.3.1) proposes a **starter set** from
the list with Tux Paint pre-ticked; the Guardian can untick it. Which apps
make up the starter set is settled by testing, not here.

Two things the image carries regardless, because the base image does: the
Steam client, which starts silent at login as §8.3 requires, and its
Proton runtime. ScummVM is installed with the first ScummVM game a Guardian
adds, not up front.

**The front door stays flat.** At L1 there are no folders and no sub-menus.
When a child has more tiles than fit, the **grid scrolls**: by mouse wheel
and by arrow keys, whole rows at a time, with the next row visible at the
edge so that "more" is seen, not guessed. The Guardian chooses what appears.
Scrolling with a wheel or a key, where the screen moves and the hand does
not, is a lesson of its own and a different one from swiping a tablet.
Folders arrive with the dock at L2.

## Consequences

- The image gets smaller and the front door is honest: on a fresh install a
  child sees Draw because the Guardian left it ticked at first boot, and
  nothing else unless the Guardian added it.
- Approved Steam titles, ScummVM games and installed apps are all tiles in
  one grid; the launcher's colour by kind still says what each is.
- Installing needs a network; a Guardian's action, failing calmly without
  one, as ADR-0007 allows. Nothing a child does needs it.
- P1-5's Guardian tooling gains `install-app` and `remove-app` on top of
  `allow-app`; the list lives at `catalogue/` when that work starts. P1-6's
  wizard gains the starter-set step, which needs a network and says so.
- The launcher's grid gains scrolling (issue in `launcher/`); the Phase 0
  child test sees whichever tiles the dev-PC manifest names.
- ADR-0009 stands: still no first-party app. The list is data, not code.
