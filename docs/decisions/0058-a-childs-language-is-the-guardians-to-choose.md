# ADR-0058: A child's language is the Guardian's to choose

**Status:** proposed. It becomes accepted when the checks under "Not
checked yet" pass on an image built from this change.
**Date:** 2026-11-09
**Closes:** issue #299. Follows ADR-0056 (a child's name is the
Guardian's), which named the language files as a known limit. Adds a
subcommand to ADR-0039's `cairn-family`, a button to ADR-0040's Children
page, and a step to ADR-0012's session dispatcher.

## Context

The maintainer decided on 2026-11-09 that a Guardian picks each child's
language, and a child does not (#299). A child who changes it by
accident may not be able to read the screens that would change it back.
A child may still change their own picture (ADR-0056's limit stands).
The concern is an accidental change, not a determined child with a
terminal.

ADR-0056 refuses AccountsService's `change-own-user-data` to children,
so a child cannot set their language over the system bus. That was not
enough. Plasma does not take the language from AccountsService at all.
It was read in source (plasma-workspace 6.8.2, SDDM 0.21, AccountsService
23.13.9, KConfig, kcmutils, systemsettings and kxmlgui as of early
November 2026) and checked on cairn-min:

- SDDM and Cairn's greeter pass no language into the session. The login
  shell sources Fedora's `/etc/profile.d/lang.sh`, which reads
  `/etc/locale.conf` and then the account's own `~/.i18n`.
- `startplasma-wayland` runs the `plasma-workspace/env` scripts, then
  reads `plasma-localerc` through KConfig's usual cascade: `[Formats]`
  `LANG`, every `LC_*` key and `LC_ALL`, and `[Translations]`
  `LANGUAGE`. Each value it finds replaces the one in the environment.
  If the file has no `LANG`, it writes the current one into the
  account's own `~/.config/plasma-localerc` at the first login.
- It then sends the environment to the systemd user manager. A key
  `plasma-localerc` sets beats `~/.config/environment.d`; a key it leaves
  out still comes from there.
- System Settings' Region & Language page writes
  `~/.config/plasma-localerc`. It also calls AccountsService's
  `SetLanguages`, which ADR-0056 now refuses without a word, and the page
  does not notice.
- Configure Language in KDE's apps writes a language for that one app,
  in `~/.config/klanguageoverridesrc`. In this KDE the entry is under
  Settings in KWrite, and under Configure in Dolphin's hamburger menu.

KDE Kiosk's `[$i]` locks are read from system-wide files. There is no
per-account lock in Kiosk itself, and `/etc/xdg` is the same for a
Guardian and a child.

## Decision

**A Guardian sets a child's language in Family, through `cairn-family
set-language` behind the Guardian's password. The command writes a
root-owned file, with every entry locked, in a folder of the child's
own under `/var/lib/cairn/locale`. `cairn-session` adds that folder,
and a Kiosk folder that hides Plasma's language settings, to the end of
`XDG_CONFIG_DIRS` for an L3 or L4 child only. For the kiosk, L1 and L2,
it exports the same values itself. Nothing is written in the child's
home.**

### The languages

Family offers a short list, kept in one table in
`guardian/src/ChildLanguage.cpp`:

| Name in Family | `LANG` and every `LC_*` | `LANGUAGE` |
|---|---|---|
| English (US) | `en_US.UTF-8` | `en_US:en` |
| English (UK) | `en_GB.UTF-8` | `en_GB:en` |

Adding a language is one line in that table, once the image has the
locale. The image has `glibc-all-langpacks`, so every locale is there.
Cairn's own screens are English until Phase 3 (ADR-0007), so a language
other than English changes Plasma and the apps that are translated, not
Cairn's own words.

### The file

`cairn-family set-language LOGIN LOCALE` writes
`/var/lib/cairn/locale/LOGIN/plasma-localerc`:

```ini
# Written by cairn-family set-language (ADR-0058). Family changes it.
[Formats]
LANG[$i]=en_GB.UTF-8
LC_ADDRESS[$i]=en_GB.UTF-8
...
LC_TIME[$i]=en_GB.UTF-8
LC_ALL[$i]=en_GB.UTF-8

[Translations]
LANGUAGE[$i]=en_GB:en
```

- Every key startplasma reads is there and locked, so neither the
  child's own `~/.config/plasma-localerc` nor their `environment.d` can
  fill one in. `LC_ALL` is set too, to the same value; without it an
  `LC_ALL` in `environment.d` would win over everything.
- The command refuses an account Cairn did not make, a grown-up's
  included, with `not-a-child`, at any level, 1 to 4, and a locale not in
  the table with `bad-language`. Family checks both before any password
  box, with the command's own rule.
- `/var/lib/cairn/locale` and the child's folder are made 0755 when
  missing, and both must be plain folders, not links, that only root
  may write in, as for each child's games file (ADR-0041). The file is
  written beside its place, 0644, and renamed into place, so a link put
  where it goes is replaced, never followed. A changed file is labelled
  again with `restorecon`. A file that already says the same is left
  alone.
- It holds the family lock, as every change to an account does (#255),
  and tells the journal `set-language LOGIN LOCALE` and the answer.
- `create-child` removes an old file of the same login first, as it does
  the games file, so a new child never starts in a language no one chose
  for them.
- The image's `tmpfiles.d` makes `/var/lib/cairn/locale` at boot.

Its polkit action, `com.cairnlinux.family.set-language`, asks for an
administrator's password every time, `auth_admin`, with the message the
other changes to an account have. Cairn's levels rule already refuses
every `com.cairnlinux.` action to every account that is not an
administrator, before any prompt, so it needs no line of its own there.

AccountsService's `SetLanguage` is not called. Nothing at login reads
it (checked below), and calling it from a root command needs D-Bus in
`cairn-family`, which has only Qt Core today. Family reads the choice
back from the Cairn file.

### The session

`session/bin/cairn-session`, for an account in `cairn-l3` or
`cairn-l4` and no administrator, before `exec startplasma-wayland`:

```sh
config_dirs="${XDG_CONFIG_DIRS:-/etc/xdg:/usr/share/kde-settings/kde-profile/default/xdg}"
config_dirs="$config_dirs:/usr/share/cairn/child-xdg"
if [ -d "$language_dir" ]; then
    config_dirs="$config_dirs:$language_dir"
fi
export XDG_CONFIG_DIRS="$config_dirs"
```

where `language_dir` is `/var/lib/cairn/locale/` followed by the login,
taken from `id -un`, never from the environment. The default is the
list Fedora's `plasma-workspace/env/env.sh` gives when it is unset;
startplasma puts `~/.config/kdedefaults` in front and keeps the rest,
and does not take `XDG_*` back from the systemd user manager. In the
image run a child's `environment.d` did not change the list for Plasma
or for the programs started in the session. It did reach three of the
child's user services. Two other start-up files of the child's did
change the list: `~/.bash_profile` its start, and a
`plasma-workspace/env` script the whole of it. See Known limits.

**After `/etc/xdg`, never before.** KConfig reads the folders in
`XDG_CONFIG_DIRS` from the last to the first, then the account's own
`~/.config`, and a group or entry locked with `[$i]` in a file read
earlier wins over every file read later. The base image's
`/etc/xdg/kdeglobals` already locks the whole group
`[KDE Control Module Restrictions]` (`build_files/hide-users-page.sh`),
so a Cairn folder in front of `/etc/xdg` is read after it and its
entries in that group are ignored. At the end, Cairn's files are read
first.

`/usr/share/cairn/child-xdg/kdeglobals` (`session/child-xdg/` in the
repository) locks two entries, each on its own rather than the group, so
the base image's `kcm_updates=false` and Cairn's `kcm_users=false` still
apply:

```ini
[KDE Control Module Restrictions]
kcm_regionandlang[$i]=false

[KDE Action Restrictions]
action/switch_application_language[$i]=false
```

The first hides the Region & Language page everywhere KAuthorized is
asked: System Settings' list and search, KRunner and Kickoff,
`systemsettings kcm_regionandlang`, `kcmshell6` and the page's own menu
entry. The second hides Configure Language in KDE's apps: under
Settings in KWrite, and under Configure in Dolphin's hamburger menu.
The key is `action/`, as `KAuthorized` reads it, not the `actions/` in
kxmlgui's header comment.

Both are a hide, not a wall. They hold while the child's session reads
this folder, and a start-up script in the child's own home can take the
folder out of the list (Known limits).

A Guardian, an account in `wheel`, and an account Cairn did not make are
left as they were: their Plasma keeps the Region & Language page and
the language they set.

For an L1 or L2 child, whose kiosk runs no startplasma, `cairn-session`
reads the same file before it starts labwc and exports each `LANG`,
`LANGUAGE` and `LC_*` line, with its `[$i]` taken off. Only a key of
that shape and a value made of a locale's characters (`A-Z a-z 0-9 _ .
: @ -`) are taken; every other line is passed over, and nothing in the
file is ever run. labwc, the launcher and the apps it starts inherit
them, and `systemctl --user import-environment` gives them to the
child's systemd too, for what it starts, such as Steam at login
(ADR-0027), as startplasma does for a Plasma session. A failed import
never stops the login.

The ways not taken:

- **AccountsService alone.** Nothing at login reads it, so a language
  set there changes no session (checked below).
- **Writing the child's own `~/.config/plasma-localerc`.** It works from
  the next login, but a root helper writing in a child's home can be led
  through a link, and the child, or Region & Language, can change it
  again.
- **A Kiosk lock in `/etc/xdg`.** It would hide the page from the
  Guardians too.

## Consequences

- An L3 or L4 child's Plasma starts in the language Family set, whatever
  their own `plasma-localerc` or `environment.d` say. System Settings
  does not list the Region & Language page, and KDE's apps do not list
  Configure Language. That stops a change by accident. It is a hide,
  not a wall: a child who writes a start-up script in their own home
  can bring both back and start their session in another language
  (Known limits).
- The Region & Language page, if a child reaches it, shows the values
  Family set and does not say they are locked. Each row's "Modify…"
  button looks live. What happens when a child picks something there
  was not shown. The page is KDE's, and Cairn does not change it.
- A language takes effect at the next login, as a level does, and
  Family says so.
- **A child with no language chosen** keeps what they had: the
  computer's language, or what their own files say. Family shows "the
  computer's own". Family cannot take a choice back to none; a
  Guardian picks the computer's language instead.
- A language a child set before this image stays until a Guardian
  chooses one.
- An empty folder left by `create-child`'s clean-up is added to
  `XDG_CONFIG_DIRS` and changes nothing.

### Known limits

What these say was seen is from the image run of 2026-11-09 (Checked).
The tries with a child's own start-up files were each one file in an L3
child's home and a fresh login, with English (UK) set in Family.

- **KDE Kiosk here is a hide, not a wall.** Every start-up file inside
  a child's own session is the child's to write. A name has a wall
  outside the session, polkit (ADR-0056); a language has none. The lock
  was asked for to stop a change by accident, and it still does that:
  a session started the usual way has no page and no menu entry. It
  does not stop a child who writes a start-up file on purpose, and that
  child can take the file away again.
- **A start-up script that replaces `XDG_CONFIG_DIRS` drops the lock
  for the whole session.** One file in the child's
  `~/.config/plasma-workspace/env/` exported `XDG_CONFIG_DIRS` as
  Fedora's default list, without Cairn's two folders, with a German
  `LANG` and `LANGUAGE`. `cairn-session` had handed startplasma the
  right list, and startplasma's own environment kept it. startplasma
  runs these scripts (Context), and everything it started had the
  child's list. The session was German, the Region & Language page
  opened, and Configure Language was back in KWrite.
  `kcm_users` and `kcm_updates` still read false; the child's list had
  kept `/etc/xdg`, where those two are locked. A list without
  `/etc/xdg` was not tried. No way to close this from inside the
  session is known. That is reasoning, not something tried.
- **The same kind of script with language variables only changed
  nothing.** With `LANG`, `LC_ALL`, `LC_TIME` and `LANGUAGE` exported
  and the list left alone, the session kept the language Family set and
  both Kiosk entries.
- **`~/.i18n`** with `LANG` alone changed nothing in Plasma or in a new
  shell. With `LANG`, `LC_ALL` and `LANGUAGE`, Plasma and both Kiosk
  entries were still as set, but a new interactive shell in the session
  had the child's `LC_ALL` and `LANGUAGE`, and `date` there named the
  day in the child's language.
- **`environment.d`** with `LANG`, `LC_TIME`, `LC_ALL`, `LANGUAGE` and
  `XDG_CONFIG_DIRS=/etc/xdg`: Plasma and the programs started in the
  session kept Cairn's list and the language Family set, and
  `systemctl --user show-environment` answered the same. Three of the
  child's user services, mpris-proxy and the two dbus-broker processes,
  had the child's values and the child's list. Whether anything on
  screen follows them was not looked at.
- **`~/.bash_profile`** exporting `XDG_CONFIG_DIRS=/etc/xdg`, `LANG`,
  `LC_ALL` and `LANGUAGE`: the session kept the language Family set and
  both Kiosk entries. The child's value did become the start of the
  list, since `cairn-session` adds its folders to the list it is
  given, so the session lost
  `/usr/share/kde-settings/kde-profile/default/xdg`. A start without
  `/etc/xdg`, and `exec startplasma-wayland` in that file, were not
  tried.
- **A child with a terminal can still set the variables for programs
  they start**, or in their shell's start-up files. With a language set
  in Family, `LANG=de_DE.UTF-8` in front of a command changed neither
  the date nor the messages, and adding `LC_TIME` did not change the
  date either. `LC_ALL=de_DE.UTF-8` changed the date, and `LC_ALL` with
  `LANGUAGE=de` changed the messages. `LANGUAGE` alone was not tried.
  `XDG_CONFIG_DIRS` can be set the same way: with the Kiosk folder left
  out for one program, `kcmshell6` opened the Region & Language page.
  That is on purpose, not by accident, and out of scope.
- Apps with their own language setting, such as Steam and Firefox, keep
  it. A keyboard layout (`kcm_keyboard`) is not the display language and
  stays open.
- Steam started the one game tried with `LC_ALL=C`, with and without a
  language file, and the game looked the same both ways. Whether a
  language set in Family reaches any Steam game was not shown.
- At L1 and L2 the values come from `cairn-session`, not Plasma, and
  KDE Kiosk does not apply; the kiosk offers no settings to change them.
- KDE Kiosk is read by KDE's programs. A GTK or Qt app that is not KDE's
  follows only the environment, which this sets.

## Checked

Before this change was written, on cairn-min on 2026-11-09, on
`test.20261107-276-292-2c4643a` (plasma-workspace 6.8.2, KConfig 6.31,
SDDM 0.21, AccountsService 26.27.3, glibc-all-langpacks), from snapshot
`before-299-research` and reverted to it afterwards. ben was moved to L3
for the run, and logged in with SDDM autologin through a test file. A
test `cairn-session` in `/usr/local/bin`, found first on `PATH`, added
the folders for ben and then ran the image's. Each login was read from
plasmashell's `/proc/PID/environ` and `systemctl --user show-environment`.

1. With no `plasma-localerc`, the first login wrote ben's own with
   `LANG=en_US.UTF-8`, and `XDG_CONFIG_DIRS` was Fedora's default.
2. AccountsService `SetLanguage de_DE.UTF-8`, as root: the account's
   file held it, and the next session was still `en_US.UTF-8`.
3. `environment.d` with `LANG`, `LANGUAGE` and `LC_TIME` against a
   `plasma-localerc` with only `LANG`: `LANG` came from the file,
   `LANGUAGE` and `LC_TIME` from `environment.d`.
4. A Kiosk folder in front of `/etc/xdg`, locking the group: ignored.
   `kreadconfig6` read `kcm_regionandlang` as true, and
   `systemsettings kcm_regionandlang` opened the page.
5. The same folder after `/etc/xdg`, locking each entry, with
   `kcm_regionandlang=true` in ben's own `kdeglobals`:
   `kcm_regionandlang`, `kcm_users`, `kcm_updates` and
   `action/switch_application_language` all read false. System Settings
   opened its front page, not Region & Language; its search no longer
   listed the page; `kcmshell6` said the module was disabled by the
   administrator; KRunner found no page.
6. A root-owned `plasma-localerc` with locked `LANG` and `LANGUAGE` in
   a folder after the Kiosk folder, against ben's own file and
   `environment.d`: the session took the locked values. With ben's own
   file removed, it was not written again at login. A key the locked
   file left out, `LC_TIME`, still came from `environment.d`; this
   change locks every key for that reason.
7. As the Guardian, with the default `XDG_CONFIG_DIRS`,
   `kcm_regionandlang` and `action/switch_application_language` read
   true.

Built and tested on the development PC on 2026-11-09: the release build,
with `cairn-family set-language`, Family's Change language, and
`cairn-session`, each with their tests.

### On an image built from this change

In cairn-min on 2026-11-09, on `test.20261109-295-296-300-4a5f38d`
(main 57bd89a with this change at a782c4a, which holds #296, and #295
merged; 106 of 106 tests, `install_tree` among them, passed in the
build). The build has no node, so the three cases that run the polkit
rule were skipped there; they passed on the development PC from the
same tree. The image was built from a782c4a. Later commits on this
branch change words only: this ADR, the comment in the Kiosk file, and
lines in DESIGN.md, ROADMAP.md and the session and guardian READMEs.
The test accounts were ada (L1), ben (L2, moved to L3 for the Plasma
checks), cy (added and removed) and guardian. The language files,
ben's level and every file put in ben's home were taken back
afterwards.

- **The image.** `/var/lib/cairn/locale` was there after boot, 0755
  root. The Kiosk file was in `/usr/share/cairn/child-xdg`, 0644 root,
  the same bytes as the repository's at a782c4a.
- **Family, with the real password box.** The Guardian set English (UK)
  for ben and for ada. Each file was root-owned, 0644, 491 bytes, in a
  0755 folder, labelled `var_lib_t`, with `LANG`, every `LC_*`,
  `LC_ALL` and `LANGUAGE` locked. Nothing in either child's home
  changed, and the journal had `set-language ben en_GB.UTF-8: done`. A
  change that was cancelled left no file and no journal line.
- **Refusals.** From a root shell, `cairn-family set-language` refused
  the Guardian, an account in `wheel`, root and a login that does not
  exist (`not-a-child`), a locale not in the table (`bad-language`), a
  login with a path in it (`bad-name`) and a caller that is a child
  (`not-a-grown-up`). In ada's session at L1 and ben's at L2 and at L3,
  polkit answered no for the action, `pkexec` said "Not authorized"
  with no password box, a write into the folder was refused, and
  faillock stayed at 0. Those probes were started from a root shell,
  moved into the child's session and dropped to the child's user; they
  were not typed in the session.
- **L1 kiosk.** At ada's next login, labwc, the launcher, the Steam
  client and `systemctl --user show-environment` had `en_GB.UTF-8` in
  `LANG` and every `LC_` key. Tux Paint said "Colours"; that was the
  only change seen on screen. PipeWire, which `cairn-session` starts
  before it reads the file, kept `en_US.UTF-8`.
- **L3 Plasma.** ben, at L3, logged in through the Cairn greeter with a
  typed password. plasmashell, kwin and
  `systemctl --user show-environment` had `en_GB.UTF-8`, with both
  Cairn folders last in `XDG_CONFIG_DIRS`. Both Kiosk entries read
  false, and ben's own `plasma-localerc` was not written.
- **The page and the menu entry.** Region & Language was not in System
  Settings' list, its search or KRunner. `kcmshell6 kcm_regionandlang`
  said the module was disabled, and `systemsettings kcm_regionandlang`
  opened the front page. The Keyboard page still opened. Configure
  Language was not in KWrite's Settings menu, nor under Configure in
  Dolphin's hamburger menu. A helper started System Settings,
  `kcmshell6`, KWrite and Dolphin in ben's session; only KRunner was
  driven by keys.
- **L3 with no language file.** The Kiosk folder was still last in the
  list and both entries read false. The page and the menus were not
  opened in that session.
- **The child's own files against the lock.** Nine tries, each a file
  in ben's home and a fresh login: his own `plasma-localerc`;
  `environment.d`, twice; `~/.i18n`, twice; his own `kdeglobals` with
  both entries true; a `plasma-workspace/env` script, twice; and
  exports in `~/.bash_profile`. Eight of the nine left Plasma in
  English (UK) with both entries false. **One did not: the
  `plasma-workspace/env` script that replaced `XDG_CONFIG_DIRS` gave a
  German session with the page and Configure Language back.** Known
  limits says what each try showed.
- **The locked page.** Opened for ben with the Kiosk folder left out of
  one program's list, the page showed British English, 09/11/2026 and
  £24.00. Every row had a live "Modify…" button, and nothing said the
  values were locked.
- **A grown-up's session.** The Guardian's list had no Cairn folder,
  the page opened, and Configure Language was in KWrite's and Dolphin's
  menus. An account in `wheel` that Cairn did not make was left alone
  too, through the greeter. The last branch of `cairn-session`, for an
  account in neither `wheel` nor a Cairn group, was run with stand-ins
  for `id` and `startplasma-wayland`, not by a login.
- **A removed child's file.** cy was added in Family, given English
  (UK), and removed outside Family, which left the file. Adding cy
  again in Family removed it, and cy's row said "the computer's own".
- **Login time.** One sample per case, with a 0.25 s poll, from Enter
  to the launcher for ada: 0.89 s and 1.24 s with a file, 1.19 s,
  1.29 s and 1.12 s without. That shows no large slowdown and nothing
  finer.

### Not checked yet

- An L2 child's login with a language file. ben was at L3 by the time
  he had one.
- L4.
- Konsole's menu, with the lock or without.
- What the Region & Language page does when a child picks something
  while the values are locked.
- A greeter login of an account in neither `wheel` nor a Cairn group.
- `exec startplasma-wayland` in a child's `~/.bash_profile`.
- The `plasma-workspace/env` script or the `~/.bash_profile` exports
  with a list that leaves out `/etc/xdg`.
- A language other than English (US) and English (UK) set through
  Family. The table has only those two; the child's own files in the
  nine tries used German, French and Spanish.
- A game shown in another language, since Steam gave the one game tried
  `LC_ALL=C`.
- Login time beyond one sample per case.
