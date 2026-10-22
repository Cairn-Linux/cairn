# ADR-0038: The first Cairn image

**Status:** accepted 2026-10-21. The image booted in the VM to the Cairn
login screen, a child reached the tiles, and a Guardian reached Plasma.
**Date:** 2026-10-21
**Closes:** (none) — the first slice of issue #164; part of ROADMAP P1-2
and P1-3

## Context

Phase 0 ended on 2026-10-12 (ADR-0031). Image work starts on the dev PC
(ROADMAP §0). Until now a Cairn machine is a stock Bazzite install that
`provision/cairn-provision.sh` changes by hand. It copies every built file
into `/usr/local` and `/etc` with its own `install_file`, and writes two
wrapper scripts itself. The CMake build had no install rules at all.

The image is `FROM ghcr.io/ublue-os/bazzite:stable`, a bootc image
(ADR-0006). On Bazzite `/usr/local` is a link to `/var/usrlocal`. `/var`
belongs to the installed machine, not the image, so an image cannot put a
file there. Everything Cairn ships has to move.

Checked on the dev PC on 2026-10-21, in a `podman build` from the Bazzite
image:

- Fedora's `qt6-*-devel` packages match the base's Qt 6.11.2 exactly.
  Installing them updates systemd from 259.9 to 259.10, inside that build
  only.
- `authselect enable-feature with-faillock` works. So does
  `semanage fcontext`, which prints a harmless warning about renaming
  across devices.
- `bootc container lint` passes, with one warning about
  `/var/lib/authselect/checksum`.
- `dracut` and `plymouth-set-default-theme` are in the base.
- `/usr/bin/cage` is in the base. `/usr/lib/sddm/` and `/etc/sddm.conf.d/`
  are not, because Bazzite uses Plasma Login Manager (ADR-0016).

## Decision

**Cairn is compiled in a stage of the Containerfile that is thrown away,
and `cmake --install` puts every file in its standard place under `/usr`.
The image copies that tree. The image ships nothing in `/usr/local`, and
nothing that names a person goes in the image.**

### How it is built

The build stage starts `FROM` the same Bazzite image and installs Fedora's
`qt6-*-devel`. Its systemd update never reaches the image. The stage
configures with `-DCMAKE_INSTALL_PREFIX=/usr` and installs into a folder
(`DESTDIR`). The final stage copies that folder over `/`. RPMs (P1-2) can
replace the copy later, because the files already sit where an RPM would
put them.

The stage runs `ctest` after compiling, and a failing test stops the
image. Compiling and testing run as an unprivileged user, because some
tests refuse to run as root or check what a person without root cannot
read; only the install runs as root, so every file is owned by root. Tests
that need what a container build lacks skip themselves and are listed in
the build log, so `ctest` on the host is still required. The stage stops
if Fedora's `qt6-*-devel` packages are not exactly the version-release of
the Qt the base ships.

The paths inside Cairn's files are fixed when the build is configured.
The project's default prefix is now `/usr`. The `release` preset keeps
`/usr/local`, because provisioning installs from that build and its
programs must name provisioning's paths.

### Where each file goes

`cmake/CairnInstall.cmake` names each place as a cache variable, so a
packager can move one.

| Place | What | Why there |
|---|---|---|
| `/usr/bin` | `cairn-session`, `cairn-launcher`, `cairn-give-up`, `cairn-log-out`, `cairn-steam-for` | The session, a menu entry or a person runs them by name |
| `/usr/libexec/cairn` | The launcher and the Guardian's window, `cairn-steam-visit`, the copy and picture steps, `kidscan.py` | Programs only other programs run |
| `/usr/lib64/cairn` | `libCairnBrand.so` | Private to Cairn; the programs find it through their rpath |
| `/usr/lib64/cairn/qml` | The `Cairn.Brand` and `Cairn.Greeter` modules | See below |
| `/usr/share/cairn` | The kiosk configuration, the manifest, `sessions/cairn.desktop` | Data |
| `/usr/share/sddm/themes/cairn` | The login screen | Where SDDM looks by default |
| `/usr/lib/sddm/sddm.conf.d/zz-cairn.conf` | SDDM's settings for Cairn | SDDM's folder for packaged settings |
| `/usr/lib/systemd/logind.conf.d` | The power button | logind's folder for packaged settings |
| `/usr/share/polkit-1/` | The level rules and the visit's action | polkit's folders for packaged rules and actions |
| `/etc/pam.d`, `/etc/security/faillock.conf`, `/etc/sddm.conf.d/zz-cairn-no-autologin.conf` | PAM, faillock, no autologin | Only these must be in `/etc` |

