# ADR-0035: A game's tile shows its picture from Steam

**Status:** proposed. It becomes accepted when the checks under
Consequences have run on the kids' laptop and the next child test
(ROADMAP P1-17) has asked whether the picture tells the younger child
which game it is.
**Date:** 2026-10-19
**Closes:** DESIGN §14 Q3, for games only — addresses #138; once
accepted, amends ADR-0024 (how a games tile looks)
**Amended by:** ADR-0041, proposed (pictures are also remade when a
Guardian saves a child's games); ADR-0047, proposed (a tile finds its
picture by the game's app id and the games file names none, and the
picture of a game still on a child's list stays)

## Context

On 2026-10-10 two children used the kids' laptop
(`docs/research/child-test-01.md`).
Every tile showed only a title.
The younger child cannot read yet, and could not tell one Putt-Putt game
from another.
She chose by guessing and by where a tile was (#138).
The write-up's first finding is that pictures matter more than anything
else on the tiles.

[DESIGN §6.1](../DESIGN.md#61-the-launcher) asks for "no reading required
beyond app names, which are paired with distinct icons".
No tile has an icon yet.
[DESIGN §14](../DESIGN.md#14-open-questions) Q3 asks where the art comes
from, and warns that Steam's own art "drags the store's visual language
onto the home screen".
ROADMAP P0-5 lists icons as the launcher's remaining work.

On 2026-10-19 the maintainer decided that a game's tile shows a picture of
the game, because a child who cannot read cannot pick a game by its
title.

Steam already keeps each game's library art on disk, in
`~/.local/share/Steam/appcache/librarycache/<appid>/`:

- `header.jpg`, a 460x215 JPEG of the game's box art, with the game's name
  in the game's own lettering;
- `logo.png`, only the lettering;
- `library_600x900.jpg`, a portrait cover;
- a 32 px icon, named by a hash.

For some games Steam keeps the header one level down, in a folder named
by a 40-character hash: on the dev PC 615 games had `<appid>/header.jpg`
and 34 had only `<appid>/<hash>/header.jpg`, and none had both.
Valve does not document this folder.
Steam fills it itself: on the kids' laptop each account's cache had about
800 folders, many more than the account's games.

Other games have the same 460x215 header only as `library_header.jpg`,
in either place.
On 2026-10-20 the Guardian's cache on the kids' laptop had 649 games with
only `header.jpg`, 98 with only `library_header.jpg` and none with both.
The picture step installed there that day read only `header.jpg`, so 9 of
the 25 shared games showed their names instead of their pictures.

On a 1366x768 screen a tile is about 409x286 px, or 409x223 once the
grid scrolls, as it does with the family's games.

ADR-0033 (proposed) decided where the game tiles come from: a games file
that provisioning makes from the shared Steam folder, which a re-run of
provisioning keeps.
This change builds that too.
ADR-0024 decided how a games tile looks: a quiet Paper card with a
hairline edge, so that making stays the loudest thing in the grid.

## Decision

**A game's tile shows the game's own picture in place of its title.**
The picture is the game's Steam header, the box art with the game's name
in the game's own lettering.
A child who cannot read can find Putt-Putt by Putt-Putt.
#138 asked for a picture beside the title; at a tile's size that cuts
the title off (see Alternatives considered).

**The picture is copied from the Guardian's own Steam, never downloaded.**
Provisioning copies each shared game's header, `header.jpg` or
`library_header.jpg`, from the Steam of the account named with
`--guardian`.
Nothing is fetched from the network.
No game art is shipped in the image or committed to this repository.
Tests make their own stand-in pictures at run time, or use the brand's
`brand/mark.svg`.

**The pictures live in a folder only root can change.**
They are `/var/lib/cairn/game-pictures/<appid>.jpg`, owned by root, mode
0644.
Provisioning makes the folder, or puts it right, owned by root, mode
0755, and refuses a link in its place.
Every account can read the pictures, and only root can change them.
The picture step, `cairn-game-pictures`, runs as root with `python3 -I
-B`.
It takes the Guardian's home from the password database, never from
`$HOME`.
From that home down, it opens every folder and the header without
following a link: `.local/share/Steam/appcache/librarycache`, the game's
folder, and the hash-named folder when the header is there instead.
In each folder it looks for `header.jpg` before `library_header.jpg`, and
it looks in the game's folder before a hash-named one.
It copies only a plain file of at most 1 MiB that starts as a JPEG does.
It writes under a temporary name and renames, so a launcher starting
meanwhile never reads half a picture.
A picture already in place stays when the Guardian's Steam no longer has
one, or has one that is left out, and the note says the tile keeps it.
It copies a picture only for a game that gets a tile, by kidscan's own
rules, and removes the picture of a game that no longer gets one.
The step prints only fixed words and app ids, and provisioning turns them
into its own sentences, so nothing read from a home reaches the
terminal.

**The games file names each picture.**
Provisioning runs kidscan with `--pictures /var/lib/cairn/game-pictures`.
A game whose `<appid>.jpg` is a plain file there gets
`"picture": "/var/lib/cairn/game-pictures/<appid>.jpg"` in its entry in
`/var/lib/cairn/games.json`.
kidscan never opens a picture.
A game without one has no `picture` key.
The file stays version 1, since a launcher without pictures ignores the
key.

**The launcher takes a picture only as the full path of a file on this
computer, and draws every title as plain text.**
A `picture` that is not text, is empty, is a relative path, is a URL,
names something inside the launcher's own resources, or is not a file
that exists, keeps the tile without a picture.
The journal then has one sentence for a grown-up.
The manifest takes the same key with the same rule.
A game's title comes from Steam, and Qt would read markup in it, such as
an `<img>` that loads from the network.
So every text the launcher draws is plain text, and a title shows as
written.

**The picture fills the card, whole, inside the card's own corners.**
The Paper card, its hairline edge and the focus ring are unchanged.
The picture sits inside an inset as wide as the tile's corner radius
(`Tokens.radiusTile`), so its square corners never cross the card's
rounded ones.
It is scaled to fit and centred, never cropped or stretched, and decoded
at the size it is drawn.
At 1366x768 that is about 373x174 px in a 409x223 tile.
The title is not drawn, because the picture carries the game's name.
The screen reader still reads the title.
A game with no picture, or a picture Qt cannot draw, shows its name as
before, so a tile is never blank.

**Only game tiles have pictures, for now.**
Draw, Practice and the other tiles keep their names.
Pictures for them are a later decision: the app's own icon, or a picture
a parent chooses, as #138 lists.
So is a first-party icon set, which belongs to brand v0.2 (ROADMAP §6).
The manifest takes a `picture` for any tile, but nothing writes one for
them yet.

**A picture of the game is not a storefront.**
DESIGN §2 says "No storefront in front of a child".
The tile shows the game the Guardian installed, and pressing it starts
that game, as before.
There is no price, no button, no rating, no other game suggested and no
way to the store.
The art was made to sell the game, and it has the store's busy,
full-colour look.
Cairn frames it in its own card and adds nothing of the store's around
it.

**The game's own art is the game's, not Cairn's.**
DESIGN §6.2 asks for a restrained palette, flat shapes and no mascots.
That is the brief for Cairn's own surfaces.
A game's picture is the game's content, like its window, and Cairn does
not reskin third-party apps (CLAUDE.md, non-negotiable 3).
Putt-Putt is the game's own character, whom the child sees the moment the
game starts, not a mascot Cairn uses to reward or coax.
Cairn's frame adds no colour, gradient or gloss.

**The games are now louder than making, and that is accepted.**
ADR-0024 made games quiet so that making stays the loudest thing in the
grid (DESIGN §2, principle 1).
A full-colour picture makes a game tile louder than an ochre make tile.
A quiet tile that a child cannot choose is worse: a child who cannot read
could not choose a game at all.
The next child test (P1-17) checks whether the pictures pull the
children away from Draw.

**The kind still shows without colour.**
ADR-0008 says colour is never the only carrier of a tile's kind.
A games tile is still a Paper card with a hairline edge, which only games
are.
In the Terminal a game is still filed under `games`.

**`cairn-game-pictures` copies, kidscan names, `TilePicture` checks,
`Tile.qml` draws.**
`TilePicture` is a small namespace with the picture rule, used for both
files.
`GamesFile` reads the games file with ADR-0033's rules, and `TileFiles`
reads both files for `TileModel` and the Steam-at-login check; those two
build ADR-0033's tile bullet.
`TileModel` hands each tile's picture to QML as a `picture` role.

## Consequences

- **A Guardian sees a line for each picture.**
  The set-up command prints `changed: the picture for <game>` when it
  copies a new or changed one.
  A game whose picture the Guardian's Steam does not have yet gets a
  note: "<game> has no picture yet, so its tile shows its name. Your own
  Steam keeps a game's picture once its library has shown the game: open
  Steam in your own login, then run this again."
  A picture that was left out gets a note with the reason.
  `docs/TRY.md` says to open Steam's library once and run the set-up
  command again.
  A child sees new tiles and pictures at their next login.
- **The trust boundary.**
  Root reads one file per game from a Guardian's home, so from the home
  down it follows no link, and it reads nothing large or endless.
  Python's standard library has no JPEG decoder, so the step checks only
  the first three bytes.
  Qt decodes the picture in the child's launcher; a picture it cannot
  draw shows the game's name.
  Only root can change the pictures folder, so no child can put a picture
  on another child's tiles.
  A Guardian can, through their own Steam, as they can already choose the
  games.
  The launcher reads only files on this computer named by their full
  path, and never a URL, and draws a title as plain text, so a title
  cannot make it fetch one.
- **A Guardian is an administrator, so these checks stop mistakes, not a
  Guardian.**
  Only Guardians can write the shared folder and their own Steam.
  A link in place of the shared `steamapps`, or anywhere from the
  Guardian's home down to a picture, is refused when it is read.
  A Guardian who swaps one in between a check and a read could still make
  root read a file elsewhere, which a Guardian can do anyway with `sudo`.
- **ADR-0033's kidscan run still reads no home.**
  It runs with an empty `HOME`, over the shared folder only (`--library`),
  and follows no `libraryfolders.vdf`.
  The picture step is the one exception to "no home's Steam is read".
  It reads one file per game, from the one Guardian named at
  provisioning.
  With more than one Guardian, the pictures come from whichever is named
  with `--guardian`.
- **Steam's `librarycache` layout is undocumented and may change.**
  It already has two layouts and two names for the header, and the
  picture step reads all of them.
  If it changes again, new games get no picture and a note, and their
  tiles show their names.
  Pictures already copied stay.
  The ROADMAP's risks table has a row for Steam's undocumented files.
- A game from a games file made before this change has no `picture`, and
  shows its name until the next provisioning run.
- A header is a few tens of kilobytes, so the pictures take little disk.
- Tested with stand-in pictures in C++, QML and Python, and by running the
  provisioning step against a made-up machine.
  Rendered at 1366x768 with real headers on the dev PC, outside the
  repository.
  Installed by hand on the kids' laptop on 2026-10-20, where 16 of the 25
  shared games got their picture (see Context).
  Run on the dev PC against a copy of that laptop's manifests and
  headers, the step that reads `library_header.jpg` too copies all 25.
- Checks before acceptance, on the kids' laptop:
  1. after a provisioning run, every shared game's tile shows its
     picture, or provisioning printed a note for it;
  2. a child's session can read `/var/lib/cairn/game-pictures/` and its
     pictures under SELinux, and the journal has no sentence about a
     picture;
  3. the tiles still show within the start time measured in
     `docs/research/launcher-footprint.md`;
  4. a game with no picture shows its name;
  5. P1-17 asks the younger child whether the picture tells her which
     game it is, and watches whether the pictures pull the children away
     from Draw.

## Alternatives considered

- **`logo.png`, the lettering alone.** A child who cannot read reads the
  game's lettering no better than Cairn's.
- **The portrait cover, `library_600x900.jpg`.** In a wide tile it is
  drawn at about a third of the tile's width.
- **The 32 px icon.** Too small to tell games apart at a tile's size, and
  named by a hash rather than by what it is.
- **Fetching the art from Steam's servers.** It phones home (CLAUDE.md,
  non-negotiable 1), and fails offline.
- **Reading each child's own Steam cache at login.** It is not reliably
  there: a child's Steam runs hidden and may never show its library.
  The launcher would also read Steam's files in the child's home.
- **The picture with a one-line title below it.** At this size the title
  is cut off, and the picture shrinks by about a third.
  It stays an option if P1-17 shows the name is missed.
- **A first-party icon set now.** It cannot draw every game a family
  owns, and Cairn's own icons belong to brand v0.2.
