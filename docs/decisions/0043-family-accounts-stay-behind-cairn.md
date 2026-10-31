# ADR-0043: The family's accounts stay behind Cairn's Family app

**Status:** proposed. Every check under "Checked" below passed in the
cairn-iso VM on 2026-10-30, from a test copy of this branch's programs
at a path of their own. It becomes accepted when the same checks pass
with the programs and files where the image puts them. "Only KDE's
pictures", added on 2026-10-31, has had a dry run in the VM so far.
**Date:** 2026-10-30; "Only KDE's pictures" 2026-10-31
**Closes:** (none) — the System Settings half of issue #113, and the
second item of the parent app Mason agreed (ROADMAP P1-5). Amends
ADR-0040 (a third page, and a second way in) and narrows the gap
ADR-0023 left open at its rank 4. Adds a step to ADR-0038's image
(only KDE's pictures, part of #166).

## Context

A parent who opens System Settings on Cairn finds Plasma's Users page.
There they can add an account, and Cairn does not know it: it is in no
level group and not an administrator, so ADR-0023 ranks it 4, an
account Cairn did not make, and gives it Plasma with a password. For a
grandparent that is right. For a second child it is a full desktop and
no limits, made by a parent who thought they were adding a child. The
Users page can also change a child's password or make a child an
administrator, behind the Family app's back.

Issue #113 found the pieces on 2026-09-30, on Plasma 6.7.5:

- KDE Kiosk hides one System Settings page with a line in a system
  `kdeglobals`, `kcm_users=false` in `[KDE Control Module
  Restrictions]`. System Settings and KCMUtils both ask
  `KAuthorized::authorizeControlModule` before they show a page.
- System Settings lists a program of another kind beside its own pages:
  a `.desktop` file of type `SystemSettingsExternalApp` in
  `share/plasma/systemsettings/externalmodules/`, as Info Center lists
  Plasma System Monitor.

The catch, also in #113: the Users page is where a Guardian changes
their own password and picture. Hiding it takes both away, so the Family
app has to do them first.

Bazzite already ships `/etc/xdg/kdeglobals`, with a group of its own,
`[KDE Control Module Restrictions][$i]` holding `kcm_updates=false`
(read in the VM, 2026-10-29). A file of Cairn's in its place would drop
Bazzite's look and its own restriction.

Plasma's Users page sets a password through AccountsService's
`SetPassword`, which takes a finished hash, not the password: the page
links `libcrypt` and makes the hash itself with `crypt_gensalt` and
`crypt` (read from `kcm_users.so` on Fedora 44). It sets the picture
with AccountsService's `SetIconFile`.

## Decision

**System Settings shows "Family" where the Users page was, and the
Users page is hidden with KDE Kiosk. A Guardian changes their own
password and picture on a third page of the Family app, "Your account".
The picture goes to AccountsService over Qt D-Bus; the password goes
through `cairn-family set-own-password`, which pkexec allows only after
the Guardian types the password they use now.**

### Family in System Settings

`/usr/share/plasma/systemsettings/externalmodules/cairn-guardian-settings.desktop`,
of type `SystemSettingsExternalApp`, named "Family", under System
(`system-administration`), the category Plasma gives the Users page. Its
`Exec` and `TryExec` are `/usr/bin/cairn-guardian`, the wrapper the menu
entry runs (ADR-0040). System Settings' search finds it by its name only:
it reads no keywords or comment for a program of this kind (VM,
2026-10-30). Like the menu entry it is plain English until the first
translation (ADR-0007).

### The Users page hidden

The image's `build_files/hide-users-page.sh` adds `kcm_users=false` to
the `[KDE Control Module Restrictions]` group of `/etc/xdg/kdeglobals`,
Bazzite's own group, and keeps every other line. When there is no such
group it adds one marked `[$i]`, as Bazzite's is, so no account's own
`kdeglobals` can show the page again. Run twice, it changes nothing. The
image build runs it right after `system_files/` is laid down.

### Your own picture

The page shows the Guardian's name and picture, KDE's own pictures to
tap (the PNG and JPEG files at the top of `/usr/share/plasma/avatars`,
then those in its `photos` folder; see "Only KDE's pictures"), and
**Choose a picture of your own**, which opens Qt Quick's own file dialog
(`QtQuick.Dialogs`, part of Qt). The window makes the chosen picture
a square from its middle, turned the way the camera meant, at most 256
pixels wide, as a PNG in a folder of its own that it removes on closing,
then calls `SetIconFile` on the Guardian's own
`org.freedesktop.Accounts.User` object on the system bus.

- AccountsService copies the file into
  `/var/lib/AccountsService/icons/`, where Plasma's lock screen reads it.
  Cairn's login screen draws each person's first letter, not a picture
  (ADR-0020), and Bazzite's menu shows no picture. AccountsService
  refuses a file over a megabyte; a 256-pixel PNG is far under.
- polkit's `org.freedesktop.accounts.change-own-user-data` is `yes` for
  an account at the machine, so no password is asked. The call allows a
  polkit box, in case a rule ever asks for one.
- Qt's D-Bus module is part of Qt and already a dependency of the Family
  app (ADR-0041, for logind). No new dependency.

### Only KDE's pictures

Added on 2026-10-31, after a user test in the cairn-iso VM (#166).
The page offered 32 pictures.
17 were KDE's: Konqi in his outfits, and Katie.
15 were game art from Half-Life, Portal, Team Fortress 2 and others,
some with weapons or fire.
A picture the Guardian picks shows on Plasma's lock screen, where
anyone at the computer sees it.

What is in Plasma's picture folder on the image, read in the VM with
`rpm -qf` and `rpm -ql`:

- **15 links at the top,** such as `alyx.jpg` and `gmod_gman.jpg`, which
  no package owns. They lead to Bazzite's copies of the art in
  `/usr/share/pixmaps/faces`, GNOME's folder of pictures, under GNOME's
  names: `alyx.jpg` leads to `hummingbird.jpg`. No package owns those
  15 copies or their folder either.
- **Three folders,** `tf2/`, `Portal2/` and `circles/`, with 23 pictures
  in all. `steamdeck-kde-presets-desktop` owns them, the package of the
  Steam Deck's look.
- **KDE's own,** from `plasma-workspace`: the 17 drawings at the top, and
  20 photos in `photos/`, each with a licence file beside it. The page
  did not list `photos/`.

**The image keeps only what `plasma-workspace` ships in Plasma's picture
folder, and the page lists KDE's photos after its drawings.**

- `build_files/kde-pictures.sh` removes every file, link and folder in
  `/usr/share/plasma/avatars` that `plasma-workspace` does not own. It
  also removes every file and folder in `/usr/share/pixmaps/faces` that
  no package owns. It asks rpm about each path, and rpm judges a link by
  the link itself, not by what it leads to.
- It removes files and links first, then folders, which are empty by
  then. A folder that still holds a KDE picture makes `rmdir` refuse,
  which stops the build rather than lose the picture. An rpm that cannot
  answer stops it before anything goes. With `--dry-run` it only prints
  what would go.
- `build_files/check-guardian-desktop.sh` stops the build unless the
  folder holds exactly what `rpm -ql plasma-workspace` lists there, with
  drawings at the top and photos in `photos/`. It also stops it if
  `/usr/share/pixmaps/faces` holds anything no package owns. So a base
  update that moves KDE's pictures to another package stops the build.
- The page lists the PNG and JPEG files at the top of the folder, then
  those in `photos/`, each in name order: 17 drawings, then 20 photos.
  It opens no other folder.
- `circles/` goes too. It is the Steam Deck look's, not KDE's: six
  pictures named Cat, Female, Konqui, Male, Penguin and Zebra. KDE's own
  set already has Konqi. One rule, only KDE's, is easier to check and to
  keep than a rule with an exception.

The other ways weighed:

- **Remove only what no package owns.** That takes the 15 links but
  leaves `tf2/` and `Portal2/`, which a package owns. Any picker that
  opens the folder's folders would still offer them.
- **A fixed list of Bazzite's files.** It would name Valve's characters
  in Cairn's repository, and it needs a change whenever Bazzite adds or
  renames a picture. The rule needs none: a new picture of anyone's but
  KDE's goes with the rest.
- **A filter in the Family app.** The art would still ship in the image,
  against ADR-0035 ("no game art is shipped in the image") and the base
  image policy (no Bazzite artwork in Cairn's UI). Plasma's Users page
  would offer it again if a Plasma upgrade brought the page back (see
  Consequences). The app would also have to ask rpm while it runs, or
  keep the list above.

Removing package files has a cost: `rpm -V steamdeck-kde-presets-desktop`
lists the three folders and their 23 pictures as missing. Nothing on the
computer reads that list, and each new image removes them again.

### Your own password

The page asks for the new password twice. It checks both before
anything runs, with the one rule both sides share (`NewPassword`):
not empty, at most 128 characters, and no tab, line break or other
character that does not show. Cairn sets no length or strength rule of
its own beyond that; Fedora's `pwquality` asks for one only of a person
changing their own password through PAM, which this is not. The page
says to choose one the children do not know.

Then it runs `pkexec /usr/libexec/cairn/cairn-family set-own-password`,
with the new password and a line break on its standard input, never in
its arguments.

- **The current password is the check.** The polkit action
  `com.cairnlinux.family.set-own-password` is `auth_self`: polkit asks
  for the password of the account at the screen, the one being changed,
  every time. Another Guardian's password does not do, as it would with
  `auth_admin`. polkit's levels rule answers no to every account that is
  not an administrator, before any box, as for every Cairn action.
- **It changes only the caller's own password.** The subcommand takes no
  login. `cairn-family` checks, as for every subcommand, that it runs as
  root and that `PKEXEC_UID` names an administrator, and takes the login
  from the account database for that number. Under `sudo` no grown-up is
  named, and it is refused with `not-a-grown-up`.
- **The password never leaves a pipe.** pkexec hands its standard input
  to the command, and Plasma's polkit box asks for the current password
  in the meantime, so the new one waits in the pipe, which only the
  command can read. `cairn-family` reads at most what the rule allows,
  one line of UTF-8, checks it again, and writes `LOGIN:PASSWORD` to the
  standard input of `/usr/sbin/chpasswd`, started directly, with an
  environment of its own. chpasswd makes the hash with the method in
  `/etc/login.defs`, yescrypt on Fedora. The password is never an
  argument, which every account can read in `/proc` while a program
  runs, and never written to the journal; the journal gets
  `set-own-password LOGIN` and the answer (#103).
- **Answers** are the ones every subcommand has, `done` or `refused`
  and a word: the caller's words, `usage`, `failed` when chpasswd fails,
  and a new word, `bad-password`, for an input the rule does not take.
  The window turns each into a sentence, as on the Children page, and a
  closed box or a wrong current password into "It needs the password
  you use now". A changed password says to use the new one from now on:
  to log in, to unlock the screen and in every password box. The new
  password stays in the two fields after a refusal, so a slip is mended
  without typing both again, and is cleared once it is in place.

AccountsService's `SetPassword`, the Users page's way, was not taken:

- It needs the hash, not the password, and making a hash in C++ needs
  `libcrypt`, a new dependency (ADR-0002). chpasswd already makes it,
  with the system's own method, and is on every Fedora.
- AccountsService hands the hash to `usermod -p` as an argument, where
  every account can read it while usermod runs.
- Fedora's polkit default for `change-own-password` is `auth_admin`, so
  with two Guardians either one's password would do.

`passwd` run as the Guardian was not taken either: it asks for the old
password on a terminal, and its `--stdin` is for root only.

### What stays open

- **Adopting an account Cairn did not make** stays for #113's next part:
  giving such an account a level, or making it a Guardian, from the
  Family app. ADR-0023's rank 4 is unchanged: an account Cairn did not
  make still gets Plasma and a password.
- What hiding the page does not stop is recorded under "Checked" below.
  An administrator can still make an account another way, such as
  `useradd` in a terminal; Cairn does not try to stop a grown-up who
  means to.

## Checked

In the cairn-iso VM on 2026-10-30, at the Guardian's own Plasma, with a
Release build of this branch installed under `/usr/local/libexec/cairn-test`,
the action file copied under its own id with that path, the externalmodules
entry in place and `build_files/hide-users-page.sh` run on the real
`/etc/xdg/kdeglobals`. All of it was taken away afterwards and the
Guardian's password and picture put back as they were.

1. `hide-users-page.sh` put `kcm_users=false` into Bazzite's own group
   and changed no other line; `kreadconfig6` read it as `false`.
2. `systemsettings --list` lists `cairn-guardian-settings`, "The
   children, their levels and games, and your own password and
   picture", and no `kcm_users`. In the window, Family is under System,
   between About this System and Power Management, and there is no
   Users entry.
3. System Settings' search: "fam" finds Family; "users" finds nothing;
   "pict" does not find Family. Picking Family opened the Family window.
4. Ways round the hidden page: `systemsettings kcm_users` opens System
   Settings on its start page; `kcmshell6 kcm_users` opens a window that
   says "The module kcm_users is disabled. The module has been disabled
   by the system administrator."; KRunner, asked for "users", lists
   "Users - System Settings" under Global Shortcuts, which opens System
   Settings on its start page. None of them shows the page. KRunner's
   entry is a dead end rather than a way in; Kiosk has no line for it.
5. A Plasma picture tapped on Your account became the Guardian's
   picture at once, with no password: AccountsService's `IconFile` named
   `/var/lib/AccountsService/icons/guardian`, a 256 by 256 PNG, and the
   lock screen showed it. A JPEG chosen in the file dialog did the same.
6. A new password typed twice opened Plasma's polkit box with the
   action's own message, "Type the password you use now to change it.",
   "Authenticating as Test Guardian (guardian)". Cancel left the
   password as it was, said so, and kept both fields. The current
   password then changed it: `sudo` took only the new one, the hash
   starts `$y$` (yescrypt), and the journal has `set-own-password
   guardian: done` and the password nowhere. A wrong current password
   was refused inside the box, which asked again; the right one changed
   it back. `faillock` counted nothing.

pkexec finds the action by the program's real path, with every link
followed. On a bootc system `/usr/local` is a link to `/var/usrlocal`,
so the test copy's action had to name `/var/usrlocal/...`; with
`/usr/local/...` pkexec fell back to its own action and Plasma's box
said "Authentication is needed to run ... as the super user". The image
puts `cairn-family` in `/usr/libexec/cairn`, which is no link.

"Only KDE's pictures", on 2026-10-31: `kde-pictures.sh --dry-run`, run
on the VM over ssh without a copy there, asked the VM's own rpm about
every path. It listed 57 to remove: the 15 links, the three Steam Deck
folders with their 23 pictures, and the 15 copies in
`/usr/share/pixmaps/faces` with their folder. It listed none of KDE's
17 drawings, its `photos` folder or the 40 files in it. The picture
lines of `check-guardian-desktop.sh`, run on the VM the same way,
failed there as they should, and passed on the dev PC's Fedora KDE.
Still to check in the VM: the removal itself, and the page showing
KDE's 37 pictures and no other.

## Consequences

- A parent who looks for accounts in System Settings finds Family, in
  Cairn's words, and no page that can make a child Cairn does not know.
- A Guardian's password and picture keep working with the Users page
  gone, and the password change asks for the current password, as the
  Users page did.
- A Guardian chooses from KDE's 37 pictures, and nothing on the image
  offers Valve's game art as a picture. A Guardian who chose one of
  those before keeps it, because AccountsService keeps its own copy,
  until they choose another.
- Only the image build removes the game art. A computer set up by hand
  with `provision/cairn-provision.sh` keeps Bazzite's `/usr` as it is,
  so Family there still offers the 15 game pictures beside KDE's 37.
- Where a fingerprint reader is set up, polkit takes the Guardian's
  fingerprint in place of the current password, as in every other polkit
  box on the machine.
- A forgotten Guardian password is not mended here. Another Guardian,
  or the machine's own recovery, is the way, as before.
- KWallet keeps its own password. Plasma's Users page offers to change
  the wallet's password with the login password; the Family app does
  not, so a Guardian whose wallet opened at login with the old password
  is asked for the old one by the wallet next time.
- If a Plasma release renames the Users page's id, the line hides
  nothing and the page comes back. The build does not catch that; check
  2 above does, and should be run again after a Plasma upgrade.
- KRunner still offers "Users - System Settings", which opens System
  Settings without the page.
- Three tabs fill the switch at the window's narrowest width. A fourth
  page will need another layout.