The QML modules stay in Cairn's own folder, not in Qt's
`/usr/lib64/qt6/qml`. Only the login screen imports them, and SDDM's
settings already put that folder on its import path. In Qt's folder every
Qt program on the machine could import them, and a later Fedora package
could clash with them.

`cairn.desktop` stays in Cairn's own `sessions` folder, not in
`/usr/share/wayland-sessions`. SDDM's `SessionDir` names only that folder,
so the login screen offers Cairn's one session and nothing else
(ADR-0012).

SDDM's packaged settings are renamed `zz-cairn.conf`. SDDM reads its
packaged folder in name order, and Fedora's `plasma-wayland.conf` there
sets `GreeterEnvironment` too. Cairn's line must come later to win.

The no-autologin file stays in `/etc/sddm.conf.d`. It must override a
file Plasma's setup writes there, and SDDM reads `/etc` after its packaged
folder.

PAM's `login` and `sddm` replace files Fedora ships in `/etc/pam.d`, and
a file there wins over `/usr/lib/pam.d`. `cairn-steam-visit` sits beside
them. `pam_faillock` reads its settings only from `/etc`.

### The files provisioning also installs

The repository's files name provisioning's `/usr/local` paths, and
provisioning installs them as they are. The build copies each one and
changes those paths to its own. A file that names some other `/usr/local`
path stops the configure step, so no image can ship one.

The two wrappers, `cairn-launcher` and `cairn-steam-for`, are templates
the build fills in. Provisioning keeps writing its own, unchanged. The
image's wrappers set no `LD_LIBRARY_PATH`: the programs carry an rpath to
`/usr/lib64/cairn` instead, so the games the launcher starts do not
inherit it.

The Guardian's window names the root helper it asks `pkexec` to run. That
path now comes from the build.

### What the image does itself (slice 3)

Some steps are not files the build makes. Two scripts in the final stage
do them, and the files they need come from `system_files/`, because
provisioning has no use for them.

- **Layered packages.** `build_files/packages.sh` installs labwc,
  ScummVM, `sddm`, `sddm-breeze`, `sddm-wayland-plasma` and the brand's
  two fonts with `dnf5`, the list provisioning layers. None is in the
  base. It runs before Cairn's files are copied in. Fedora's `sddm` ships
  `/etc/pam.d/sddm`; installed after Cairn's, rpm would keep Cairn's and
  leave its own beside it as `sddm.rpmnew`.
- **desktop-backgrounds-compat.** `sddm` needs it, and its two wallpaper
  links clash with links in the base that no package owns. Provisioning
  forces it over them in an `rpm-ostree` transaction of its own. `dnf5`
  in a build replaces a file no package owns without asking, so here it
  needs no step of its own. Fedora's default wallpaper links replace
  Bazzite's, as on the kids' laptop.
- **The login screen.** `build_files/build.sh` disables
  `plasmalogin.service` and enables `sddm.service`, in that order, since
  both claim `display-manager.service`. A preset,
  `40-cairn.preset`, says the same. Without it a first boot's presets
  would try to enable Plasma Login Manager again, from Fedora's
  `85-display-manager.preset`, and log an error.
