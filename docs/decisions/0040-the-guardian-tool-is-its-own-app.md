# ADR-0040: The Guardian tool is its own app, "Family" in the menu

**Status:** accepted 2026-10-23. In the cairn-iso VM, the Guardian
opened "Family" from the menu, moved a child to level 1 and added a child
with it, each after their password. The two children's accounts cannot
run the program, so `TryExec` keeps "Family" out of any menu of theirs.
The names `cairn-guardian` and "Family" are now the program's names.
**Date:** 2026-10-23
**Closes:** (none) — the second slice of issue #167; part of ROADMAP P1-5
**Amended by:** ADR-0041, proposed (a Games page beside Children; the
switch between them and a read-only Games page landed 2026-10-28)

## Context

DESIGN §3.2 gives Guardians full administration from their own login,
behind their own password: accounts, levels, permissions, time, system and
games. It does not say what shape the tool takes. Issue #167 asks for
Cairn's own app for parents, not pages added to System Settings, because
parents may not be technical. It lists an ADR for that as its last step,
with two pieces that are not built yet: a "Family" link in System Settings,
and Plasma's Users page hidden for children's accounts (#113).

ADR-0039 made the command under it, `cairn-family`, and left the window
on top of it for this slice. ADR-0032's window for a child's Steam is
already a Guardian-only program: `root:cairn-guardian` 0750, with a menu
entry whose `TryExec` names it, so Plasma's menu shows it only to
accounts that may run it.

## Decision

**The Guardian tool is a program of its own, `cairn-guardian`, shown
as "Family" in a Guardian's application menu. Its first
page is Children. It runs `cairn-family` through pkexec, one change at a
time, and turns every answer into one plain sentence.**

### The program

It lives in `guardian/`, in C++20 and Qt Quick, with its own QML module,
`Cairn.Family`, so the Steam window keeps its module as it is. The two
share one button, `qml/TextButton.qml`. It is installed as the image
installs the Steam window: the program at
`/usr/libexec/cairn/cairn-guardian` and a wrapper at
`/usr/bin/cairn-guardian`, both `root:cairn-guardian` 0750, set by
`build_files/build.sh`. The wrapper runs it under `systemd-cat`, so its
notes go to the journal as `cairn-guardian` and it stays the process
that started pkexec.

Its menu entry, `/usr/share/applications/cairn-guardian.desktop`, is
named "Family", with the theme's `system-users` icon, under Settings and
System. Its `TryExec` names the wrapper, so only a Guardian's menu shows
it. The entry is plain English for now; a desktop file carries its own
translations, which wait for the first translation (ADR-0007).

Hand provisioning on a stock Bazzite installs neither the program nor
`cairn-family` (ADR-0039), so the tool is on the image only.

### Pages

Two pages, Children and Games (ADR-0041). A switch above the page, two
buttons that screen readers hear as page tabs, moves between them, and
stays in place while the page scrolls. The window opens on Children.
Each page has the same scrolling area, which starts at the page's top
when the page is shown, and keeps keyboard focus in sight. Nothing is
drawn for a page that does not exist.

### The Games page

Added by ADR-0041, step 3. It only shows each child's list; choosing
games by highlighting them, and Save, come in its step 4.

- The parent picks a child, from the same children as the Children
  page, in the same order. The first is picked when the page opens.
- Below, a grid of the games in the shared folder, from
  `/var/lib/cairn/games.json`, each with its picture from
  `/var/lib/cairn/game-pictures` and its title in plain text, never rich
  text, as on the launcher's tile. The games on that child's own list,
  `/var/lib/cairn/games/<login>.json`, are highlighted and say so in
  words. Both files are read with the launcher's rules. Once step 4 has
  the launcher read the child's own list, a game is highlighted when the
  child's launcher gives it a tile; until then every child's launcher
  still reads the full list, and the page says so. A game on the child's
  list that the shared folder no longer has still shows, last, and says
  so: the child's own copy may still play it. A child with no file has
  no games; a file that cannot be read counts as none, and the page says
  so and what will write it again.
- The page reads the children and both files again each time it is
  shown and after each change on the Children page.
- **Open Steam for <child>** starts `cairn-steam-for <login>` as a
  program of its own, the window the "Steam for <child>" menu entries
  open (ADR-0032). The menu entries stay. When it cannot be started, the
  page says so, and the reason is in the journal under `cairn-guardian`;
  a window that started and then stopped writes under `cairn-steam-for`.
- With no children, it says to add one on the Children page. With no
  games, it says how to install one into the shared folder from the
  Guardian's own Steam.
- It says plainly that, for now, it only shows each child's list.

### The Children page

- Every Cairn child, in the order their accounts were made, as on the
  login screen (ADR-0020), with their name and level. The list comes from
  the account database through NSS, read again after every change. A
  child is the same as `cairn-family`'s: a person's account in a level
  group, and no administrator. The rule lives in `accounts/` for both.
- **Add a child:** the name the login tile will show, and level 1 or 2,
  each with one sentence from DESIGN §3. The window makes the account's
  login name from the name, in C++: lower case, accents dropped, only a
  to z, digits, `-` and `_`, starting with a letter or `_`, at most 32.
  When the name leaves nothing, a second field asks for a short name. If
  any account or group already has the login, it adds 2, 3 and so on, so
  the Add form never names an existing child, whom `create-child` would
  change instead. Spaces at either end of the name are dropped. Then the
  window checks the whole request with `cairn-family`'s own rule
  (`FamilyRequest`), so a name the command would refuse is refused before
  the password is asked for.
- **Move to level N:** one button per level the page can give, the same
  words up or down, with no warning and no second question (DESIGN §3).
  A child at level 3 or 4 shows that level and is offered 1 and 2.
- **Running the command:** `pkexec`, started directly by `QProcess`,
  never through a shell, with the absolute path of `cairn-family` and
  `create-child LOGIN LEVEL --full-name NAME` or `set-level LOGIN LEVEL`.
  Every control waits while it runs; a second change cannot start. The
  answer is `done` with exit 0, `refused WORD` with exit 1, or pkexec's
  126 (box dismissed) or 127 (not authorized, and Plasma's Cancel). Any
  other answer is unknown. pkexec also gives 127, without a password
  box, when the command is not there, so the window checks that the
  command can be run first and calls a missing one unknown, never a
  wrong password.
- **Sentences:** each of ADR-0039's words, pkexec's two, an unknown
  answer and the window's own two checks is a `Reason` in C++, and the
  page's QML picks a `qsTr` sentence for each, as the Steam window does.
  A closed box and a wrong password share one; every other reason has its
  own. They say what happened and what to do next, in the parent's
  voice:
  - Added: the child can log in now, at that level, and the Games page
    shows their list. Until ADR-0041's step 4, the family set-up script
    is what gives them their games, and the sentence says so.
  - Level changed: the child is at that level now, and it starts the next
    time they log in. That is so: PAM and `cairn-session` read the groups
    once, at login.
  - A closed box or a wrong password: nothing changed, and it needs the
    password.
  - `failed`: the change did not finish, trying again is safe (ADR-0039:
    a second run finishes the first), and where the system log is.
  - `not-root` and `usage`, which the window should never meet: nothing
    changed, the likely cause, and where the system log is.
  - An unknown answer: where the system log is, and that the list shows
    what is in place now.
  A refusal leaves the form filled in, so the parent can try again.
- **Trying an add again:** a `create-child` that fails, or whose answer
  cannot be read, may have made the account before it stopped, so the
  child is on the list. The window remembers that login. Adding the same
  name again gives `create-child` the same login while it is free or is
  still that child, so the second run finishes the first instead of
  making a second account with the same name. A `done` for that login
  forgets it.
- **Keyboard and scrolling:** after each change, focus goes back to where
  the parent was: the child's row, or the name field (the short name
  field when that is what is missing). The window scrolls so the outcome
  sentence, and whatever has keyboard focus, is in sight, and shows a
  thin bar when the page is longer than the window. A child's buttons go
  under their name when the row is too narrow for both.

## Consequences

- A parent can add a child and change a level without a terminal, with
  their password for each change, as ADR-0039 asks.
- The window, the family's setup and the tests run the same command and
  the same account rule.
- Adding two children asks for the password twice. A Guardian who is in
  `wheel` but not `cairn-guardian` may run `cairn-family` but not this
  window, as with the Steam window.
- A Guardian at the screen must still copy games to a new child with the
  family set-up script (ADR-0033) until the window can.
- Still to decide, and to add here when they land: the "Family" link in
  System Settings and the Kiosk hide of Plasma's Users page, with the
  Guardian's own password and picture reachable from the tool first
  (#113); the Games page; the links to Plasma's own pages for Wi-Fi,
  printers and updates; the About page with Fedora's notice (P1-14).
- Dependencies: Qt only. pkexec through `QProcess` adds no library.
