# provision

Phase 0 only. A script that turns a stock Bazzite KDE install (ADR-0006)
into a Cairn machine, so the experience can be tested on real hardware and
with a real child **before** any image, ISO or CI exists (DESIGN §13 Phase 0).

`cairn-provision.sh` is that script (P0-2, first run in the VM on
2026-09-06). From a checkout with a release build, on the target machine:

```sh
cmake --preset release && cmake --build --preset release   # on the dev PC
rsync -a --exclude build/debug --exclude .git . cairn-min:cairn/
ssh cairn-min                                                # then, in the VM
cd cairn && sudo ./provision/cairn-provision.sh --guardian guardian --child ada
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
| the child account | In `cairn-l1`, password locked: the greeter lets an L1 child in, `su` and `ssh` do not (#35) |
| labwc, ScummVM | Layered into the OS; Tux Paint and GCompris as system Flatpaks |
| `atkinson-hyperlegible-next-fonts`, `atkinson-hyperlegible-mono-fonts` | Layered: the brand's typefaces for the login screen, the launcher and the Terminal (DESIGN §6.2), which the Bazzite image does not carry |
| `/usr/local/share/cairn/labwc/` | The kiosk configuration from `../session/labwc/` |
| `/usr/local/bin/cairn-session`, `/usr/local/share/cairn/sessions/cairn.desktop` | The one session entry and its dispatcher (ADR-0012) |
| `/usr/local/bin/cairn-give-up`, `/etc/systemd/logind.conf.d/10-cairn-power.conf` | The grown-up's way out of a stuck program and the power button (ADR-0018, #41) |
| `/usr/local/bin/cairn-log-out` | The child's Log out, which the launcher runs (ADR-0021, #44) |
| sddm, sddm-breeze, sddm-wayland-plasma | Layered; `plasmalogin.service` disabled and `sddm.service` enabled (ADR-0016). `desktop-backgrounds-compat` is layered first from a downloaded RPM with `--force-replacefiles`, because the image carries its two wallpaper paths as symlinks no package owns |
| `/etc/polkit-1/rules.d/10-cairn-levels.rules` | What a child's session may ask the system to do, keyed off the level groups (`../session/polkit/`, #35) |
| `/etc/sddm.conf.d/`, `/etc/pam.d/sddm` | Only Cairn's session directory offered, the Cairn theme, no autologin, and the greeter-only passwordless rule for L1 and L2. A `20-cairn-guardians.conf` from an earlier run, which hid the Guardians, is removed (ADR-0020) |
| `/usr/local/share/cairn/sddm/themes/cairn/`, `/usr/local/lib64/cairn/qml/` | The login screen from `../greeter/theme/`, and the `Cairn.Brand` and `Cairn.Greeter` QML modules it imports (ADR-0020) |
| `/etc/security/faillock.conf`, `/var/lib/faillock` | A lockout after wrong passwords for every account that has one: `authselect`'s `with-faillock`, with its count kept where a reboot does not clear it, and an SELinux rule labelling that directory `faillog_t` (ADR-0020) |
| `/usr/local/bin/cairn-launcher` | Wrapper that runs the release launcher from `/usr/local/libexec/cairn` with the manifest in `/usr/local/share/cairn/manifest.json` and `--log-out /usr/local/bin/cairn-log-out`, which gives the tiles a Log out button. Its Draw tile runs Tux Paint with `--autosave --saveovernew`, so a child who leaves with Super+Q never loses a picture (ADR-0019) |

Not yet: a picture for the child's tile (ADR-0020 leaves it for later) and
a `logind.conf` key for the lid. Re-running the script puts back the
repository's `manifest.json`, so a tile added by hand in the VM goes.

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