- **The visit's cage.** `install -m 0711 /usr/bin/cage
  /usr/libexec/cairn/cage-visit`. The copy is made again whenever the
  image is built, so it always matches the base's cage.
- **The polkit action.** Provisioning rewrites it to the helper's real
  path under `/var/usrlocal`. In the image `/usr/libexec` is a real path,
  so the build's copy is already right.
- **Groups.** `/usr/lib/sysusers.d/cairn.conf` makes the four level
  groups and `cairn-guardian` at first boot, with fixed numbers 461 to
  465. Nothing in the base uses them: Bazzite's system groups count down
  from 999 and reach 959, and Fedora's fixed ones stay below 201. The
  level groups get fixed numbers too, so every Cairn machine numbers
  them the same.
- **The Guardian's window.** It must be `root:cairn-guardian`, mode 0750.
  `/usr` is read-only on a booted machine, so the build sets the group
  by its number, 465, read from `sysusers.d`.
- **Folders.** `/usr/lib/tmpfiles.d/cairn.conf` makes `/var/lib/cairn`,
  the shared game folder and its `steamapps` with provisioning's owner,
  mode and ACL, `game-pictures`, and `/var/lib/faillock`. systemd makes
  them at every boot and puts back anything that changed.
- **faillock and SELinux.** `authselect enable-feature with-faillock`,
  and the `semanage fcontext` labels for `/var/lib/faillock` and
  `/run/cairn`. authselect also writes a checksum in
  `/var/lib/authselect`, which an image cannot ship, so the build
  removes it. authselect uses it only to notice its files changed by
  hand, and `authselect check` passes without it.

`/run/cairn` itself is made by the visit helper, as on the kids' laptop.

`dnf5` leaves its lock and `sddm`'s run folder in `/run`, which
`packages.sh` removes. `bootc container lint` then passes with no
warnings, and the build runs it with `--fatal-warnings` so it stays that
way. The image was built and checked on the dev PC on 2026-10-21. The same
day it booted in the VM, switched from a never-provisioned Bazzite with
`bootc switch`. SDDM showed the Cairn login screen. A test child in
`cairn-l1` reached the tiles in labwc, and a test Guardian in
`cairn-guardian` reached Plasma. The groups had their fixed numbers, the
folders in `/var` and their labels were right, the Guardian's window was
`root:cairn-guardian`, and SELinux denied nothing.

When the kids' laptop moves to the image, its groups keep the numbers
`groupadd --system` gave them, because `systemd-sysusers` leaves a group
that exists alone. Its `cairn-guardian` will not be 465, so no Guardian
could open the window. That move must renumber `cairn-guardian` first,
and the files that carry its number with it (slice 4, below).

### What the family does on a computer that runs the image (slice 4)

What is left on a computer that runs the image names a person or is the
family's own choice: the accounts, a "Steam for <child>" menu entry for
each child, `games.json` and the game pictures, each child's copy of the
games, and the Flatpaks. `provision/cairn-family-setup.sh` does these,
run as root from a checkout. It needs no build.

- **A script of its own, made of provisioning's steps.** It loads
  `cairn-provision.sh` for its functions and runs its own short list.
  So both kinds of computer check accounts the same way. A step added to
  provisioning later never runs on an image until it is added to that
  list. `cairn-provision.sh` refuses on a computer that runs the image,
  and the family's setup refuses on one that does not. The image's
  `/usr/lib/sysusers.d/cairn.conf` tells them apart: only the image ships
  it, and `/usr` cannot be written on a stock Bazzite.
- **Nothing new in the image.** The script stays in `provision/`, which
  goes when the first-boot wizard (P1-6) and the Guardian tool (#167)
  arrive. The install-tree test keeps refusing any file that names
  `/usr/local`.
- **The checkout must match the image.** The script reads what the
  image's copy step, picture step and visit helper print, word by word.
  It compares each of them, and kidscan, with the checkout's file, with
  `/usr/local` changed to `/usr` as the build changes it. It refuses on
  any difference and says to check out the commit the image was built
  from.
- **The menu entries go in `/usr/local/share/applications`.** On Bazzite
  that is `/var/usrlocal/share/applications`: the computer's own, and
  writable. A Guardian's Plasma already reads it. In the VM on 2026-10-21
  the Guardian's `plasmashell` had
  `XDG_DATA_DIRS=/home/bazzite/.local/share/flatpak/exports/share:/var/lib/flatpak/exports/share:/usr/local/share:/usr/share`.
  `matchpathcon` gives the folder `usr_t`, so it needs no SELinux rule.
  The image ships nothing there. The menu entries are data the family's
  setup writes on the computer, which is what `/usr/local` is for. A
  `tmpfiles.d` line makes the folder at boot, so it is there before
  Plasma first builds its menu. Each entry names `/usr/bin/cairn-steam-for`.
  That program is `root:cairn-guardian`, mode 0750, so Plasma shows the
  entries only to a Guardian.
  In the VM on 2026-10-21 the entries showed up in the Guardian's menu
  without logging in again.
- **The Flatpaks come last.** They need the internet. An offline computer
  still gets the accounts, entries and tiles, and then the run stops with
  flatpak's own message. A second run online installs them.
- **Groups.** A group that is missing is made with its fixed number, and
  `groupadd` stops if another group has it. `systemd-sysusers` would give
  it some other number without saying so. A `cairn-guardian` with
  another number is refused, with a pointer to the renumbering below.
  The level groups may keep any number: the polkit rules, PAM, the login
  screen and provisioning all read them by name. They are fixed on new
  computers so that every computer installed from the image matches.
  Provisioning by hand now gives a new group its fixed number too,
  unless another group has it.

**Renumbering before the move.** `provision/cairn-renumber-guardians.sh`
gives `cairn-guardian` the number 465. It runs once, on the computer set
up by hand, before `bootc switch`. Nothing on that computer reads the
number, so it keeps working. Each deployment has its own `/etc`, and all
of them share `/var`. Done before the switch, the new number is in the
`/etc` that `bootc switch` carries into the image, and in the one a
rollback would boot. Done after, a rollback would find files at 465 and a
group with the old number.

- It moves the files first, then the group. A file's group is only a
  number, so a file can move before `/etc/group` says so. It walks
  `/etc`, `/var/lib/cairn`, `/var/usrlocal`, `/var/home` and `/var/tmp`,
  each on its own filesystem. It leaves the rest of `/var` alone, because
  the container and Flatpak stores keep numbers of their own.
- ACLs name a group by number, and the shared game folder's default ACL
  puts the Guardian's group on every file Steam makes there. getfacl's
  dump of those places is rewritten and restored with `setfacl
  --restore`.
- After `groupmod` it walks again, so a file made meanwhile moves too, and
  then names any file still on the old number.
  A file that vanishes during a walk does not stop the run, so the check
  at the end is always reached.
  A run that stops after `groupmod` prints the command to run again with
  the old number, which walks the files from it once more.
  Without that number a rerun would see 465 and do nothing.
- It refuses while a Guardian has a desktop open or has Steam open. A
  Guardian who is logged in keeps the old number until they log in
  again. Their Steam would lose the shared game folder in the middle of
  a download, and anything it made meanwhile would keep the old number.
- It runs from a text console or over ssh, and the Guardian's own login
  there is fine. It judges a session by its Type: only `x11`, `wayland`
  or `mir` counts as a desktop. A text console or ssh login is `tty`.
  The first version read logind's `Display`, which names a text session
  when there is no desktop. In the VM it refused the very ssh login
  that ran it, so it could never run as written.
  In the VM on 2026-10-21 the fixed script refused while the Guardian was
  in Plasma. Logged out of it, with a Ctrl+Alt+F3 login open, it ran over
  ssh and moved every file and ACL entry from 945 to 465.
- It refuses when another group has 465. Then the laptop still works as
  it did, and nothing has changed.
- Only `cairn-guardian` moves. The level groups are read by name, so
  walking the children's homes for them would buy nothing.

### Name and logos (slice 5)

The base calls itself Bazzite in `os-release`, in System Settings' About
page, in the boot menu and in its logos. Fedora's guidelines ask a system
built from Fedora to drop `fedora-logos` and show no Fedora artwork.
`build_files/name-and-logos.sh` gives the image Cairn's name and mark.
`build_files/check-name-and-logos.sh` runs in its own step after it, so
a base update that brings back a Bazzite logo or welcome file stops the
build.
Checked on 2026-10-21 in `localhost/cairn:dev`, built from Bazzite
44.20261012, and in bootc-image-builder's source (osbuild/image-builder
at f43ddf4, where its code now lives).
The image booted in the VM the same day.
Everything below held there except the hostname and the boot menu
titles, which stay Bazzite's until #165; "What does not change on the
screen yet" says why.

**`/usr/lib/os-release`** is written whole:

| Key | Value | Why |
|---|---|---|
| `NAME`, `PRETTY_NAME` | `Cairn Linux` | The boot menu entry on a fresh install or after a kernel change, System Settings' heading, `hostnamectl` and the installer's title |
| `ID` | `cairn` | See below |
| `ID_LIKE` | `fedora` | bootc-image-builder and Steam fall back on it |
| `VERSION_ID`, `VERSION` | the base's, `44` | Must equal the Fedora release; the build stops if it does not equal `rpm -E %fedora` |
| `ANSI_COLOR` | Sky, from `semantic.console` in `brand/tokens.json` | The name on the boot console and in the Guardian's prompt; Ink would vanish on black |
| `LOGO` | `cairn-logo-on-ink` | Drawn by System Settings' About page and kinfocenter in the Guardian's Vapor window colours, a dark grey; Sand measures 12.9:1 on it and Ink 1.2:1 |
| `DEFAULT_HOSTNAME` | `cairn` | Only `hostnamectl` reports it today; the name in use stays `bazzite` until #165 rebuilds the initramfs (below) |
| `HOME_URL` | `https://cairnlinux.com/` | A link a person clicks; nothing fetches it |
| `BUG_REPORT_URL` | Cairn's issues | Own support channels only |
| `SUPPORT_END` | the base's | Fedora 44's real end date |
| `OSTREE_VERSION` | the base's | Bazzite's `custom-resolution-helper` exits when it is empty |
| `IMAGE_ID` | `cairn` | `hostnamectl` shows it as "OS Image" |

