# ADR-0047: A game's picture reaches every child who has it

**Status:** proposed. It becomes accepted when the checks under "Checks in
the VM" have run in the cairn-iso VM and none of them overturns a bullet
of the Decision.
**Date:** 2026-10-31
**Closes:** (none) — issue #205; part of ROADMAP P1-5 and P1-13. Once
accepted, amends ADR-0035, ADR-0041 and ADR-0042.

## Context

A child who cannot read finds a game by its picture (#138, ADR-0035).
A tile with only the game's name is the planned fallback, not a state to
leave unexplained.

On the image a tile took its picture from the `picture` key in the
child's own list, as saved (ADR-0041).
While checking pictures on the test VM on 2026-10-30, where a Save gave
every game its picture, #205 found five gaps:

1. The Save's window never said a picture was missing: `cairn-family`
   dropped the picture step's lines and said `pictures made` either way.
2. `pictures kept` was neither shown nor logged, and the sentence for a
   games list that could not be made sent the parent to an empty journal.
3. A card without a picture gave no hint.
4. A picture copied later reached only a child whose list a later Save
   changed.
5. The picture step removed the picture of every game kidscan left out:
   uninstalled, moved back or on a beta, and, as kidscan's tests assume
   though real Steam has not shown it, while Steam updates it (check 3).
   A child who still had the game kept a tile with only its name.

On 2026-10-30 Mason chose the issue's second way to fix 4 and 5, with the
parent told about 1 to 3.

## Decision

**A tile finds its game's picture by the game's app id, the pictures
folder keeps the picture of a game a child still has, and the Family app
names each game without a picture and says what helps.**

### A tile finds its picture by app id

- The launcher shows a Steam game's picture from
  `/var/lib/cairn/game-pictures/<appid>.jpg`, for the app id it already
  checks, when that is a plain file, not a link.
  `--pictures` names the folder: the image's wrapper passes it, and it
  is the default, which provisioning's wrapper relies on.
- The Games page reads each card's picture by the same rule.
- So a picture that any later Save or Look copies reaches every child who
  has the game at their next login, with no change to their list.

### A games file names no picture

- `cairn-family` and provisioning run kidscan without `--pictures`, so no
  games file carries a `picture` key, and an older entry's key is not read.
- A manifest keeps the key and ADR-0035's rule, and kidscan keeps the
  option for it.

### The folder keeps the picture of a game a child still has

- The picture step reads every child's list itself, as root.
  It keeps a picture whose game gets a tile from the shared folder by
  kidscan's rules, or is on a child's list as the step found it when it
  began, and removes any other.
- So the folder holds a picture only for a game in the family folder now,
  or one a parent gave a child.
  A game that is neither in the family folder nor on any child's list
  loses its picture at the next Save, Look or set-up run.
- It reads only `<login>.json` names, by cairn-family's rule for a login:
  each a plain file of at most 4 MiB, opened without following a link or
  waiting on a FIFO, holding the launcher's games file, version 1.
  Only an entry the launcher gives a tile counts.
  Only root can put a file there, so one it cannot read is a mistake: it
  keeps nothing, and its games show their names once they leave the
  shared folder.
- No folder means no lists, as on a computer set up by hand.
  A folder that is a link, not a folder, not root's, or one others could
  write in is `refused lists`, and nothing is touched.
- It only keeps: it copies a picture only for a shared game, so it reads
  the Guardian's Steam only for the family folder's games (ADR-0035).
- This changes ADR-0042's rule that the folder holds only the family
  folder's games.
  A kept picture is of a game a parent gave a child, so it tells no child
  about a game the parent never shared.

### What the parent sees

After a Save or a Look whose picture step went to its end, the Save's
window names each shared game without a picture by its title: "No picture
yet: Spy Fox. On a child's screen a game without a picture shows its name
instead. Your own Steam keeps a game's picture once its library has shown
the game: open Steam in your own login, then press Look for new games. If
only another grown-up's Steam has the game, they can do the same in their
own login."

A game whose picture the step left out is named apart, "Cairn could not
use your own Steam's picture for Spy Fox.", without the advice, since
opening Steam does not help it.
When both sentences come, only the first says what a child sees.
When the step did not go to its end, no game is named, since what helps is
then unknown; one sentence says the pictures could not all be copied, that
those copied before stay, and that pressing Look for new games tries
again.

The window's first paragraph points to the journal once, at its end,
when any of its sentences has its reason there: the pictures that could
not all be copied, a picture Cairn could not use, a games list kept from
before, or a tool left out (ADR-0046).
It ends with "The system log says why. In a terminal:
journalctl -t cairn-family", so no sentence follows the command; each
child's sentence is a paragraph of its own below it.

A card without a picture says so, with the same advice and the journal
for when it does not help; one no longer shared can get a picture only
once it is shared again, and says that.

### What cairn-family says

Two new lines of `save-games`, after `catalogue made` or `catalogue kept`,
only after `pictures made`, with fixed words and checked appids as ADR-0041
requires:

| Line | Meaning |
|---|---|
| `picture-missing APPID` | A game in the full list has no picture in the folder, by the launcher's rule, and the Guardian's Steam has none |
| `picture-refused APPID` | The same, but the Guardian's Steam has one, which the picture step left out |

`journalctl -t cairn-family` gets `pictures made` or `pictures kept`, then
each of the picture step's lines but `finished`, after `picture step: `.
A step that did not run or finish, and did not say why, adds `no-home` (no
Guardian's home, as under sudo), `no-answer` or `stopped`, as the tools
step does (ADR-0046).
Then `catalogue made` or `catalogue kept`, with `catalogue step: failed`,
`not-a-games-list` or `not-written` when it was kept.
The step's error output is dropped, so no path or name reaches the journal.

### The set-up script and where it applies

The set-up script's notes are true again on the image: "which has no tile
now, removed" is said only for a game no child has, and running it again
after opening Steam brings a missing picture to every child who has the
game.
The launcher's rule and kidscan without `--pictures` apply on both kinds
of computer; the kids' laptop's games file loses its keys at its next
provisioning run.
Kept pictures and the Family app's sentences are the image's only, as in
ADR-0041.

## Consequences

- The five gaps close, and ADR-0041's "a child not named is left alone"
  stands.
- Root also reads the lists folder, with the rules of its other reads.
  A list can only stop a deletion, never cause a read of a home.
- Limits:
  - a game no longer shared that never had a picture gets none until it is
    shared again, and a kept picture is not refreshed meanwhile;
  - an account removed outside Cairn leaves its list, whose games keep
    their pictures until a child with that login is made again, which
    removes it (ADR-0041);
  - pictures still come only from the Steam of the Guardian who presses
    Save or Look;
  - the window names each shared game without a picture after every Save
    and Look until it has one, which is every new game if Steam's
    `librarycache` changes (ROADMAP risk row).
- On a computer set up by hand, provisioning installs its launcher from
  `build/release` before it makes the games file, so that build must come
  from the same checkout: an older launcher shows no pictures from a games
  file that names none.
  It still starts, since provisioning's wrapper does not pass
  `--pictures`, which an older launcher would refuse.
- Tested in Python, C++ and QML, and in the VM on 2026-10-31 (checks 1,
  2, 4, 5, 6, 7 and 8; see "Run on 2026-10-31").

## Checks in the VM

In cairn-iso, with an image built from this change:

1. A shared game whose picture the Guardian's Steam lacks: a Save names
   it, its card says what helps, and the journal has `picture step:
   missing APPID`. After opening Steam and a Look, the card and the tile
   of a child who already had the game show the picture.
2. A listed game uninstalled from the family folder, or moved back, keeps
   its picture after a Look; once off every list, the next Look removes it.
3. The same while Steam updates a shared game, and on a beta branch.
4. `/var/lib/cairn/game-pictures` replaced by a link: the pictures-kept
   sentence, and `picture step: refused folder`; or `/var/lib/cairn/games`
   made writable by its group or others, then a Look: the pictures-kept
   sentence, and `picture step: refused lists`.
5. A shared game whose header in the Guardian's Steam is not a JPEG, with
   no older picture in place: a Look names it in the could-not-use
   sentence, the paragraph ends by pointing to the journal, and the
   journal has `picture step: refused APPID not-a-jpeg`.
6. A games list that cannot be made: `catalogue step:` and its word.
7. The set-up script run again after opening Steam copies the missing
   picture, and the child sees it.
8. No AVC denial, and no path or Steam name in the journal.

### Run on 2026-10-31

In cairn-iso, with this change's build laid over `/usr` instead of a new
image, since the VM cannot reboot without losing that. Looks ran as
`cairn-family save-games` under sudo with `PKEXEC_UID=1000`, except one
from the Family app itself.

1. Passed, in part. The four test games have no picture in the
   Guardian's Steam. Each gave the window `picture-missing APPID` and the
   journal `picture step: missing APPID`. In the Family app, a Look said
   "No picture yet: Test Game Big, Test Game One, Test Game Three, and
   Test Game Two.", then the rest of that paragraph. Each test game's card
   said "No picture yet, so a child's tile shows the name." with its
   advice. A picture put in place by hand as `900001.jpg` showed on ada's
   Test Game One tile, though her list names no picture for it. Not run:
   opening Steam until it shows a game, since the test games are not
   Steam's.
2. Passed, by moving the manifests of Arthur's Birthday (on ada's list)
   and Freddi Fish 2 (on no list) out of the family folder, then a Look.
   The games file lost both. `2471140.jpg` stayed, with no `removed`
   line, and ada's tile still showed it. `294530.jpg` went, with
   `removed 294530`. With both put back, a Look gave `copied 294530`.
3. Not run: no update or beta was to be had.
4. Passed. The link gave the window `pictures kept` and the journal
   `picture step: refused folder`. Mode 0775 gave `picture step: refused
   lists`, and the pictures folder did not change. Both put back.
5. Passed, without looking at the window. With Arthur's Birthday's header
   overwritten by text and its picture removed, the window had
   `picture-refused 2471140` and the journal `picture step: refused
   2471140 not-a-jpeg`. With the header back, a Look gave `copied
   2471140`.
6. Passed. With `steamapps` a link, the window had `pictures kept` and
   `catalogue kept`, and the journal `picture step: failed`, `catalogue
   kept` and `catalogue step: failed`. The games file did not change. Put
   back.
7. Passed, simulated as check 2 was: with `2471140.jpg` moved out of
   the pictures folder, the set-up script from this change's checkout,
   run as `--guardian guardian --child ada`, said "changed: the picture
   for Arthur's Birthday" and a "no picture yet" note for each test game,
   and ended "done: 2 change(s)". The picture was back, byte for byte the
   same, and ada's Arthur's Birthday tile showed it at her next login.
   The games file lost its `picture` keys, and each child's list was byte
   for byte the same. Not run: opening Steam until it shows a game.
8. Passed: none of the 64 journal lines holds a `/` or a game title, and
   there was no AVC denial.

The games file named no picture after the first Look, and each child's
list was byte for byte the same before and after. This stays proposed
until check 3 runs, unless the maintainer accepts it without it.

## Alternatives considered

- **`cairn-family` writes the picture into every child's list at each Save
  and Look,** the issue's first way: it breaks ADR-0041's rule that a
  child not named is left alone, the set-up script needs its own fix, and
  one fact lives in many files.
- **Copy a picture for a listed game no longer shared:** root would read
  the Guardian's Steam for games outside the family folder, for a case
  that mends itself once the game is shared again.
- **The step's own `missing` lines as the window's list:** a refused
  picture may leave an older one in place; the folder is what tiles read.
