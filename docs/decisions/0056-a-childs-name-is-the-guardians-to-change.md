# ADR-0056: A child's name is the Guardian's to change

**Status:** accepted. The maintainer decided on 2026-11-11 that it is
accepted when this change merges (#296): the checks on two built images
passed (Checked, below), and what is under "Not checked yet" stays
listed without holding the label.
**Date:** 2026-11-07
**Closes:** issue #292. Amends ADR-0043 (polkit's answer for an
account's own data). Adds a subcommand to ADR-0039's `cairn-family` and
a button to ADR-0040's Children page.

## Context

Mason decided on 2026-11-06 that children may not change their own full
name: a Guardian sets it (#292). On 2026-11-07 he widened it to a child's
picture and language too, which the system keeps under the same
permission.

A child's full name is the name on their login tile (ADR-0020), on
Family's pages and in Family's Notices (ADR-0054). ADR-0054 already
shows a login name beside every full name, because a child who named
themselves "Mum" would otherwise read as a parent there.

Ways an account can change its own full name on the image, as read on
2026-11-07:

- **AccountsService**, over the system bus, with `SetRealName` on the
  account's own object. It asks polkit for
  `org.freedesktop.accounts.change-own-user-data` when the caller is
  the account itself. Fedora's policy says `yes` to it for every
  account, at the machine or not (`allow_any`, `allow_inactive` and
  `allow_active` are all `yes`). ADR-0043 relied on that for a Guardian's
  own picture, and called it `yes` "for an account at the machine"; it
  is `yes` from anywhere.
- **chfn**, util-linux's, set-uid root, which changes the full name in
  `/etc/passwd`.
- `usermod` and `useradd` change it too, but only for root.
- Plasma's Users page goes through AccountsService, and the image hides
  it (ADR-0043).
- Fedora's `passwd` has no option for the full name. libuser's `lchfn` is
  not on the image, and is not set-uid on Fedora 44.

What `change-own-user-data` covers, read from AccountsService 23.13.9's
source (Fedora 44, `src/user.c`), when the caller changes their own
account: the full name, email, language and languages, session, session
type and X session, location, picture, password hint, the password's
expiry policy and the account's expiry date, and any property of an
extension interface that names no permission of its own. A child could
make their own account expire. The image's AccountsService, 26.27.3 on
Fedora 45, names only three actions in its program: this one,
`change-own-password` and `user-administration`. Changing one's own
password asks `change-own-password`, `auth_admin` on Fedora. Changing
another account, its type, lock, shell, home or login name asks
`user-administration`, `auth_admin_keep`.

polkit sees the action, not the method, so a rule cannot refuse the name
and allow the language.

chfn reads `CHFN_RESTRICT` from `/etc/login.defs` to learn which fields
an account other than root may change. util-linux 2.41.6's `chfn.c`
reads it before it asks PAM for anything; with no value it stops with
"CHFN_RESTRICT does not allow any changes". Fedora's `login.defs` has
the line commented out. So chfn already refuses every account but root,
before any password, and no wrong try is counted.

## Decision

**A child, at any level, cannot change their own name, nor their
picture, language or anything else through AccountsService: Cairn's
levels rule refuses `change-own-user-data`, and AccountsService's other
two actions, to every account in a level group that is not an
administrator. chfn already refuses every account but root, and a build
check keeps it so. A Guardian changes a child's name on Family's
Children page, with Change name, through `cairn-family set-name` behind
the Guardian's password.**

### The rule

`session/polkit/rules.d/10-cairn-levels.rules` adds
`org.freedesktop.accounts.change-own-user-data` to the Guardian's list,
the actions it refuses at every level, L3 and L4 included (DESIGN §3.2).
It adds `change-own-password` and `user-administration` too. Fedora
asks for a grown-up's password for those, so a child calling
`SetRealName` on a sibling's or a Guardian's account would get a
password box in their own session, and `auth_admin_keep` keeps a
password typed there for a few minutes. DESIGN §3.2 says Cairn never
asks for a Guardian's password in a child's session. No Cairn screen in
a child's session uses either action; a Guardian's own password goes
through `cairn-family set-own-password` (ADR-0043), and `passwd` in a
terminal goes through PAM, not polkit.

- An administrator, in `cairn-guardian` or `wheel`, is let through
  first, as for every action (ADR-0023), so a Guardian's own picture on
  Your account works as before (ADR-0043).
- An account Cairn did not make keeps Fedora's default.
- The rule answers before polkit asks for any password, and only ever
  says no.
- The image installs the rule in `/usr/share/polkit-1/rules.d`, and
  provisioning in `/etc/polkit-1/rules.d`, so a computer set up by hand
  gets it at its next provisioning run.

What a child loses with it: nothing on their screen today. The kiosk has
no settings, and Cairn's login screen draws a letter, not a picture
(ADR-0020). At L3 and L4, a Plasma page that sets the account's language
or picture through AccountsService would be refused; whether any page
of Plasma 6.8 does so outside the hidden Users page was not checked.

### chfn and the rest

Nothing changes. `build_files/check-own-account-data.sh` runs in the
image build after `build.sh` and stops it when:

- AccountsService's policy no longer has the action, or its program no
  longer names it, or Cairn's installed rule does not name it: the rule
  would then refuse nothing.
- `/usr/bin/chfn` is no longer util-linux's.
- `/etc/login.defs`, `/usr/etc/login.defs`, or a file in a
  `login.defs.d` beside either, sets `CHFN_RESTRICT`. util-linux on
  Fedora reads only `/etc/login.defs` today; the others are checked in
  case it is built with libeconf, which reads them too.
- `/usr/bin/lchfn` exists and is set-uid.

### Change name in Family

`cairn-family set-name LOGIN NAME`:

- Refuses an account Cairn did not make with `not-a-child`, at any
  level, 1 to 4. A name is not a level.
- Checks the name with create-child's rule, `bad-full-name`: not empty,
  no spaces at an end, no control character, colon, comma or backslash.
- Changes only the full name, with `usermod --comment`, and only when it
  differs. Then it writes the child's "Steam for" menu entry with the new
  name, as create-child does. The level and the password stay.
- Holds the family lock, as every change to an account does (#255).
- Tells the journal `set-name LOGIN` and the answer, not the name.

Its polkit action, `com.cairnlinux.family.set-name`, asks for an
administrator's password every time, `auth_admin`, with the message the
other changes to an account have: "A grown-up's password is needed to
change a child's account."

On the Children page, each child's row has **Change name** beside the
level buttons. It opens a field under the row with the child's name in
it, and **Save name** and **Cancel**. Escape cancels; Enter saves. The
window refuses, before any password:

- an empty name: "Type the child's name first.";
- the full name the child has, exactly: "Ada is the name already. Type
  a different one, or press Cancel.";
- a name another child on the list has, whatever the case of its
  letters, as Add a child does (#212): "Another child is called Ben
  already. Two children with one name look the same on every screen, so
  choose a different one, such as one with an initial." The sentence
  names the other child as the list shows them, not as typed;
- a grown-up's name, the name the login screen shows for an
  administrator's account, whatever the case of its letters: "Mum is
  the name of a grown-up on this computer. A child with a grown-up's
  name looks like them on every screen, so choose a different one." Add
  a child refuses it too, with the same sentence;
- a name the command's rule refuses, with Add a child's sentence.

The same name in other capitals is a change. So is a child's login
given as a name, for a child whose account has no full name and so is
listed by their login: the account gains a full name. The window checks
against the children and grown-ups as it last read them, and
`set-name` checks for no clash of its own, so a name another window
gave meanwhile goes through, as with Add a child. A level chosen while
the field is open closes it. Once it is in place:
"Ada is called Ada Lovelace now, on the login screen and in Family."
The field closes and the keyboard goes back to Change name. Every other
answer has the sentence it has for a level, and a refused name stays in
the field, with the keyboard there, to mend or try again.

The way not taken: running `create-child` with the child's level and
`--full-name`. It already changes an existing child's name, but it takes
only levels 1 and 2, so an older child would be refused, and it makes
the account when the child was removed in the meantime.

## Consequences

- A child cannot become "Mum" on the login screen or in Family, from
  any program. Family does not give a child a grown-up's name either.
- A child cannot change their picture or language through
  AccountsService. **Files in the child's own home are not covered.**
  Plasma also takes an account's picture from `~/.face.icon` or
  `~/.face`, which shows on Plasma's lock screen at L3 and L4 and on any
  login screen theme that shows faces; Cairn's draws a letter
  (ADR-0020). Plasma takes the language from `~/.config/plasma-localerc`
  and `~/.config/environment.d/`. An L3 or L4 child with a file manager
  or a terminal can change those. Only the full name is locked.
  The maintainer decided on 2026-11-09 that a child may change their
  own picture, so `~/.face.icon` stays open (AccountsService still
  refuses it, as one action covers the name too), but that a Guardian picks
  a child's language: a child who changes it by accident may not be
  able to read the screens that change it back. Closing the language
  routes, and a language setting in Family, are #299 (ADR-0058).
- **A name, picture or language a child set before this image stays
  until a Guardian changes it.** Family changes the name, and since #300
  the language (ADR-0058). Family cannot change a child's picture yet;
  Cairn's own screens do not show a child's picture.
- ADR-0054's login name beside every full name stays: names set before
  stay, and a Guardian may give any name.
- AccountsService's other changes to one's own account, such as the
  expiry date, are refused to children too, which is wanted.
- Family can rename a child at level 3 or 4, though it cannot yet give
  those levels.
- If AccountsService gives one of these changes an action of its own,
  the rule no longer covers it. The build check catches a renamed action,
  not a new one; the check below should be run again after an
  AccountsService upgrade.

## Checked

In cairn-min on 2026-11-07, on the image dev.20261106-57bd89a (Fedora
45, polkit 127, AccountsService 26.27.3, util-linux 2.42.4). `/usr` was
made writable until the next boot with `bootc usr-overlay`, this
change's rule put in place of the image's, and polkitd restarted. The
image's rule was put back afterwards and the VM shut down.

1. `pkcheck` for the action, from a process of each account: ada (L1)
   and ben (L2) "Not authorized"; the Guardian authorized.
2. `SetRealName` and `SetLanguage`, each to the value the account had,
   and `SetIconFile` with one of KDE's pictures, called over the bus by
   each account on itself: refused with "Not authorized" for ada and ben,
   done for the Guardian. With the image's rule, `SetRealName` and
   `SetLanguage` by ada and ben went through. The Guardian's new picture
   was taken off again afterwards.
3. `chfn -f Mum`, run as ada: "chfn: /etc/login.defs: CHFN_RESTRICT does
   not allow any changes", exit 1, the account database unchanged and no
   wrong try counted.

What the run taught: polkitd loads every rules file into one JavaScript
context. A copy of the new rule under another name in
`/etc/polkit-1/rules.d` changed nothing, because the image's file, loaded
after it, gave the shared names, such as the Guardian's list, back their
old values. A change to the rule has to be tried in place of the image's
file, not beside it.

### Not checked yet

This ADR first listed four things here: that no earlier rule file
answers `yes` first, the other two actions refused with no password box,
an image built from this change, and Change name through the real
password box. Each was checked on built images, the rule for an L1 and
an L3 child and Change name for one child. The re-check of 2026-11-09
left three more: an L2 child's session on a built image, a child acting
inside their session by hand, and a name put back through Family. The
run of 2026-11-11 tried each. The sections below say what was seen and
how. Still open after it:

- The same refusals when a child acts inside their session by hand. In
  the runs on a built image up to 2026-11-11 each probe was started
  from a root shell, moved into the cgroup of the child's session and
  dropped to the child's user. On 2026-11-11 two lines were typed with
  keys into an L2 child's Terminal, and the Terminal answered both
  itself. Up to then no command that reaches polkit, AccountsService or
  `cairn-family` had been typed inside a child's session, at any level.
  On 2026-11-12 lines were typed with keys in a terminal window in an
  L3 child's session, in one login: `busctl` calls to AccountsService,
  `chfn`, `usermod`, and `cairn-family set-name` with `pkexec` and
  without it. Each was typed once, and two of them a second time. Each
  was refused, and no password box is in the screenshots taken after
  them. That run was on a test image built from main with this change
  and five others merged in, and no child was at the computer
  ("Checked on 2026-11-12", below). Still not tried by hand: a child at
  L4. At L1 and L2 the Terminal answered the typed lines itself: two
  lines at L2 on 2026-11-11, and lines about the updater (#304) at L1
  and L2 on 2026-11-12.

The maintainer decided on 2026-11-11 that this stays listed and does not
hold the label (Status).

### On an image built from this change

In cairn-min on 2026-11-07, on `test.20261107-276-292-2c4643a` (main
57bd89a with this change at 328f9df and #295 merged; 105 of 105 tests
and `check-own-account-data.sh` passed in the build):
- ada (L1) and ben (L2, moved to L3 with a password for the run, then
  put back) called `SetRealName`, `SetIconFile`, `SetLanguage` and
  `SetPasswordHint` on themselves, and a rename of the Guardian: each
  "Not authorized". `pkcheck` for `change-own-user-data`,
  `change-own-password` and `user-administration` answered no, not a
  password box, so no earlier rule file answered first. `chfn` was
  refused by CHFN_RESTRICT. No wrong try was counted.
- The Guardian renamed ben in Family through the real password box: the
  account, the login screen and his "Steam for" entry took the new name,
  and the journal said `set-name ben: done`. A grown-up's name was
  refused before any password box. Add a child still worked, and the
  Guardian still changed their own name and picture.
- The build first failed: the install-tree test still counted five
  `cairn-family` actions, where this change makes six. Fixed here.
- Found there and fixed here: Escape after a refused name closed the
  field but left the refusal on the page.

Not tried: whether Plasma's region page goes through AccountsService
(the PR's question 2), the `~/.face.icon` and `plasma-localerc` routes
this ADR names as known limits, and that an account Cairn did not make
keeps Fedora's answer.

### Checked again on 2026-11-09

The re-check ran in cairn-min on 2026-11-09, on
`test.20261109-295-296-300-4a5f38d` (main 57bd89a with this change at
73e519e, #295 at b254364 and #300 at a782c4a; 106 of 106 tests and
`check-own-account-data.sh` passed in the build). The rule in the image
differs from this change's in one comment, which #300 rewords. Family in
the image is #300's build too: #300 adds Change language to each row and
moves this change's name refusals into a class of their own. So the
Family results below are for this change as #300 carries it. The build
has no node, so the rule's tests skipped the three cases that run the
rule itself. They passed on the dev PC from the same tree. The run's
record was audited against its evidence afterwards, and what follows
keeps to what the audit found shown.

How the probes were made: ada (L1) and ben (L2, moved to L3 with a
password for the run, then put back) each logged in at the login screen.
Each probe was then started from a root shell, moved into the cgroup of
the child's session and dropped to the child's user and groups. No
command was typed inside either session. The run of 2026-11-07 on a
built image made its probes the same way.

- For both children, `SetRealName`, `SetIconFile`, `SetLanguage` and
  `SetPasswordHint` on their own account, and a rename of the Guardian,
  were each refused with "Not authorized". `chfn` was refused by
  CHFN_RESTRICT. Each child's line in the account database, and their
  name, picture and language in AccountsService, were the same before
  and after. No wrong try was counted.
- `pkcheck` answered no for `change-own-user-data`,
  `change-own-password` and `user-administration`, for both children, so
  no earlier rule file answered `yes` first. It was run without its
  interactive flag, so it could not have opened a password box. That no
  box opened rests on the bus calls, and on one screenshot of each
  child's session taken after the probes. A polkit agent was found in
  ben's L3 session and none in ada's, so only in his could a box have
  opened.
- In Family a grown-up's name was refused before any password, and no
  `pkexec` was started. Escape then cleared the refusal, which is the
  fix made after the run of 2026-11-07. Opening #300's Change language
  in the same row cleared it too. That code is #300's.
- The Guardian renamed ben in Family through the real password box:
  `pkexec` ran `cairn-family set-name`, the journal said `set-name ben:
  done`, the account, his "Steam for" entry and the login screen took
  the new name, and no wrong try was counted. Family was opened by a
  helper, run from a root shell as the Guardian, that starts its menu
  entry, and its buttons were pressed with keys sent to the VM.

Add a child still worked, in #300's checks. Not tried this time: this
ADR's probes in an L2 child's session, a command typed inside a child's
session, and the Guardian's own name and picture. ben's name was put
back afterwards by a script run as root, not through Family. Of the
things not tried on 2026-11-07, the PR's question 2, the `~/.face.icon`
route and Fedora's answer for an account Cairn did not make were not
tried here either. A child's own `plasma-localerc` was tried in the same
run, but only against #300's lock, so that result is #300's to record.
No fault was found in this change.

The audit of this run left three things open that were not on this
ADR's first list: an L2 child's session on a built image, a child acting
inside their session by hand, and a name put back through Family. The
run of 2026-11-11, below, tried each. On 2026-11-11 the maintainer
decided that what is still under "Not checked yet" stays listed without
holding the label.

### Checked on 2026-11-11

In cairn-min on 2026-11-11, on the image of the re-check
(`test.20261109-295-296-300-4a5f38d`), started from the snapshot taken
after that re-check and reverted to it afterwards. The run was made for
#300, and ADR-0058 has what it saw of a child's language. ben stayed at
L2 throughout. One login of his was made, so each line about his
session is one sample. The run's record was audited against its
evidence afterwards, and what follows keeps to what the audit found
shown.

- **An L2 child's session on a built image.** ben logged in at the
  login screen and got the kiosk's launcher. The probes were then run as
  ben from a root shell placed in the cgroup of that session, with no
  terminal and with root's environment. `SetRealName`, `SetIconFile`,
  `SetLanguage` and `SetPasswordHint` on his own account, and a rename
  of the Guardian, were each refused with "Not authorized". `chfn` was
  refused by CHFN_RESTRICT. `pkcheck`, without its interactive flag,
  answered no for `change-own-user-data`, `change-own-password` and
  `user-administration`. His line in the account database, and his
  name, picture and language in AccountsService, were the same before
  and after. No wrong try was counted. A screenshot taken straight
  after shows the launcher and no password box, and no process of his
  with a name that begins `polkit` was found, so no box could have
  opened.
- **Typed by hand at L2.** Two lines were typed with keys into ben's
  Terminal. `chfn -f mum` got "chfn takes one name at a time." and
  `pkexec` got "I don't know "pkexec". Type help to see what I know."
  A look 16 to 19 seconds later found no `chfn` or `pkexec` process and
  no journal line from either, and his line in the account database
  unchanged. That look could miss a process that ran and ended at
  once; the journal and the unchanged line are the stronger signs. The
  Terminal's help lists six words (`ls`, `cd`, `open`, `cat`, `help`,
  `exit`), and none of the kiosk's key bindings opens another terminal.
  `open` was not tried. So no command that reaches polkit was typed by
  hand in this run, and that item stayed open after it. It was tried
  in an L3 child's session on 2026-11-12: see "Checked on 2026-11-12",
  below.
- **A name put back through Family.** The Guardian renamed ben to
  "Ben B" in Family through the real password box: the account
  database, AccountsService and his "Steam for" entry took the name, and
  the journal said `set-name ben: done`. Family was opened from a root
  shell that starts its menu entry, and its buttons were pressed with
  keys sent to the VM. Putting the name back did not give the same line
  as before. ben had no name in the account database before the rename,
  and Family showed his login, "ben", in the name field. With the field
  emptied and Enter pressed, Family said "Type the child's name first."
  and opened no password box. So the name was set to "ben" through
  Family, with the password box and a second `set-name ben: done`, and
  his line then held the name "ben" where it had been empty. The login
  screen read "ben" before and after. Only Enter in the emptied field
  was tried, not the Save name button. A child added in Family always
  has a name, so only an account made another way starts without one.

Not tried in this run: a command typed inside an L3 child's session,
where there is a real shell, the Guardian's own name and picture, and
the three things left untried on 2026-11-07. The first of these was
tried on 2026-11-12: see "Checked on 2026-11-12", below. The Guardian's
own name and picture, the PR's question 2, the `~/.face.icon` route and
an account Cairn did not make were not tried then either.

### Checked on 2026-11-12

In cairn-min on 2026-11-12, on a test image built on the dev PC from
main (57bd89a) with this change at 4a950d6 and #295 (0df14bf), #300
(f2423c4), #303 (5cbb741), #304 (728d184) and #306 (05bc05a) merged
together (`test.20261112-combined-8e15d47`). It has neither #302 nor
#307. So each result is for this change with the other five merged in,
not for an image built from this change alone. The levels rule in the
image is this change's, with one comment reworded by #300 and 45 lines
added by #304. Family's Children page is as #300 and #306 carry it.
This is a record of one run. Each thing below was tried once, or twice
where it says so. The run's record was audited against its evidence
afterwards, and what follows keeps to what the audit found shown.

The run tried the item still listed under "Not checked yet": the same
refusals when the commands are typed by hand inside a child's session.
It was tried at L3 only.

How the lines were typed: ben was moved to L3 from a root shell and
given a password for the run. He was logged in at the login screen with
keys, and a terminal window (Konsole) was opened on his screen with
keys. Each line in the list below was typed with keys in that window. A
screenshot was taken a few seconds after each, and no password box is in
them. "Typed with keys" means the tester sent keys to the VM's keyboard,
as a person at the computer would press them. No child was at the
computer. Most lines were typed through a one-line shell function, typed
first in the same window, that also copies each answer to a file in
ben's home. `SetRealName` and the `pkexec` line for `set-name` were also
typed without it, with the same answers.

- **AccountsService, his own account.** `busctl` calls to change his
  own name, picture, language and password hint (`SetRealName`,
  `SetIconFile`, `SetLanguage` and `SetPasswordHint`) each answered
  "Call failed: Not authorized".
- **AccountsService, the Guardian's account.** A `busctl` call to
  change the Guardian's name answered "Call failed: Not authorized".
- **chfn.** `chfn -f Mum` printed "chfn: /etc/login.defs: CHFN_RESTRICT
  does not allow any changes".
- **usermod.** The first line that `usermod -c Mum ben` printed was
  "usermod: Permission denied."
- **`cairn-family set-name`.** The line
  `pkexec /usr/libexec/cairn/cairn-family set-name ben Mum` printed
  "Error executing command as another user: Not authorized". Without
  `pkexec`, `set-name` printed "refused not-root".
- **The Users page.** `kcmshell6 kcm_users`, which asks for the
  desktop's Users page, was typed in the same window. A window opened
  and said "The module kcm_users is disabled. The module has been
  disabled by the system administrator."

Read from a root shell before and after those lines: ben's and the
Guardian's account lines, stored account values, language files and
wrong-password counts were the same.

A password box can open in that session. In the same run a start of the
Bluetooth service, which the rule does not name, brought one, and it
was cancelled with nothing typed. That check is #304's.

Saving an empty name in Family was tried on the Guardian's screen, with
keys. The Guardian was logged in at the login screen and Family was
opened from the start menu. On ben's row Change name was opened, the
field was emptied and Enter was pressed. The page said "Type the
child's name first." No password box is in the screenshots taken 2 and
5 seconds later. The field stayed open and empty. Escape closed it, and
ben's account was unchanged.

Afterwards ben was put back at L2, and the VM was put back to the
snapshot made after the image was installed.

Not tried in this run:

- A name command typed at L1 or L2. The lines typed into the Terminal
  there in this run were about the updater (#304), and the Terminal
  answered them itself. On 2026-11-11, above, `chfn -f mum` and
  `pkexec` were typed into an L2 Terminal, which answered both itself.
- A child at L4.
- The Users page reached by clicks in System Settings. It was asked for
  by name from the terminal window only.
- A change of password (`passwd`, `SetPassword`).
- In Family: a name saved through the password box, and the Save name
  button with an empty field. Enter was pressed, as on 2026-11-11.
- A second L3 login, or another child at L3.
- As before: the Guardian's own name and picture, the PR's question 2,
  the `~/.face.icon` route and an account Cairn did not make.
- An image built from this change alone, and a real computer.