Dropped: `VARIANT`, `VARIANT_ID` and `VERSION_CODENAME`, which said
Kinoite or bazzite; `DOCUMENTATION_URL` and `SUPPORT_URL`, which pointed
at Bazzite's docs and Discord; `BUILD_ID` and `BOOTLOADER_NAME`, which a
search of `/usr` and `/etc` found no reader for; and `CPE_NAME`.
Cairn has no registered CPE vendor, and an invented one helps no scanner.
`/usr/lib/system-release-cpe` keeps `cpe:/o:fedoraproject:fedora:44`,
which is true of the packages. `REDHAT_*` keys were already absent.

**Why `ID=cairn` is safe.** Every tool that reads the ID treats an
unknown one the same way, so `cairn` behaves exactly as `bazzite` did.

- bootc-image-builder picks its distro definition in
  `pkg/distro/defs/bootc_imagetype.go`, `NewDistroYAMLFrom`. It tries
  `ID-VERSION_ID`, then each `ID_LIKE` entry with the same `VERSION_ID`.
  `bazzite-44` has no definition either, so both fall back to
  `fedora-44`, and no `--distro` is needed. `GetDistroAndRunner` gives an
  unknown ID the same null distro and the same ISO label. The source has
  no "bazzite" anywhere. The UEFI vendor comes from the shim's folder,
  not from `os-release`.
