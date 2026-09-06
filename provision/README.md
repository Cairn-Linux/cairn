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

`CAIRN_GUARDIAN_PASSWORD` in the environment sets the Guardian's password
when the account is created; otherwise the script says to run `passwd`.
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
| `/usr/local/share/cairn/labwc/` | The kiosk configuration from `../session/labwc/` |
| `/usr/local/bin/cairn-session`, `/usr/local/share/cairn/sessions/cairn.desktop` | The one session entry and its dispatcher (ADR-0012) |
| sddm, sddm-breeze, sddm-wayland-plasma | Layered; `plasmalogin.service` disabled and `sddm.service` enabled (ADR-0016). `desktop-backgrounds-compat` is layered first from a downloaded RPM with `--force-replacefiles`, because the image carries its two wallpaper paths as symlinks no package owns |
| `/etc/sddm.conf.d/`, `/etc/pam.d/sddm` | Only Cairn's session directory offered, Guardians hidden (generated from `cairn-guardian`), no autologin, and the greeter-only passwordless rule for L1 and L2 |
| `/usr/local/bin/cairn-launcher` | Wrapper that runs the release launcher from `/usr/local/libexec/cairn` with the manifest in `/usr/local/share/cairn/manifest.json` |

Not yet: an avatar for the child, the Cairn greeter theme (P1-16), and the
polkit rule for the level groups that the P0-4 run showed is needed
(`../docs/research/kiosk-containment.md`, #35).

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
