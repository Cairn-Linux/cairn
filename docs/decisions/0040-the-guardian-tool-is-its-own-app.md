# ADR-0040: The Guardian tool is its own app, "Family" in the menu

**Status:** accepted 2026-10-23. In the cairn-iso VM, the Guardian
opened "Family" from the menu, moved a child to level 1 and added a child
with it, each after their password. The two children's accounts cannot
run the program, so `TryExec` keeps "Family" out of any menu of theirs.
The names `cairn-guardian` and "Family" are now the program's names.
**Date:** 2026-10-23
**Closes:** (none) — the second slice of issue #167; part of ROADMAP P1-5
**Amended by:** ADR-0041, proposed (a Games page beside Children; the
switch between them and a read-only Games page landed 2026-10-28, and
highlighting games and Save the same day); ADR-0043, proposed (a third page, Your account,
with the Guardian's own password and picture, and "Family" in System
Settings in place of Plasma's hidden Users page); the section "A name a
child on the list already has" below (2026-10-31, #212); the sentences
dated 2026-11-12 under "The Children page" and in that section's
consequences (the page reads the accounts again when it is shown, when
Family comes back to the front, and before each change); the sentences
dated 2026-11-12 under "Add a child" there (the form shows levels 3 and
4 as planned, and neither can be chosen); the section "The About page"
below (2026-11-12, P1-14: a fifth page with Fedora's notice)

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

Added by ADR-0041, step 3; highlighting games and Save came in its
step 4, and ADR-0041 says how they work.

- The parent picks a child, from the same children as the Children
  page, in the same order. The first is picked when the page opens.
- Below, a grid of the games in the shared folder, from
  `/var/lib/cairn/games.json`, each with its picture from
  `/var/lib/cairn/game-pictures` and its title in plain text, never rich
  text, as on the launcher's tile. The games on that child's own list,
  `/var/lib/cairn/games/<login>.json`, are highlighted and say so in
  words. Both files are read with the launcher's rules, and the image's
  launcher reads the child's own list, so a saved game is highlighted
  when the child's launcher gives it a tile. A game on the child's
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
- A tap on a game puts it on the child's list or takes it off until
  Save, which gives every changed child their new list with one
  password (ADR-0041).

### The Children page

- Every Cairn child, in the order their accounts were made, as on the
  login screen (ADR-0020), with their name and level. The list comes from
  the account database through NSS, read again after every change.
  Since 2026-11-12 it is also read each time the page is shown, each
  time Family comes back to the front, and before each change. A child
  is the same as `cairn-family`'s: a person's account in a level group,
  and no administrator. The rule lives in `accounts/` for both.
- **Add a child:** the name the login tile will show, and level 1 or 2,
  each with one sentence from DESIGN §3.
  Since 2026-11-12 the form also shows levels 3 and 4 under them, each
  with one sentence from DESIGN §3 and the word "Planned", and neither
  can be chosen (the maintainer's choice of 2026-11-09 for the first
  image that leaves the family; he suggested they might be blurred).
  The look was chosen when it was built and is his to settle: words on
  the page itself with no card or ring, in full colour, with nothing
  blurred or faded, because a faded or blurred sentence could not be
  read.
  No tap, key or screen reader's action picks one.
  Tab stops on each, though there is nothing to press, so that a person
  who moves with Tab and a screen reader hears that they exist; the
  outline that marks the keyboard's place shows round one only then.
  Each tells a screen reader it is text, "Level 3, planned", with its
  sentence, and offers nothing to check or press; this is tested through
  Qt's accessibility calls, and not yet with a real screen reader.
  The form still gives level 1 or 2 only, and `cairn-family` refuses any
  other as before.
  A child's row shows no planned level.
  The window makes the account's
  login name from the name, in C++: lower case, accents dropped, only a
  to z, digits, `-` and `_`, starting with a letter or `_`, at most 32.
  When the name leaves nothing, a second field asks for a short name. If
  any account or group already has the login, it adds 2, 3 and so on, so
  the Add form never names an existing child, whom `create-child` would
  change instead. Spaces at either end of the name are dropped. A name a
  child on the list already has is refused before the login name is made
  (the last section, #212). Then the window checks the whole request with
  `cairn-family`'s own rule (`FamilyRequest`), so a name the command
  would refuse is refused before the password is asked for.
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
  answer and each of the window's own checks is a `Reason` in C++, and the
  page's QML picks a `qsTr` sentence for each, as the Steam window does.
  A closed box and a wrong password share one; every other reason has its
  own. They say what happened and what to do next, in the parent's
  voice:
  - Added: the child can log in now, at that level, and starts with no
    games, which the parent chooses on the Games page (ADR-0041).
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
  name again, whatever the case of its letters (#212), gives
  `create-child` the same login while it is free or is still that child,
  so the second run finishes the first instead of making a second account
  with the same name. A `done` for that login forgets it. So does closing
  the window (the last section).
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
- A Guardian at the screen gives a child their games on the Games page
  (ADR-0041); the family set-up script no longer copies them.
- Still to decide, and to add here when they land: the "Family" link in
  System Settings and the Kiosk hide of Plasma's Users page, with the
  Guardian's own password and picture reachable from the tool first
  (#113); the Games page; the links to Plasma's own pages for Wi-Fi,
  printers and updates.
- Dependencies: Qt only. pkexec through `QProcess` adds no library.

## A name a child on the list already has (2026-10-31, #212)

In a user test on the cairn-iso VM on 2026-10-31, the family already had
a child called Cy, with the login `cy`. The parent added "Cy" again, at
level 1, and nothing warned them. The window gave the new account the
login `cy2`, as this ADR says, and the name "Cy". From then on every
screen showed two Cys that could not be told apart: two rows on
Children, two choices on Games, two "Steam for Cy" entries in the menu,
and two tiles on the login screen, told apart only by colour. At level 1
a tap on a tile logs straight in, so a child could land in the other
child's account. Family cannot rename or remove a child yet, so only a
terminal could undo it.

The numbered login keeps the form from changing an existing account.
Nothing kept apart the name, which every screen shows.

**Add a child refuses a name that a child on the list already has,
whatever the case of its letters, before the password is asked for.**

- The name is compared with each listed child's name as the login screen
  shows it: the full name, or the login name when there is none. The
  case of a letter does not count, nor do spaces at either end, so "cy"
  and " CY " are Cy's name. An accent counts: "Zoe" is not "Zoë".
- The page says "Cy already has an account on this computer. To add
  another child, give them a different name, such as one with an
  initial.", with the name as the parent typed it. Like every refusal,
  it leaves the form filled in, and focus goes back to the name.
- The check comes before the one for a missing short name. A name in
  another alphabet that a child already has is refused even with the
  short name left empty.
- Trying an add again still works. The same name, whatever the case of
  its letters, gets the remembered login, as "Trying an add again" says.
  The child the failed add made is on the list by then, but the retry is
  not refused for having that child's name.
- The login rule stays. A login that another account or a group already
  has still gets 2, 3 and so on, so the form never names an existing
  account. That now happens only for a name no child on the list has,
  when its login is taken all the same: by a child with another name,
  as when the parent types "Cy" beside a child called "Cy B" whose login
  is `cy`, by an account that is not a child, or by a group.

### Consequences

- The form can no longer make two children with one name, as long as no
  child is made outside Family while it asks for the password. A family
  with two children of one name gives one of them an initial, as the
  sentence says.
- The window compares the name with the children as it reads them just
  before it asks for the password. Until 2026-11-12 it compared them as
  it last read them, when it opened and after each change, so a child
  removed in a terminal kept their row and their name until the next
  change was answered or Family was closed. A child made elsewhere while
  the password is asked for is still not compared. A retry is not
  compared at all, so a child of that name made elsewhere between a
  failed add and its retry does not stop it.
- Only children are compared. A child may still get a grown-up's name;
  a grown-up's tile asks for a password, so a child cannot land in that
  account with a tap.
- A window closed after a failed add forgets it, as before. Opened
  again, it lists the child the add made like any other and refuses
  their name, so no second account is made. That child may still lack
  the steps after `useradd`: the lock on the password and their "Steam
  for <child>" entry. Moving them to another level runs both, as
  `set-level` does for every child. Remembering a failed add from one
  window to the next would need a file the window keeps, which is not
  done.
- A family that already has two children of one name, such as the VM's
  `cy` and `cy2`, still needs a terminal to rename or remove one.
- Tested in C++, in `tst_childlist` and `tst_childrenpage`, and in the
  real window, in `tst_familywindow`. Checked in the cairn-iso VM on
  2026-10-31, beside its two children named Cy, `cy` and `cy2`. "Cy"
  and " cy " were refused before any password box, and "Cy B" was added
  after the password, with the login `cyb`.

## The About page (2026-11-12, P1-14)

Fedora's trademark guidelines ask for a prominently displayed notice;
ADR-0038 reads that as where the system names itself. ADR-0038 put it on
this tool's About page, because KDE's own About page can hold no link
and no translation of Cairn's own, and P1-14 blocks publishing on it.
Until now Family had no such page.

**Family has a fifth page, About. It says what Cairn Linux is, which
version the computer runs when the image says, what Cairn Linux is built
on, with Fedora's notice in full, and where the licences are. It reads
one file and one folder on the computer, and starts nothing until a
parent presses a button.**

### What the page shows

In this order, under three headings:

- **About Cairn Linux.** Two sentences on what it is: the system on this
  computer, made for children, with a grown-up in charge. Then the
  version.
- **What it is built on.** The lineage sentence, with ® after the first
  "Fedora", and the not-affiliated sentence, both as the README has
  them. Fedora's notice, with the address https://fedoraproject.org/
  written out. A button, "Open fedoraproject.org in your web browser".
  "This copy is derived from Fedora Linux 45.", with the number the
  image gives. Fedora's attribution statement.
- **Licences.** The licence sentences, after the README's. The folder
  that holds the full texts, `/usr/share/licenses/cairn`, and what each
  of its four files is. A button, "Open the folder with the full texts".
  Two sentences that the rest of the system is other projects' software,
  with its packages' licences in `/usr/share/licenses`.

Every block is plain text, never rich text. The version, Fedora's
release, the address and the folder's path are put into their sentences
by the program. So nothing read from a file is ever markup, and a
translation cannot change the notice's address or the folder's path.
The web button's label and the last sentence name `fedoraproject.org`
and `/usr/share/licenses` in their own words, with a note that asks the
translator to keep each as it is.

Not on the page: any other address, a support address, the maintainer's
name, the Linux trademark sentences, Bazzite's build number and a logo.
`NOTICE` and `TRADEMARKS.md`, which the page points to, carry the marks.

### The version

The version is `IMAGE_VERSION` from `/usr/lib/os-release`, and nothing
else. The page shows "Version: " and the value only when the file's `ID`
is `cairn` and the value is 1 to 64 letters, digits, dots, dashes and
underscores. Otherwise it says "This copy of Cairn Linux does not say
which version it is."

No image writes that key today. `build_files/name-and-logos.sh` writes
fourteen keys and none is Cairn's own version, which exists only as a
label on the image (ADR-0038, "Labels"). So every computer shows the
second sentence until the build writes the key. That is a change to the
image, of its own, and the maintainer has not said yes to it. It is not
made here.

Fedora's number is never shown as Cairn's version. `VERSION_ID` is the
Fedora release of the base, 45 today. "Cairn Linux 45" would read as
Cairn's 45th version, and would change by itself when the base moves to
Fedora 46. The number appears once, lower down, named as Fedora's, and
only when `ID` is `cairn` and the value is 1 to 4 digits. On a computer
that does not run Cairn's image, such as the dev PC, the page says it
does not say which version it is, and leaves the Fedora line out.

### Fedora's address, and the two buttons

The address is written out inside the notice, so the notice is whole on
a computer with no web browser and no network. The guidelines also say
"linking to", and a page of plain text has no link. So one button under
the notice asks the desktop to open https://fedoraproject.org/ in the
parent's own browser. It is the only thing in Family that starts a web
browser. The second button asks the desktop to open the licence folder
in the parent's file manager. It shows only when the folder holds all
four files as plain files; otherwise the page says "The full texts could
not be found on this computer."

This is not phoning home (DESIGN §2, principle 6). Family makes no
network call itself. Nothing is looked up, fetched or checked when
Family starts or when the page is shown, not even whether a browser is
installed. The address is fixed in the program, and the page can ask
for no other. A browser starts only when the parent presses the button,
as with `HOME_URL` in `os-release`: "A link a person clicks; nothing
fetches it" (ADR-0038).

When Qt answers that it could start nothing, one sentence appears under
that button and the window scrolls to it: "No web browser opened. The
address above works in any browser, on this computer or on a phone.", or
"The folder did not open. Its path is written above."

Qt's answer does not always say what happened. Under Wayland, which the
image uses, Qt 6.11 answers yes at once, whatever happens next. On the
dev PC on 2026-11-12 it did so with no launcher to find, and with a
launcher that started and failed. Under X11 it answered no with no
launcher to find, and yes with the one that failed. None of this was
tried on an image. So on the image neither sentence is expected to show,
and a press that opens nothing leaves the page as it was.

The maintainer has not yet said yes to the web button. It is one block
in `guardian/qml/AboutView.qml`, and the notice reads the same without
it.

### The fifth button

The window opens at the size it had, and its smallest size is the same.
A wider window would give the Games page a fourth column of cards.
Measured from the brand font's file, the five buttons need about 645 px
of the 672 px row in the window as it opens, so they are on one line
there. In a narrower window, the smallest one or one tiled to half of a
1366 px screen, About goes on to a second line. The bar above the page
is then 64.5 px taller, and every page has that much less room.

In the smallest window that costs one thing the tests could measure. A
game card needs 191 px to show with the full room the window keeps
above and below it, and the page there is 187.5 px tall. The keyboard
still brings the whole card and its ring into sight, with the full room
above it and a little less below.

About is a page tab like the other four. Tab and Shift+Tab pass through
it, and Left and Right move between it and Notices.

On the page, Tab stops on each block of words and each button in turn,
and the window scrolls to show it. So the page can be read with the
keyboard alone, and each block has a name and a role for a screen reader
(not yet tried with Orca). Up, Down, Page Up and Page Down scroll the
page from any of them.

### Consequences

- The About-screen part of P1-14 is built. The website still needs the
  same sentences, so the two boxes in
  `docs/research/base-image-policy.md` stay open.
- The notice now lives in three places: `README.md`, `NOTICE` and
  `guardian/qml/AboutView.qml`. Nothing compares them, because the build
  stage that runs the tests is given neither of the first two. The
  window test holds the page's English sentences, and the three change
  together.
- The notice is a translatable string like every other (ADR-0007). A
  note to the translator asks for its meaning and every name in it to be
  kept. Whether the English stays on screen beside a translation is a
  question for the first translation.
- The page's two notice sentences and the attribution rest on this
  repository's quote of Fedora's guidelines
  (`docs/research/base-image-policy.md`). They were compared with
  Fedora's own source again on 2026-11-12. The guidelines' file was last
  changed at `fedora-legal-docs` a7507b7, the revision the quote was
  taken from, and the sentences match.
- Nothing in the repository shows that a web browser is on the image,
  and under Wayland Qt answers that an address opened whatever happened.
  So the button may do nothing useful, and on the image its sentence is
  not expected to show. The written-out address is the part that always
  works.
- The two buttons are the first place Cairn's own code asks the desktop
  to open an address or a folder. The Guardian audit (#86) should have
  them on its list.
- After Page Down, the ring around the block that has the keyboard can
  be out of sight until the next Tab. That Tab goes to the block after
  the one that had the keyboard, and the window scrolls back up to it.
- Dependencies: Qt only. `QDesktopServices` is part of Qt GUI, which
  Family already uses.
- Tested in C++, in `tst_osrelease` and `tst_aboutpage`, and in the real
  window, in `tst_familywindow`. Not yet seen on the VM or on an image.
  Still to check by hand there, as a Guardian: the five buttons in the
  brand font, on one line as the window opens and with About on a
  second line in a window tiled to half of a 1366 x 768 screen; the page
  read with the keyboard alone, and by Orca; whether "What it is built
  on" starts in the first screenful on a 768 px tall screen; each button
  with the network on and off, and whether the image has a web browser
  at all; `NOTICE` opened from the folder; and, as a child, that Family
  still cannot be started.
