# provision

To try Cairn on a spare computer, follow [`docs/TRY.md`](../docs/TRY.md);
this page is the script's reference.

Written for Phase 0, and still the way to try Cairn until the image and
ISO exist (P1-3, P1-4). A script that turns a stock Bazzite KDE install
(ADR-0006) into a Cairn machine, so the experience can be tested on real
hardware and with a real child **before** any image, ISO or CI exists
(DESIGN §13 Phase 0).

`cairn-provision.sh` is that script (P0-2, first run in the VM on
2026-09-06). From a checkout with a release build, on the target machine:

```sh
cmake --preset release && cmake --build --preset release   # on the dev PC
rsync -a --exclude build/debug --exclude .git . cairn-min:cairn/
ssh cairn-min                                                # then, in the VM
cd cairn && sudo ./provision/cairn-provision.sh --guardian guardian --child ada --child ben:2
```

Use the `release` preset: it keeps the install prefix `/usr/local`, so
the Guardian's window names the root helper where this script puts it.
The image installs the same files under `/usr` with `cmake --install`
instead (ADR-0038). On a computer that runs the image this script refuses
to run; [`cairn-family-setup.sh`](#on-a-computer-that-runs-the-image)
does what is left there.

When the script creates the Guardian and runs at a terminal, it runs
`passwd` for that account, which asks for the password twice without
showing it; the script never sees it. Without a terminal it says to run
`passwd` afterwards. `CAIRN_GUARDIAN_PASSWORD` in the environment sets the
password instead, but only from a root shell: `sudo
--preserve-env=CAIRN_GUARDIAN_PASSWORD` writes the value into the system
journal as part of sudo's log line, and `sudo CAIRN_GUARDIAN_PASSWORD=…`
puts it on the command line as well. Both were checked in the VM on
2026-09-30. The script traces every command (`set -x`) but never the
password step, and hands the password to `chpasswd` on its standard input
(#103).
The step puts the trace back as it found it: on only if it was on.

Before it changes anything the script checks the two names: plain login
names, two different accounts, a child that is not a system account and
not in `wheel` or `cairn-guardian`, and a Guardian that is not already in a
level group. A level change adds the new group before it removes the old
one, so a run cut off in between never leaves a child in none (ADR-0023). It refuses to install a file from the
checkout that is a link to somewhere outside it, and checks the signature
of the one package it downloads itself.
Every step checks before it changes anything and prints `changed:` when it
does, so the second run ends with `done: 0 change(s)`. The first run layers
labwc and ScummVM with `rpm-ostree` and asks for a reboot; a rerun before
that reboot still reports nothing to do.

What it leaves on the machine:

| Path | What |
|---|---|
| `/var/lib/cairn/facts.txt` | What the stock system shipped: image id, display manager, sddm and greetd presence |
| groups `cairn-l1` … `cairn-guardian` | System groups, one per level (ADR-0011). A new group takes the number the image gives it, 461 to 465, from `../system_files/usr/lib/sysusers.d/cairn.conf`; when another group has that number, it gets any free one and a `note:` line says so. Only `cairn-guardian`'s number matters to the image; see [Renumbering cairn-guardian](#renumbering-cairn-guardian). A group that is there keeps its number |
| the Guardian account | In `wheel` and `cairn-guardian`, password from the environment or set later |
| each child's account | One per `--child`, in `cairn-l1`, or `cairn-l2` when the name ends `:2`; password locked: the greeter lets an L1 or L2 child in, `su` and `ssh` do not (#35). Today L2 gets the same kiosk and tiles as L1 |
| labwc, ScummVM | Layered into the OS; Tux Paint and GCompris as system Flatpaks |
| `atkinson-hyperlegible-next-fonts`, `atkinson-hyperlegible-mono-fonts` | Layered: the brand's typefaces for the login screen, the launcher and the Terminal (DESIGN §6.2), which the Bazzite image does not carry |
| `/usr/local/share/cairn/labwc/` | The kiosk configuration from `../session/labwc/` |
| `/usr/local/bin/cairn-session`, `/usr/local/share/cairn/sessions/cairn.desktop` | The one session entry and its dispatcher (ADR-0012) |
| `/usr/local/bin/cairn-give-up`, `/etc/systemd/logind.conf.d/10-cairn-power.conf` | The grown-up's way out of a stuck program and the power button (ADR-0018, #41) |
| `/usr/local/bin/cairn-log-out` | The child's Log out, which the launcher runs (ADR-0021, #44) |
| sddm, sddm-breeze, sddm-wayland-plasma | Layered; `plasmalogin.service` disabled and `sddm.service` enabled (ADR-0016). `desktop-backgrounds-compat` is layered first from a downloaded RPM with `--force-replacefiles`, because the image carries its two wallpaper paths as symlinks no package owns |
| `/etc/polkit-1/rules.d/10-cairn-levels.rules` | What a child's session may ask the system to do, keyed off the level groups (`../session/polkit/`, #35) |
| `/etc/sddm.conf.d/`, `/etc/pam.d/sddm` | Only Cairn's session directory offered, the Cairn theme, no autologin, the greeter-only passwordless rule for L1 and L2, and the refusal of a child whose Steam a grown-up has open (ADR-0032). A `20-cairn-guardians.conf` from an earlier run, which hid the Guardians, is removed (ADR-0020) |
| `/usr/local/share/cairn/sddm/themes/cairn/`, `/usr/local/lib64/cairn/qml/` | The login screen from `../greeter/theme/`, and the `Cairn.Brand` and `Cairn.Greeter` QML modules it imports (ADR-0020) |
| `/etc/security/faillock.conf`, `/var/lib/faillock` | A lockout after wrong passwords for every account that has one: `authselect`'s `with-faillock`, with its count kept where a reboot does not clear it, and an SELinux rule labelling that directory `faillog_t` (ADR-0020) |
| `/usr/local/libexec/cairn/cairn-steam-visit`, `/usr/local/libexec/cairn/cage-visit`, `/etc/pam.d/cairn-steam-visit`, `/etc/pam.d/login`, `/etc/polkit-1/actions/com.cairnlinux.steam-visit.policy`, an SELinux rule for `/run/cairn` | A grown-up's way into a child's Steam from their own login (ADR-0032, `../session/`): the root helper; a copy of the image's cage the child may run but not read (mode 0711), made again when the image's cage changes; the visit's PAM session; Fedora's console `login` with the refusal line; the polkit action, naming the helper's real path under `/var/usrlocal`, with polkit restarted when it changes; and the `etc_t` label that lets the login screen read the list of children on a visit |
| `/usr/local/bin/cairn-steam-for`, `/usr/local/libexec/cairn/cairn-steam-for`, `/usr/local/share/applications/cairn-steam-for-CHILD.desktop` | The Guardian's window for a child's Steam (ADR-0032, `../guardian/`), and a "Steam for <child>" menu entry for every child account in a level group. The program and its wrapper are `root:cairn-guardian`, mode 0750: Plasma's menu leaves out an entry whose `TryExec` the account cannot run, so only a Guardian sees the entries. An entry whose account is no longer a child is removed. The window writes to the journal under `cairn-steam-for` |
| `/var/lib/cairn/steam-library/`, `/var/lib/cairn/steam-library/steamapps/` | The folder a Guardian's Steam installs the children's games into (ADR-0033). Both are `root:cairn-guardian`, mode 2775, with the group in the access ACL and the default ACL `d:u::rwx,d:g::rwx,d:g:cairn-guardian:rwx,d:m::rwx,d:o::r-x`, so only Guardians can change what is in it and every account can read it. Only these two folders are set; the games inside are never walked. A link in place of either is refused |
| `/usr/local/libexec/cairn/cairn-copy-games`, `/usr/local/libexec/cairn/cairn-game-pictures`, `/usr/local/libexec/cairn/kidscan.py` | The copy step (`cairn-copy-games.py`, ADR-0033), the picture step (`cairn-game-pictures.py`, ADR-0035) and kidscan, whose rules both share, mode 0644, run with `python3 -I` |
| `/var/lib/cairn/game-pictures/` | The picture on each game's tile (ADR-0035): `<appid>.jpg`, root-owned, mode 0644, in a folder made, or put right, root-owned, mode 0755. On every run that finds the shared folder, the picture step copies the header picture of each shared game that gets a tile from the Steam of the account named with `--guardian`: `header.jpg`, or `library_header.jpg`, the name Steam gives some games' header, from `~/.local/share/Steam/appcache/librarycache/<appid>/`, or from the one folder below it named by a 40-character hash, where Steam keeps some games' pictures, and downloads nothing. It takes the home from the account database and follows no link from the home down, and copies only a plain file of at most 1 MiB that starts as a JPEG does. A picture already in place stays when the Guardian's Steam has none or one that is left out, and the `note:` line says so. On the image the step also reads each child's own list in `/var/lib/cairn/games` (`GAME_LISTS`), as root, without following a link, and keeps the picture of a game still on one; a computer set up by hand has no lists. The picture of a game that is neither shared nor on a list is removed (ADR-0047). A launcher finds each game's picture here by its app id. A game with no picture gets a `note:` line saying to open Steam in the Guardian's own login, then run the script again. A link or a file in place of the folder is refused, and a link in place of the shared `steamapps` leaves the pictures and the games file as they were, with a `note:` line |
| `/var/lib/cairn/games.json` | The children's game tiles (ADR-0033), which the launcher reads with `--games`: kidscan run as root with an empty `HOME` and `python3 -I`, with `--no-scummvm` over the shared folder only (`--library`). It names no picture: the launcher finds each game's picture by its app id (ADR-0047). Written on every run that finds the shared folder, and reported as changed only when it differs. With no shared folder, or when kidscan fails, the file from the last run stays |
| each child's `~/.local/share/Steam/steamapps/` | A copy of each game in the shared folder, by reflink, with its manifest copied byte for byte after it. Made on every run that finds a game in the shared folder, one child at a time, by `cairn-steam-visit --copy-games CHILD --all`, which lists the child in `/run/cairn/steam-visits` as a visit does, waits until nothing of theirs is running, and runs the copy as the child. A game the child already has is never touched. A child who is logged in, on a visit, or whose Steam has not run yet gets a `note:` line and their copies at a later run, and so does a game left out, with the reason. Without reflink nothing is copied. The helper keeps a record of each copy in the journal, under `cairn-steam-visit`. Provisioning copies every game; `cairn-steam-visit --copy-games CHILD APPID...` (`cairn-copy-games --only APPID...`) copies only those games, for a child's own list (ADR-0041), copies nothing when given no appids, reports a listed game that is not in the shared folder as `skipped APPID not-shared`, and refuses an appid that is not a plain number before anything changes. On the image the same step then copies Steam's tools the child lacks from the tools folder ([below](#steams-tools-on-the-image)); provisioning makes no tools folder, so here it copies none and says nothing about tools |
| `/usr/local/bin/cairn-launcher` | Wrapper that runs the release launcher from `/usr/local/libexec/cairn` with the manifest in `/usr/local/share/cairn/manifest.json`, `--games /var/lib/cairn/games.json`, which adds a tile for each shared game after the manifest's (ADR-0033), and `--log-out /usr/local/bin/cairn-log-out`, which gives the tiles a Log out button, `--scope-apps`, which starts each program in a scope the give-up key can end (ADR-0025), and `--start-steam`, which starts the Steam client at login for a child with a Steam tile (ADR-0027). The launcher finds each game's picture by its app id in its default folder, `/var/lib/cairn/game-pictures` (ADR-0047). The wrapper leaves that to the default, so a launcher from an older build still starts. The launcher writes to the journal under `cairn-launcher` (#122). Its Draw tile runs Tux Paint with `--autosave --saveovernew`, so a child who leaves with Super+Q never loses a picture, and with `--noprint`, which turns its Print button off, since the Flatpak cannot reach a printer (ADR-0019) |

When something needed a grown-up, the reason is in the journal, kept
across logins. From the Guardian's account, no sudo needed:

```sh
journalctl -t cairn-launcher --since today   # what the launcher noticed
journalctl -t cairn-kiosk --since today      # the kiosk, give-up and Log out
journalctl -t cairn-steam-for --since today  # a child's Steam opened from your login
```

The launcher's lines include what the child's apps printed. Before #122 all
of it went to the child's own `~/.cache/wayland-errors`, which SDDM replaces
at every login.

Not yet: a picture for the child's tile (ADR-0020 leaves it for later), and
a `logind.conf` key for the lid.
Re-running the script puts back the repository's `manifest.json`, so a tile
added by hand in the VM goes; the game tiles in `/var/lib/cairn/games.json`
stay, and are made again from the shared folder.

## Secrets

No secret may reach the trace, a log or a command line (#103).
Every account can read a program's command line while it runs.
The only secret `cairn-provision.sh` and `cairn-family-setup.sh` handle
is the Guardian's password, in the one step described above, which both
share.

Everything else on provisioning's path was checked for a secret on
2026-10-31:

- A child has no password: this script and `cairn-family create-child`
  lock it.
- `cairn-renumber-guardians.sh` handles no secret.
- `cairn-family set-own-password`, which the Family app runs when a
  grown-up changes their own password, reads the new one on its standard
  input and hands it to `chpasswd` on chpasswd's. The journal gets whose
  password changed and the answer, never the password (ADR-0043).
- The visit helper never sees a password: `pkexec` asks for it in
  polkit's own box.
- The copy step, the picture step, the tools step and kidscan run without
  a trace, and none of them reads Steam's sign-in files.
- `CAIRN_GUARDIAN_PASSWORD` stays in the environment of the programs the
  script starts before the password step, and through the whole run when
  the Guardian already has an account. Those programs are root's own, and
  only root can read their environment. The one program provisioning runs
  as a child, the copy, starts with only the few variables `env -i` gives
  it.

`tests/test_provision.py` runs the password step, and
`tests/test_family_setup.py` the family's whole setup, with a fake
password, and both look for it in everything printed.
The first-boot wizard keeps the same rule (ROADMAP P1-6).

## On a computer that runs the image

The image already has every program and setting this script installs
(ADR-0038). What is left names a person or is the family's own choice.
`cairn-family-setup.sh` does it, from a checkout of the commit the image
was built from. Nothing needs building:

```sh
rsync -a --exclude build --exclude .git --exclude .claude . cairn-min:cairn/
ssh cairn-min                                                # then, in the VM
cd cairn && sudo ./provision/cairn-family-setup.sh --guardian guardian --child ada --child ben:2
```

It runs this script's own steps, loaded from this file, so the accounts
are checked the same way. The children come from the image's own
`/usr/libexec/cairn/cairn-family`, the command the Guardian's tool runs
too (ADR-0039). It runs only its own list of steps:

1. The checks: the names, the computer runs the image, and the checkout
   matches it. The image's copy step, picture step, tools step, visit
   helper and kidscan must be the checkout's files, though it runs only
   the picture step and kidscan itself, with `/usr/local` changed to
   `/usr` as the build changes it. On any difference it refuses and says
   to check out the commit the image was built from. `cairn-family` is
   compiled, so it cannot be compared; it must be there, and an image
   built before it is refused. A child named who already has an account
   must be one Cairn made, in a level group: `cairn-family` never makes
   another account a child. `cairn-guardian` must have the image's
   number, 465; if not, it refuses and points at the renumbering below.
2. A group that is missing, made with its fixed number. `groupadd` stops
   if another group has it.
3. The Guardian, as above.
4. Each child, with `cairn-family create-child CHILD LEVEL`: the account
   with its level group and a locked password, or an existing child's
   level, and its "Steam for <child>" entry in
   `/usr/local/share/applications`, naming `/usr/bin/cairn-steam-for`.
   The run stops on any answer but `done`, and prints the answer as it
   came; `journalctl -t cairn-family` has a line for each change.
5. Every child's entry, with the same text, named on this run or not.
   A child left off the run, or set up by hand, gets the image's entry.
   An entry whose account is no longer a child is removed.
6. The shared game folder, put right now rather than at the next boot.
7. The full list of shared games and their pictures, so the Guardian
   tool's Games page shows every game. It copies no game to any child:
   each child has their own list, chosen on that page, whose Save copies
   the games (ADR-0041). A new child has no games until then, and the
   run's last `note:` line says where to choose them.
8. The Flatpaks, last, because they need the internet. Offline, the run
   gets everything above and then stops with flatpak's own message; run
   it again once online.

It skips everything else: no build check, no layered packages, and none
of this script's programs or settings in `/usr` or `/etc`.
It writes only the accounts, in `/etc`, and the menu entries in
`/usr/local`, which is the computer's own folder.
The second run ends with `done: 0 change(s)`.

What it leaves on the computer:

| Path | What |
|---|---|
| the Guardian account | In `wheel` and `cairn-guardian`, password as above |
| each child's account | In `cairn-l1` or `cairn-l2`, password locked |
| Tux Paint and GCompris | System Flatpaks from Flathub, which the image has as a remote |
| `/usr/local/share/applications/cairn-steam-for-CHILD.desktop` | "Steam for <child>" in a Guardian's menu, running `/usr/bin/cairn-steam-for`. On Bazzite `/usr/local` is a link to `/var/usrlocal`, which belongs to the computer, and a Guardian's Plasma reads it. The image's `tmpfiles.d` makes the folder at boot. The image's window is `root:cairn-guardian`, mode 0750, so only a Guardian sees the entries |
| `/var/lib/cairn/games.json`, `/var/lib/cairn/game-pictures/` | The full list of shared games and their pictures, as above, made by the image's kidscan and picture step in `/usr/libexec/cairn`. On the image the launcher does not read the list: each child's launcher reads their own list, `/var/lib/cairn/games/<login>.json`, which the Games page's Save writes, and finds each game's picture in the folder by its app id, so a picture this run copies reaches every child who has the game (ADR-0047) |

### Steam's tools on the image

A child's games are copies, so the child's own Steam never installs them,
and would first meet the tools they run in, the Steam Linux Runtimes and
Proton, at the child's first tap (#206). On the image a Guardian's Save
brings those tools with the games instead (ADR-0046). Provisioning does
none of this.

- `/var/lib/cairn/steam-tools`, made by the image's `tmpfiles.d`, is
  `root:cairn-guardian`, mode 2775, with the shared folder's default ACL.
  It holds one folder for each tool build, `<appid>-<buildid>`, with the
  tool's manifest byte for byte and its `common/<installdir>`, never its
  `var/`.
- `cairn-share-tools` (`cairn-share-tools.py`) fills it. `cairn-family
  save-games` runs it at every Save and every Look for new games, as the
  Guardian whose password opened it, as a system job systemd stops after
  five minutes. It reads every library of that Guardian's Steam, takes
  each app kidscan's rule calls a tool whose folder has a
  `toolmanifest.vdf`, and copies it by reflink when Steam has fully
  installed it, on no beta branch, and every account can read its files.
  Each build is made in a folder of the run's own and renamed into place
  whole, and the tool's older builds go. It refuses to run as root, writes
  nothing in the Guardian's home, and says only `tool-shared APPID`,
  `tool-had APPID`, `tool-kept APPID REASON`, `tool-not-shared APPID
  REASON`, `no-steam`, `refused root`, `refused folder`, `finished` or
  `failed`.
- The copy step, after a child's games, copies each tool the child's
  library lacks from the newest build there, by a game's own rules, and
  never writes over a tool the child has. It says `copying-tools` before
  the first, then `tool-copied APPID`, `tool-have APPID` or
  `tool-skipped APPID REASON` for each, `tool-missing APPID` for a tool
  one of their tools runs in that their Steam still lacks after the copy
  (the folder has no build of it, or that build was left out, and its
  own `tool-skipped` line says why), `no-tools` when the folder is empty,
  and `tools-unreadable` when it could not be read. The visit helper
  passes these lines on, and keeps all but `tool-have` in the journal.
- Both programs take what a tool is from kidscan, `tool_installdir` and
  `TOOL_BUILD`, so they agree on it. A build stays in the folder until a
  newer build of that tool replaces it, so the folder can hold a tool the
  saving Guardian's Steam no longer has, and a child can be given it.

### Renumbering cairn-guardian

The image gives the Guardian's window the number 465, the one its
`sysusers.d` gives `cairn-guardian`. A computer set up by hand made the
group with another number, so after a move to the image no Guardian
could open "Steam for <child>". `cairn-renumber-guardians.sh` fixes that.
Run it once, before `bootc switch`, from a text console (Ctrl+Alt+F3) or
over ssh, with every Guardian logged out of the desktop and Steam closed.
A Guardian's own console or ssh login is fine, including the one that
runs the script:

```sh
sudo ./provision/cairn-renumber-guardians.sh
```

Nothing on a computer set up by hand reads the number, so it keeps
working afterwards. Each deployment has its own `/etc`, and they share
`/var`. Done before the switch, the new number goes into the image with
`/etc`, and a rollback finds it too.

- It moves every file in `/etc`, `/var/lib/cairn`, `/var/usrlocal`,
  `/var/home` and `/var/tmp` from the old number to 465, then the group,
  then walks again for a file made meanwhile. Each walk stays on its own
  filesystem and changes a link itself, never what it leads to. It can
  take some minutes, because it crosses every child's copy of the games.
- ACL entries for the old number move too. The shared game folder's
  default ACL puts the group on every file Steam made there.
- At the end it names any file still on the old number.
- It refuses while a Guardian has a desktop open (an X11 or Wayland
  session) or has Steam open, and when another group has 465. A text
  console or ssh login does not count. A refused run changes nothing.
- A file that vanishes during a walk, in `/var/tmp` or a home in use,
  does not stop the run. The check at the end still names anything a
  walk missed.
- A run cut off before the group moves starts again from the start.
  One cut off after it prints the command to run again, with the old
  number: `sudo ./provision/cairn-renumber-guardians.sh 945`, say.
  That walks the files from 945 once more. Without the number, a rerun
  sees 465 and says so.
- `chgrp` takes the setgid bit off a program file, as Linux does on any
  change of group. Folders keep theirs, and the shared game folder is
  put right by the family's setup and at every boot.
- Anyone logged in keeps the old number until they log in again.
- Only `cairn-guardian` moves. The level groups are read by name
  everywhere, so they keep their numbers.

### What the kids' laptop's move will need

Not done yet; only the maintainer runs these, on the laptop. Before anything,
check that no group has the image's numbers:
`getent group 461 462 463 464 465` should print nothing, or only Cairn's
own groups.

1. `sudo ./provision/cairn-renumber-guardians.sh`, over ssh as the
   Guardian, with every Guardian logged out of the desktop and Steam
   closed. The ssh login itself is fine.
2. `rpm-ostree reset` of the layered packages, including the forced
   `desktop-backgrounds-compat`.
3. `bootc switch` to the image, then reboot.
4. Remove `/usr/local/{bin,libexec,lib64,share}/cairn*`. The copies in
   `/usr/local/bin` come before `/usr/bin` in `PATH`, so they would
   shadow the image's. Keep `/usr/local/share/applications`.
5. Remove the `/etc` files that would override or repeat the image's
   copies in `/usr`: `/etc/sddm.conf.d/10-cairn.conf`,
   `/etc/polkit-1/rules.d/10-cairn-levels.rules`,
   `/etc/polkit-1/actions/com.cairnlinux.steam-visit.policy`, which names
   `/var/usrlocal`, and `/etc/systemd/logind.conf.d/10-cairn-power.conf`.
6. Compare `/etc/pam.d/sddm`, `/etc/pam.d/login`,
   `/etc/pam.d/cairn-steam-visit`, `/etc/security/faillock.conf` and
   `/etc/sddm.conf.d/zz-cairn-no-autologin.conf` with the image's.
   bootc keeps an `/etc` file that was changed on the computer.
7. Delete the local `semanage fcontext` rules for `/var/lib/faillock` and
   `/run/cairn`, which the image now carries.
8. Remove the leftover sudoers file.
9. Run `cairn-family-setup.sh` from a checkout of the image's commit.
   Its entries replace provisioning's, which named `/usr/local/bin`,
   for every child, named on that run or not.

## What it must do

Its first act on the VM is to record `rpm -q sddm plasma-login-manager` and
the active display manager via
`readlink -f /etc/systemd/system/display-manager.service`.
Recorded 2026-09-06 on `bazzite-44.20260902`: Plasma Login Manager
(`plasmalogin.service`), sddm and greetd not installed.

1. Create the level groups (`cairn-l1` … `cairn-guardian`, ADR-0011).
2. Create one Guardian account (password) and one L1 child account (no
   password, avatar).
3. Install the kiosk compositor (labwc proposed under D3 until P0-10 closes
   it; ADR-0004) and the Phase 0 app set: Tux Paint, GCompris (Qt), ScummVM —
   via Flatpak where available, `rpm-ostree`/`bootc` layering otherwise.
   Steam is already on Bazzite.
4. Install the session entry, dispatcher, and display manager and greeter
   configuration (ADR-0016) from `../session/`.
5. Install the launcher and shell prototypes from `../launcher/` and
   `../shell/`.
6. Be idempotent. Running it twice on the same machine changes nothing the
   second time.

## What it must not become

The product. Everything it does by hand is what the Phase 1 image does by
construction and the first-boot wizard does with a GUI. When Phase 1 ships,
this directory is deleted.
