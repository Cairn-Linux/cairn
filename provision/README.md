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
| groups `cairn-l1` … `cairn-guardian` | System groups, one per level (ADR-0011) |
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
| `/var/lib/cairn/game-pictures/` | The picture on each game's tile (ADR-0035): `<appid>.jpg`, root-owned, mode 0644, in a folder made, or put right, root-owned, mode 0755. On every run that finds the shared folder, the picture step copies the header picture of each shared game that gets a tile from the Steam of the account named with `--guardian`: `header.jpg`, or `library_header.jpg`, the name Steam gives some games' header, from `~/.local/share/Steam/appcache/librarycache/<appid>/`, or from the one folder below it named by a 40-character hash, where Steam keeps some games' pictures, and downloads nothing. It takes the home from the account database and follows no link from the home down, and copies only a plain file of at most 1 MiB that starts as a JPEG does. A picture already in place stays when the Guardian's Steam has none or one that is left out, and the `note:` line says so; the picture of a game that no longer gets a tile is removed. A game with no picture gets a `note:` line saying to open Steam in the Guardian's own login, then run the script again. A link or a file in place of the folder is refused, and a link in place of the shared `steamapps` leaves the pictures and the games file as they were, with a `note:` line |
| `/var/lib/cairn/games.json` | The children's game tiles (ADR-0033), which the launcher reads with `--games`: kidscan run as root with an empty `HOME` and `python3 -I`, with `--no-scummvm` over the shared folder only (`--library`), and `--pictures` naming each game's picture. Written on every run that finds the shared folder, and reported as changed only when it differs. With no shared folder, or when kidscan fails, the file from the last run stays |
| each child's `~/.local/share/Steam/steamapps/` | A copy of each game in the shared folder, by reflink, with its manifest copied byte for byte after it. Made on every run that finds a game in the shared folder, one child at a time, by `cairn-steam-visit --copy-games CHILD`, which lists the child in `/run/cairn/steam-visits` as a visit does, waits until nothing of theirs is running, and runs the copy as the child. A game the child already has is never touched. A child who is logged in, on a visit, or whose Steam has not run yet gets a `note:` line and their copies at a later run, and so does a game left out, with the reason. Without reflink nothing is copied. The helper keeps a record of each copy in the journal, under `cairn-steam-visit` |
| `/usr/local/bin/cairn-launcher` | Wrapper that runs the release launcher from `/usr/local/libexec/cairn` with the manifest in `/usr/local/share/cairn/manifest.json`, `--games /var/lib/cairn/games.json`, which adds a tile for each shared game after the manifest's (ADR-0033), and `--log-out /usr/local/bin/cairn-log-out`, which gives the tiles a Log out button, `--scope-apps`, which starts each program in a scope the give-up key can end (ADR-0025), and `--start-steam`, which starts the Steam client at login for a child with a Steam tile (ADR-0027). The launcher writes to the journal under `cairn-launcher` (#122). Its Draw tile runs Tux Paint with `--autosave --saveovernew`, so a child who leaves with Super+Q never loses a picture (ADR-0019) |

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