- Bazzite's and Universal Blue's scripts read
  `/usr/share/ublue-os/image-info.json`, not `os-release`. Several rerun
  their first-boot work when its `image-name` changes, so that file stays
  as it is. Bazzite's extras are P1-1.
- dnf takes `$releasever` from `fedora-release`'s `system-release(44)`.
  rpm-ostree's package layering on a running computer takes it from
  `VERSION_ID`. Both stay 44.
- `bootc` and `uupd` read nothing from `os-release` but text to show.
- The only Cairn reader is provisioning's log, which records
  `PRETTY_NAME` and `IMAGE_ID`.

**`/usr/lib/fedora-release`** says `Cairn Linux release 44`. bootupd names
a new install's firmware boot entry from the words before "release".
`/etc/system-release` and `/etc/redhat-release` link to it. The
`fedora-release` packages stay installed, because dnf's `$releasever` and
many packages depend on them; Bazzite already rewrites this file.

Both files are written to a temporary file and copied into place with
`install -m 0644`. That gives them a known mode however the storage
driver shares the old file. `mktemp`'s own 0600 would hide the system's
name from everyone but root. The check asserts mode 644.

**What does not change on the screen yet.** libostree takes a boot menu
entry's title from `PRETTY_NAME`, but it writes the entries again only
when the kernel, the initramfs or the kernel arguments change.
Cairn reuses Bazzite's kernel and initramfs as they are.
So a computer that updates from Bazzite keeps "Bazzite (ostree:N)"
titles until the base kernel changes or #165 rebuilds the initramfs.
The VM showed this: after two upgrades to slice 5 images, both entries
still read "Bazzite".
A fresh install is expected to write "Cairn Linux (ostree:0)", and
slice 6 checks that.

