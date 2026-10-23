# ADR-0040: The Guardian tool is its own app, "Family" in the menu

**Status:** proposed. It becomes accepted when a Guardian in the VM has
opened "Family" from the menu, added a child and moved a child to another
level with it, and an account that is not a Guardian has no "Family"
entry. The names `cairn-guardian` and "Family" stay proposals until then.
**Date:** 2026-10-23
**Closes:** (none) — the second slice of issue #167; part of ROADMAP P1-5

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

**The Guardian tool is a program of its own, `cairn-guardian` (a
proposal), shown as "Family" in a Guardian's application menu. Its first
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

One page so far, Children. The window holds the page in a scrolling area
of its own, so later pages, such as Games (#167), go beside it with a way
to move between them. Nothing is drawn for a page that does not exist.

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
  other answer is unknown.
- **Sentences:** each of ADR-0039's words, pkexec's two, an unknown
  answer and the window's own two checks is a `Reason` in C++, and the
  page's QML picks one `qsTr` sentence for each, as the Steam window does.
  They say what happened and what to do next, in the parent's voice:
  - Added: the child can log in now, at that level, and this window does
    not copy Steam games to them yet; the family set-up script does.
  - Level changed: the child is at that level now, and it starts the next
    time they log in. That is so: PAM and `cairn-session` read the groups
    once, at login.
  - A closed box or a wrong password: nothing changed, and it needs the
    password.
  - `failed`: the change did not finish, trying again is safe (ADR-0039:
    a second run finishes the first), and where the system log is.
  - An unknown answer: where the system log is, and that the list shows
    what is in place now.
  A refusal leaves the form filled in, so the parent can try again.

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
