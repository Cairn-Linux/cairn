# ADR-0056: A child's name is the Guardian's to change

**Status:** proposed. It becomes accepted when the checks under "Not
checked yet" pass on an image built from this change.
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

**A child, at any level, cannot change their own name, picture,
language or anything else AccountsService keeps under
`change-own-user-data`: Cairn's levels rule refuses that action to
every account in a level group that is not an administrator. chfn
already refuses every account but root, and a build check keeps it so.
A Guardian changes a child's name on Family's Children page, with
Change name, through `cairn-family set-name` behind the Guardian's
password.**

### The rule

`session/polkit/rules.d/10-cairn-levels.rules` adds
`org.freedesktop.accounts.change-own-user-data` to the Guardian's list,
the actions it refuses at every level, L3 and L4 included (DESIGN §3.2).

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
- the name the child has, exactly: "Ada is the name already. Type a
  different one, or press Cancel.";
- a name another child on the list has, whatever the case of its
  letters, as Add a child does (#212): "Another child is called Ben
  already. Two children with one name look the same on every screen, so
  choose a different one, such as one with an initial.";
- a name the command's rule refuses, with Add a child's sentence.

The same name in other capitals is a change. Once it is in place:
"Ada is called Ada Lovelace now, on the login screen and in Family."
The field closes and the keyboard goes back to Change name. Every other
answer has the sentence it has for a level, and a refused name stays in
the field, with the keyboard there, to mend or try again.

The way not taken: running `create-child` with the child's level and
`--full-name`. It already changes an existing child's name, but it takes
only levels 1 and 2, so an older child would be refused, and it makes
the account when the child was removed in the meantime.

## Consequences

- A child cannot become "Mum" on the login screen or in Family, or
  change their picture or language, from any program.
- **A name, picture or language a child set before this image stays
  until a Guardian changes it.** Family changes the name. Family cannot
  change a child's picture or language yet; Cairn's own screens do not
  show a child's picture.
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

- An image built from this change: the build check, the rule where the
  image puts it, and checks 1 to 3 again without an overlay.
- Change name in Family, in the VM, with the real password box: the new
  name on the login screen and in Family, and the Guardian's "Steam for"
  entry renamed.