The hostname does not change yet either.
On a computer with an empty `/etc/hostname`, like the VM, systemd picks
the name in the initramfs, from that image's own `initrd-release`.
That file is still Bazzite's and says `ID=bazzite`, and systemd keeps
the name after it hands over to the real root.
So `hostnamectl` reports the default as `cairn`, but the name in use is
`bazzite`, with source "default".
The prompt, DHCP, mDNS (`bazzite.local`), KDE Connect and Steam Remote
Play keep showing `bazzite`.
A fresh install from the slice 6 ISO uses the same initramfs, so it is
called `bazzite` too, unless the installer writes `/etc/hostname`.
The image does not ship `/etc/hostname`, because every computer would
then get the same fixed name, and the name is the family's choice.
A `systemd.hostname=` kernel argument is not used either, because it
would override a name the family sets.

**`fedora-logos` becomes `generic-logos`.** `packages.sh` runs
`dnf5 swap fedora-logos generic-logos`. generic-logos is Fedora's own
package for this. It provides `system-logos`, which Breeze, oxygen,
Plymouth's graphics library, KDE's settings and SDDM require, so
`rpm -Va --nofiles` finds nothing broken. It stays registered in rpm, and
no Cairn step writes its files, so `rpm -V generic-logos` stays clean.
Bluefin erases generic-logos from rpm's database after the swap; here
that would leave five packages missing `system-logos`. A `cairn-logos`
package can replace it with P1-2. generic-logos' art is Fedora's
placeholder: a hot dog in `fedora-logo.png`, a plain `start-here` icon.
Nothing Cairn shows names those files now, and the installer is slice 6.
The swap leaves Bazzite's changed `/etc/favicon.png` as `.rpmsave`,
which is removed. `fedora-release-notes` is not in the base, and the
check keeps it out.

**The mark.** `brand/build.py` makes `brand/image/cairn-logo.svg` (Ink)
and `cairn-logo-on-ink.svg` (Sand). They are square, `0 -11 120 120`,
computed from the stones with one base-stone height of clear space.
Qt's icon loader stretches an SVG into the square it is asked for, and a
box that is not square would change the mark's shape. Both go in hicolor's
`scalable/places`. Bazzite's place names, which no package owns, lead to
the Sand mark, and so does `os-release`'s `LOGO`, because the panel, the
splash and the About page are all dark today:

- `distributor-logo.svg` and `distributor-logo-white.svg` become links to
  `cairn-logo-on-ink.svg`.
- `distributor-logo-steamdeck.svg`, `bazzite-logo.svg` and
  `bazzite-logo-white.svg` already link to those two, and keep their
  names. The first is the Guardian's start-menu button, which Valve's
  Vapor panel setup names. The second is the icon of Bazzite's Big
  Picture dialog. A Guardian's own settings are not touched.
- `bazzite-logo-le.svg` and the eight `bazzite-logo-icon.png`, which
  nothing names, are removed.
- The splash of the Vapor and VGUI looks loads
  `images/bazzite_logo.svgz` on black. Bazzite's `Splash.qml` names the
  file, so the Sand mark goes in under that name.

Once deployed, every file in the image has the same time, the icon
cache included, so nothing would see that the icons changed. The build runs
`gtk-update-icon-cache` so the cache lists the new names. Plasma also
keeps a cache of icons it has drawn in each account, which survived
every image switch in the VM. The build writes a fingerprint of every
icon file and link to `/usr/share/cairn/icons.sum`. A short script in
`/etc/xdg/plasma-workspace/env/` removes the account's
`icon-cache.kcache` at login when the fingerprint differs from the one it
saw last.

At 120 units a side the stones are 82 tall, so the mark keeps the brand's
16 px minimum only in an icon of 24 px or more.

**System Settings' About page.** Bazzite changed KDE's
`/etc/xdg/kcm-about-distrorc` to name Bazzite, its website and a logo.
`system_files/` replaces it with an empty `[General]`, so the page takes
the name, logo and website from `os-release`. It is a config file the VM
never changed, so bootc's merge of `/etc` takes the new one.

