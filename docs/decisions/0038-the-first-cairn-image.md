# ADR-0038: The first Cairn image

**Status:** proposed. It becomes accepted when the image boots in the VM
to the Cairn login screen, a child reaches the tiles, and a Guardian
reaches Plasma.
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
The image copies that tree. Nothing goes in `/usr/local`, and nothing that
names a person goes in the image.**

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

Some steps are not files the build makes. The Containerfile does them:

- **Layered packages.** labwc, ScummVM, SDDM and the brand's fonts, as
  provisioning layers them, with `sddm.service` enabled and
  `plasmalogin.service` disabled.
- **The visit's cage.** `install -m 0711 /usr/bin/cage
  /usr/libexec/cairn/cage-visit`. The copy is made again whenever the
  image is built, so it always matches the base's cage.
- **The polkit action.** Provisioning rewrites it to the helper's real
  path under `/var/usrlocal`. In the image `/usr/libexec` is a real path,
  so the build's copy is already right.
- **Groups and folders.** The level groups come from
  `/usr/lib/sysusers.d/cairn.conf`, and `/var/lib/cairn` and its folders
  from `/usr/lib/tmpfiles.d/cairn.conf`.
- **The Guardian's window.** It must be `root:cairn-guardian`, mode 0750.
  The group does not exist when the build installs it, so the tree has
  the mode only. `/usr` is read-only on a booted machine, so tmpfiles
  cannot set the group there. The image sets it at build time, which
  needs a fixed group number in `sysusers.d`.
- **faillock and SELinux.** `authselect enable-feature with-faillock`,
  and the `semanage fcontext` labels for `/var/lib/faillock` and
  `/run/cairn`.

### What stays out of the image

- **Anything that names a person.** Accounts, `games.json`, a child's
  "Steam for <child>" menu entry, Flatpaks, and each child's copy of the
  games stay a later step for the family, never in the image.
- **Name and logos.** The `os-release` rewrite, swapping
  `generic-logos`, Bazzite's 28 logo files including
  `distributor-logo-steamdeck.svg`, and the attribution in `NOTICE` are
  slice 5. The boot screen is #165.
- **A first account.** The ISO ships with no user, and the installer does
  not offer one. The first account comes from the first-boot wizard
  (ADR-0020, P1-6). Until it exists, a test ISO for the VM adds a test
  Guardian through a `bootc-image-builder` config kept on the dev PC. That
  config is never committed or shipped.
- **Bazzite's extras.** The first-boot portal, Game Mode and Waydroid stay
  until P1-1.

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
  Once a machine runs the image, provisioning shrinks to the family's own
  steps (slice 4), and its copies of the files go.
- A build configured for one prefix and installed to another names the
  wrong paths. The install-tree test runs only in a build configured for
  `/usr`, and says it was skipped otherwise.
- The tree has the Guardian's window and wrapper with mode 0750 and the
  group `root` until slice 3. Until then only root could run them.
- Compiling in the image's build stage makes every image build take a few
  minutes longer. RPMs (P1-2) would move that work out of the image build.