**Bazzite's welcome and support channels.** Files no package owns are
removed: the banner `/usr/libexec/ublue-motd` prints in every terminal
(Bazzite's issues, docs, Discord and Bluesky), the fastfetch aliases with
Bazzite's logo, the autostart that turns Bazzite's announcements into
notifications, the "Documentation" menu entry that opens Bazzite's docs,
and the "Bazzite Portal" autostart every new account gets from
`/etc/skel`. With the banner gone, Universal Blue's `user-motd.sh` prints
`/etc/user-motd`, which does not exist. A plain `fastfetch` now picks its
own logo from `ID_LIKE`.

**Labels.** The image's title, description, URLs, vendor and licence
name Cairn; the revision, creation date and Artifact Hub labels are
emptied, since they named Bazzite's commit, date and logo. The version
label stays the base's build, which `bootc status` shows, until P1-3
gives Cairn versions of its own. `containers.bootc` and the `ostree.*`
labels stay, because bootc reads them.

**Licence texts.** `LICENSE`, `LICENSE-CC-BY-SA-4.0`, `NOTICE` and
`TRADEMARKS.md` go in `/usr/share/licenses/cairn/`, so the mark travels
with its licence and trademark notice. The two image SVGs carry an SPDX
line too.

**Left on purpose, and where each goes:**

- **#165, the boot screen and the hostname.** The initramfs is built in
  the base. It still holds Bazzite's `initrd-release` (`ID=bazzite`), its
  Plymouth watermark and `system-logo-white.png`, so the boot screen is
  unchanged. The shutdown and reboot screen loads from the root
  filesystem, where the watermark went with `fedora-logos`, so it now
  shows none. #165 rebuilds the initramfs, and both match again.
  The rebuilt initramfs carries Cairn's `os-release`, so the hostname
  becomes `cairn` on every computer with an empty `/etc/hostname`.
  That renames computers already installed, and the new name goes out in
  DHCP and mDNS (`cairn.local`) and shows in KDE Connect and Steam Remote
  Play. Nothing pairs by name, so nothing breaks, but #165 checks it in
  the VM: `hostnamectl` should then report `cairn` with source "default".
  The new kernel image set also makes libostree write new boot entries,
  so upgraded computers get "Cairn Linux" titles then.
- **#166, the Guardian's desktop.** The start-menu button and the splash
  now show the Sand mark, chosen because Vapor's panel and the splash are
  dark. `LOGO` names it too, because the About page is drawn in Vapor's
  dark window colours. The panel and the windows have colour schemes of
  their own, so #166 decides each one. A light panel points the three
  Bazzite links at `cairn-logo.svg`, and light windows point `LOGO` at
  `cairn-logo`. An About page opened without the session's settings,
  such as `kinfocenter` started over ssh without
  `XDG_CURRENT_DESKTOP=KDE`, falls back to light colours, where the Sand
  mark nearly vanishes; a Guardian never opens it that way.
  The splash ground is Splash.qml's black, not Ink, and #166 owns
  Splash.qml. Valve's previews and `deck_icon.png` are Valve's and stay.
  A Guardian who resets the panel or picks a Fedora look gets
  generic-logos' plain `start-here` icon, not the mark; overwriting it
  waits for `cairn-logos`. `mark-currentcolor.svg` is not used, because
  Plasma recolours only its own colour classes, not `currentColor`.
- **#167, the Guardian tool.** Fedora's guidelines also ask for a notice
  where the system names itself: not provided or supported by the Fedora
  Project, official Fedora at https://fedoraproject.org/. KDE's About page
  has only its `Variant` line, which can hold no link and no
  translation of Cairn's own. The notice goes on the Guardian tool's About
  page. Until then this one condition is not met, and P1-14 blocks
  publishing on it.
- **P1-1, Bazzite's extras.** Still in menus a Guardian sees: "Bazzite
  Updater" and "Bazzite Portal", from Bazzite's own packages, and a pin
  of the portal that `bazzite-pins.js` adds to an empty task bar.
  Bazzite's documentation, wallpapers, videos and `ujust` recipes stay
  too. The check allows exactly these Bazzite file names and no others.
- **Slice 6, the ISO.** bootc-image-builder's Fedora definition installs
  `fedora-logos` in the installer and uses it for its artwork, so the
  installer shows Fedora's whatever the image ships. bootupd names the
  firmware entry from `/usr/lib/fedora-release`, but the shim's
  `BOOTX64.CSV` says "Fedora", and the firmware's fallback recreates a
  lost entry with that name. Slice 6 checks both with `efibootmgr`, and
  runs one pinned bootc-image-builder whose `manifest` step proves the
  `ID_LIKE` fallback. Next to that it checks that the boot menu titles
  read "Cairn Linux" after an ISO install. If a fresh install should be
  called `cairn` before #165 lands, that is an installer setting in
  slice 6 that writes `/etc/hostname`, never a file in the image.

### What stays out of the image

- **Anything that names a person.** Accounts, `games.json`, a child's
  "Steam for <child>" menu entry, Flatpaks, and each child's copy of the
  games stay a step for the family, never in the image. That step is
  `provision/cairn-family-setup.sh` (slice 4).
- **The boot screen and the Guardian's desktop.** Plymouth and the
  initramfs are #165. The Guardian's wallpaper, colours, splash and lock
  screen are #166. Slice 5, above, lists what each inherits from it.
- **A first account.** The ISO ships with no user, and the installer does
  not offer one. The first account comes from the first-boot wizard
  (ADR-0020, P1-6). Until it exists, a test ISO for the VM adds a test
  Guardian through a `bootc-image-builder` config kept on the dev PC. That
  config is never committed or shipped.
  The image's groups only exist after `systemd-sysusers` runs at first
  boot. So any config that adds an account at install time must first
  create `cairn-guardian` itself, with number 465. It does the same for
  any level group it uses (`cairn-l1` to `cairn-l4`, 461 to 464).
  Otherwise the group is missing, or it gets some other number that
  `systemd-sysusers` then leaves alone, and the Guardian cannot open
  "Steam for <child>". In a `bootc-image-builder` config that is
  `[[customizations.group]]` with `name = "cairn-guardian"` and
  `gid = 465`. In a kickstart it is `group --name=cairn-guardian
  --gid=465`. The other way is to add the test Guardian only after first
  boot, once the groups exist.
- **Bazzite's extras.** The first-boot portal's menu entry, the updater,
  Game Mode and Waydroid stay until P1-1.
  So does Bazzite's first-login step for every account, a child's included.
  It runs as root through `pkexec`, turns on network time, and makes the
  account a Tailscale operator. Tailscale is not signed in, so that does
  nothing yet, but it is on the P1-1 list.

The image stays private and inside the family until P1-14, #86 and P1-17
pass (ADR-0031).

## Consequences

- `cmake --install` makes the whole tree, and a test installs it into a
  temporary folder and checks every file, its mode, and that none is
  under `/usr/local` or `/var` or names `/usr/local`.
- Provisioning on the kids' laptop installs what it did before. Its
  release build still names `/usr/local`, and it now refuses a build that
  was not configured for `/usr/local`.
- Two copies of the wrappers now exist, provisioning's and the build's.
  On a computer that runs the image, only the family's setup runs, and
  provisioning's copies are not made. The two copies stay until the kids'
  laptop has moved; `provision/README.md` lists what that move removes.
- A build configured for one prefix and installed to another names the
  wrong paths. The install-tree test runs only in a build configured for
  `/usr`, and says it was skipped otherwise.
- The tree `cmake --install` makes has the Guardian's window and wrapper
  with mode 0750 and the group `root`. Only the image gives them
  `cairn-guardian`, so a tree installed some other way needs that step
  too.
- The level and Guardian groups have the same numbers on every machine
  installed from the image. A machine that made them by hand renumbers
  `cairn-guardian` before it moves to the image; its level groups may
  keep their numbers.
- The family's setup runs from a checkout of the commit the image was
  built from. A checkout-free way waits for the first-boot wizard.
- The image calls itself Cairn Linux, with `ID=cairn`, and still reports
  Fedora's release as its version. Cairn's own version numbers wait for
  P1-3.
- A base update that adds a Bazzite logo, menu entry, autostart or
  terminal file stops the build at the slice-5 check, until it is removed
  or named there.
- Compiling in the image's build stage makes every image build take a few
  minutes longer. RPMs (P1-2) would move that work out of the image build.
