# Kiosk containment: what a child can and cannot reach from the L1 session

**Date:** 2026-09-06
**For:** ROADMAP P0-4 (issue #3) and issue #41
**Result:** Every row that can run nested on the dev PC passes, two of them
only after changes to `session/labwc/` made in the same change. The three
rows that needed the VM ran there on 2026-09-08 during P0-10: the
shortcut and VT rows pass at the real console; `pkcheck` as the child
said polkit lets a child power off, mount and change Wi-Fi, and the rule
in `session/polkit/` (#35) closed that the same day, checked from inside
the child's session. The hung or trapped app row (#41) now passes too:
Ctrl-Alt-Home runs `cairn-give-up` (ADR-0018). On 2026-09-29 a row for
the child's own way out was added after the first child pilot (#77):
Super+Q closes the app in front (ADR-0019), checked in the VM.

## How it was run

labwc 0.9.6 nested inside the Plasma session on the dev PC, started as the
session dispatcher will start it, with the launcher as the session client:

```sh
labwc -d -C session/labwc -S './build/debug/launcher/cairn-launcher \
  --manifest launcher/manifests/dev-pc.json'
```

The nested output is 1280 by 720; the launcher's window rule made it
fullscreen on map. Test windows were opened from a second terminal with
`WAYLAND_DISPLAY=wayland-1` and `DISPLAY=:1` (labwc starts its own XWayland
at once). Focus was read from XWayland's side with `xdotool getwindowfocus`
and the root window's `_NET_ACTIVE_WINDOW`, stacking from `grim` screenshots
of the nested output, and the compositor's own decisions from its debug log.
No key or pointer events were injected in this run: no virtual-keyboard tool
(`wtype`, `wlrctl`) was installed yet, and under Plasma a real Alt-Tab or
Super never reaches a nested compositor because KWin takes it first. The
leave-key rows (#77) were later run headless with `wtype`; see "Leaving an
app".

Where a row depends on how labwc is written, the 0.9.6 sources were read
(`src/input/keyboard.c`, `src/xwayland.c`, `src/config/rcxml.c`,
`src/desktop.c`).

## Rows

| Row | Result | What happened |
|---|---|---|
| A launched window appears on top; closing it returns to the launcher | **Pass** | Tux Paint at `--fullscreen=native` mapped as a fullscreen X11 window over the launcher and took focus. When it ended, focus returned to the launcher and the tiles were up. See "Tux Paint" below for the manifest bug this found. |
| Alt-Tab, Super and other compositor shortcuts unreachable | **Pass** (by configuration, then by keypress in the VM, 2026-09-08) | The debug log shows no "load default key bindings" line, so the one no-op keybind did its job. At the VM's console Alt-Tab reached the launcher as a plain Tab and moved its focus ring one tile; Super and Alt-F4 did nothing, no switcher or menu appeared, and the launcher and compositor were still running. See "Default bindings" for what keeps them out. Since ADR-0019 one Super combination is bound on purpose, Super+Q (row below); Super on its own still does nothing. |
| Ctrl-Alt-Fn VT switching unreachable | **Pass** (VM, 2026-09-08) | labwc switches VTs in code, before any keybind, with no rc.xml option to stop it. The keymap option `srvrkeys:none`, set in `session/labwc/environment`, removes the keysyms it looks for. The keymap labwc handed its clients had 24 `XF86Switch_VT` entries before and none after. At the VM's console, Ctrl-Alt-F2 and Ctrl-Alt-F1 sent through the virtual keyboard left `/sys/class/tty/tty0/active` on the kiosk's `tty3`. |
| A focus-stealing X11 client cannot take focus from the launcher | **Fail, then pass** | A raw X focus change (`XSetInputFocus`) is reverted by wlroots and never reached the launcher. An activation request (`_NET_ACTIVE_WINDOW`) was honoured: labwc un-minimised the X window and gave it focus over the launcher. With `ignoreFocusRequest="yes"` on every window the request is logged and ignored. |
| Two X11 clients sharing one XWayland (#35) | **Same as above** | Between two xterms a raw focus change was reverted and an activation request switched focus. The same rule stops it. |
| `pkcheck` as the child for power-off, mount, network and package actions (#35) | **Fail, then pass** (VM, 2026-09-08) | With the L1 account's labwc as the subject, polkit says yes to power-off, reboot, suspend, hibernate, udisks2 mount and eject, NetworkManager enable-disable-wifi, network-control and modify-own-connections, Flatpak app-update and runtime-install, and rpm-ostree upgrade; it wants an admin password only for modify-system, Flatpak app-install, set-user-linger and rpm-ostree install. No Cairn rule existed in `/etc/polkit-1/rules.d/`. With `10-cairn-levels.rules` installed, every one of those says no and reads of the child's own parental-control settings still say yes; the table is under "The rule" below. |
| A hung or trapped client can be exited without a reboot (#41) | **Fail, then pass** (VM, 2026-09-08) | With no bindings there was no way out but the app's own quit. Ctrl-Alt-Home now runs `cairn-give-up` (ADR-0018), which ends the app however frozen; the matrix is under "Hung or trapped apps" below. |
| A child can leave any app on their own (#77) | **Fail, then pass** (VM, 2026-09-29) | In the first child pilot (#8) a six-year-old needed help to leave every app. Super+Q now asks the app in front to close (ADR-0019), and in the launcher it is one step back that never ends the session; the matrix is under "Leaving an app" below. A frozen app still needs Ctrl-Alt-Home. |
| A child can end their turn and leave nothing running (#44) | **Fail, then pass** (VM, 2026-09-30) | An L1 child had no way back to the login screen, and quitting the launcher would have left Flatpak apps and the Steam client running. A Log out button above the tiles now ends the child's whole account (ADR-0021); the matrix is under "Logging out" below. |
| X11 windows carry no server decorations | **Fail, then pass** (new row) | An xterm got a titlebar with minimise, maximise and close buttons: `<decoration>client</decoration>` only governs Wayland clients. `serverDecoration="no"` on every window removes it. |

The launcher behaved as designed throughout: each window opened from the
side terminal produced "Something needs a grown-up" naming the app id, and
the tiles came back when the window closed.

## Findings

### Tux Paint

`launcher/manifests/dev-pc.json` ran Tux Paint as `tuxpaint --fullscreen`.
Tux Paint 0.9.35 refuses that: "Command line option '--fullscreen' needs a
value", exit 52. So the Draw tile on the dev PC has been showing the
grown-up screen, which is the right failure but the wrong reason. The
manifest now says `--fullscreen=native`, which fills the output at its own
size. `kidscan` emits `--fullscreen` for ScummVM, where it is correct.

Tux Paint runs under XWayland (SDL picks X11 here), so the X11 rows are the
ones that apply to it.

### Default bindings

labwc loads its default key and mouse bindings when the config defines none
of that kind. At the first run `rc.xml` had one keybind and one mousebind,
both with the action `None`, and labwc 0.9.6 drops those only after checking
whether the list is empty (`post_processing` before
`deduplicate_key_bindings` in `rcxml.c`), so that order was all that kept
the defaults out. The keyboard no longer depends on it: Ctrl-Alt-Home
(ADR-0018) and Super+Q (ADR-0019) are real bindings that labwc never drops,
and the `None` one for Alt-Tab stays only as a fallback. The mouse still
does: its one binding is `None`, and a labwc release that swapped the order
would silently load the root menu, the window menu and Super-drag. The
debug lines to watch for on any labwc upgrade are `load default mouse
bindings` and `load default key bindings`; their absence is the pass.

To confirm by keypress, run the nested command above in the VM or on the
laptop, open two windows and press Alt-Tab, Alt-F4, Super-Return, Super-A,
Alt-Space and Ctrl-Alt-F2. Nothing should happen.

### VT switching

`keyboard.c` in labwc 0.9.6 handles `XF86Switch_VT_1` to `_12` before
keybinds and before the lock check, and calls `wlr_session_change_vt`. There
is no configuration for it. The keysyms come from the keymap, where the
`pc` symbols include `srvr_ctrl(fkey2vt)`; the XKB option `srvrkeys:none`
replaces that with plain F keys. labwc reads `XKB_DEFAULT_OPTIONS` from the
`environment` file beside `rc.xml`, so the option ships with the kiosk
configuration and applies to every keyboard labwc creates. Checked by
dumping XWayland's keymap with `xkbcomp -xkb :1` under nested labwc with and
without the file.

A Guardian who needs a console reaches it from their own Plasma session, not
from a child's.

### Activation requests

labwc honours an X11 `_NET_ACTIVE_WINDOW` request and a Wayland
`xdg-activation` request unless the window rule property
`ignoreFocusRequest` says otherwise (`xwayland.c` and `xdg.c`). A window that
maps normally still receives focus, so a launched app comes up focused as
before; only requests from already-open windows are refused. The rule is on
`identifier="*"`, placed before the launcher's own rule so that later, more
specific rules still win.

### polkit defaults on Fedora 44

`pkaction --verbose` for the actions the row names, run on the dev PC:

| Action | Active local user, no password |
|---|---|
| `org.freedesktop.login1.power-off`, `reboot`, `suspend`, `hibernate` | yes |
| `org.freedesktop.udisks2.filesystem-mount` (removable) | yes |
| `org.freedesktop.NetworkManager.network-control` | yes |
| `org.freedesktop.NetworkManager.enable-disable-wifi` | yes |
| `org.freedesktop.udisks2.filesystem-mount-system` | admin password |
| `org.freedesktop.NetworkManager.settings.modify.system` | admin password |
| `org.freedesktop.packagekit.package-install` | admin password |
| `org.freedesktop.Flatpak.app-install`, `app-uninstall` | admin password (wheel: yes) |

So a child at the console can power the machine off, suspend it, mount a USB
stick and switch Wi-Fi networks with nothing in the way. None of these is a
launcher matter; P0-2 needs a polkit rule under `/etc/polkit-1/rules.d/`
that returns `NO` for the `login1`, `udisks2` and `NetworkManager` actions
when the subject is in `cairn-l1` or `cairn-l2`, and the `pkcheck` row runs
in the VM once that account exists. Power-off from the launcher itself, if
it ever exists, would go through the same rule.

### The rule (#35)

`session/polkit/rules.d/10-cairn-levels.rules`, installed by provisioning
to `/etc/polkit-1/rules.d/`, keyed off the level groups. It only ever
returns NO or steps aside. At every child level: NetworkManager, Flatpak,
PackageKit, rpm-ostree, and changes to malcontent's parental controls,
which DESIGN §3.2 gives the Guardian. At L1 and L2 as well: power-off,
reboot, halt, suspend, hibernate and everything under udisks2. Reading
one's own parental-control settings stays allowed because malcontent's
Flatpak check runs as the child and needs it. Fedora's `50-default.rules`
still names `wheel` as the admin identity, and `polkitd` reads a new file
in the directory without a restart.

Checked on 2026-09-08 in the VM (polkit 127, Bazzite `44.20260902`) with
`pkcheck` run as `ada` from a process placed in her own session scope, and
then by asking logind, which is what a real caller gets:

| Action | Before | After |
|---|---|---|
| `login1.power-off`, `reboot`, `suspend`, `hibernate` | yes | no; logind `CanPowerOff`, `CanReboot`, `CanSuspend` answer "no" |
| `login1.inhibit-block-idle` | yes | yes (untouched; games ask for it) |
| `udisks2.filesystem-mount`, `eject-media` | yes | no |
| `NetworkManager.enable-disable-wifi`, `network-control`, `settings.modify.own`, `wifi.scan` | yes | no |
| `Flatpak.app-update`, `runtime-install` | yes | no |
| `Flatpak.app-install`, `ParentalControls.AppFilter.ChangeOwn`, `Malcontent.SessionLimits.Extend`, `rpmostree1.upgrade` | password or yes | no |
| `ParentalControls.AppFilter.ReadOwn`, `accounts.change-own-user-data` | yes | yes (untouched) |
| The `bazzite` account's ssh shell (wheel, no level group), `login1.power-off` | admin password | admin password (untouched) |

Two things the run taught. First, a prefix on `login1.reboot` was not
enough: `pkcheck` kept answering yes for `reboot` alone while every other
power action said no. polkit honours the `org.freedesktop.policykit.imply`
annotation, and logind's policy says that being allowed
`set-reboot-parameter`, `set-reboot-to-firmware-setup` or the two
`set-reboot-to-boot-loader-*` actions, all "yes" for an active user,
implies being allowed `reboot`. The rule now names `set-reboot-` too. The
symptom to recognise: `pkcheck` prints `polkit.result=no` and still exits
0, because the detail comes from the rule and the verdict from the imply.
Second, `polkitd` logged "Error loading script" for this file on three
of about twenty writes, with `install(1)` in place and with a rename
alike, and the rules answered correctly every time afterwards. It watches
the directory and reloads on every event, so a read can land before a
write is complete; the retry on the next event is what makes it right.
Provisioning now writes every file beside its target and renames it,
which shortens the window but did not remove the line entirely. The
file never changes while a child is logged in, so the line is a
provisioning-time curiosity, not a hole.

### Hung or trapped apps (#41), and the way out (ADR-0018)

Nothing in the session let a child leave a window that would not close on
its own. Tux Paint's quit is a button and a dialog; ScummVM's is a key; a
frozen window has none. The fix is a grown-up's key combination,
Ctrl-Alt-Home, that runs `cairn-give-up` (`session/bin/`). Not a single key
or a gesture: the stuck child fetches an adult and the adult presses it.

The helper does not go through the launcher, which for a Flatpak or Steam
app is already showing its tiles behind the fullscreen window: the app has
reparented into its own session (a Flatpak tree under `bwrap`, a Steam game
under Steam's `reaper`) and the launcher's own child exited seconds after
the launch. So the helper signals those sandbox roots in the child's session
directly, SIGCONT first so a stopped process can receive it, then SIGTERM,
then SIGKILL. The launcher, the compositor, the terminal, the session
services and the resident Steam client match none of the roots and are left
running; when the app's window dies its tiles are uncovered.

Run in the VM on 2026-09-08, each app started at the kiosk and ended with
Ctrl-Alt-Home:

| Case | Result |
|---|---|
| Live Tux Paint (XWayland Flatpak) | Ended; tiles back in 2.1 s; launcher, labwc, pipewire up. |
| **Frozen Tux Paint** (SIGSTOP, `T` state, white canvas on screen) | Ended by the SIGCONT then SIGKILL; tiles back in 2.2 s. This is the case nothing could do before. |
| Live GCompris (Wayland-native Flatpak) | Ended; tiles back in 2.1 s. |
| Steam game (Putt-Putt via `reaper`) | Game ended; tiles back in 0.6 s; **the Steam client stayed signed in** (its `steamwebhelper` processes are not a `reaper` tree). |
| Nothing running | Harmless: the helper matches nothing, exits 0, the frame is untouched. |
| The frame after a give-up | Interactive: an arrow key moves the focus ring. |

What it does not do: dismiss a Steam **client** window that forces itself
open (sign-in, update). That is `steamwebhelper`, not a game, and ending it
is a Guardian's `steam -shutdown`, not the panic key; the launcher's
grown-up screen already covers it (DESIGN §8.3). A hard compositor or kernel
lock is still the power button's job, below.

### The power button (ADR-0018)

`session/logind.conf.d/10-cairn-power.conf`: `HandlePowerKey=ignore` and
`HandlePowerKeyLongPress=poweroff`. A tap does nothing at any level; a
five-second hold powers off in order through logind. Verified in the VM: a
tap left the kiosk running, a held press shut the domain off in about ten
seconds. This is the layer below the give-up key, for when the compositor
itself is gone.

### Leaving an app (#77), and the leave key (ADR-0019)

The first child pilot (#8, 2026-09-25) found that a six-year-old could not
leave an app without help. Each app hides its quit somewhere different, and
the only way out the frame had was the grown-up's. The fix is one key for
the child: Super+Q runs labwc's `Close` on the window in front, the polite
request a titlebar's close button sends. (An old X11 app that does not take
that request is disconnected instead, as a titlebar's close button would
do; every app here takes it.) When the launcher is in front the
request reaches the launcher, which treats it as one step back and never
closes (`launcher/src/CloseRequest.h`), because the session ends with it.

It ran twice. Nested on the dev PC, headless labwc 0.9.6 with the kiosk
config and the Debug launcher, keys sent with `wtype` through the virtual
keyboard protocol, which labwc matches against its keybinds by keysym only;
the VM run covers the keycode path a real keyboard takes. Then in
the VM on 2026-09-29 as `ada`, keys sent with `virsh send-key` through the
VM's own keyboard, with the Release launcher and this `rc.xml` installed.
`xdotool` cannot reach the Flatpak Tux Paint, so pointer input in the VM
came from a USB tablet hot-plugged for the run and removed after. Times
were taken by polling over ssh every half second, so they are rough.

| Case | Result |
|---|---|
| At the tiles | Nothing happens. The launcher keeps its pid and the tiles stay up (VM and nested). |
| In the Terminal | Back to the tiles, focus on the Terminal tile. `exit` and Escape still work (VM and nested). |
| On the grown-up screen after a failed launch (the empty Music tile) | Back to the tiles, focus on Music (VM and nested). |
| A window that opened on its own: foot (Wayland) and xterm (X11) | Closed politely; the grown-up screen went when the window did (nested). |
| Tux Paint, Flatpak, with a new stroke on the canvas | "Do you really want to quit?" with a tick and a cross. A second Super+Q is "No, take me back", and the stroke is still there. The tick quits in about 1.7 s and the tiles return. With `--autosave --saveovernew` the picture was saved with no further question (`~/.var/app/org.tuxpaint.Tuxpaint/.tuxpaint/saved/`). The same held nested with the Fedora package. |
| GCompris, Flatpak, at its menu and inside an activity | Closes at once with no question, about 1.0 s; tiles return. |
| Putt-Putt via Steam (ScummVM under `reaper`) | The game closes about 2.3 s after the key and the tiles return. The Steam client stays signed in. |
| Super+Q held for two seconds at Tux Paint | Nothing while it is held; one close when it is let go, whichever key comes up first (VM, keys through QEMU, and nested). Before the binding acted on release, a nested run showed labwc repeating a held binding 25 times a second and Tux Paint's question opening and shutting about fourteen times in three seconds. |
| Frozen Tux Paint (SIGSTOP) | Nothing, as expected: a stopped app cannot answer a polite close. Ctrl-Alt-Home ended it in about 3 s, and the Steam client stayed up. |

Two things for later. Tux Paint's question is in words, but each answer
has a picture, a tick or a cross, so a pre-reader can answer it. The
first-run guide (#78, #79) is where the child learns that the way out of
Tux Paint is Super+Q and then the tick. And
during the Steam run, Steam's own notification about Shift+Tab showed in a
corner of the game, which is #74's to settle.

### Logging out (#44, ADR-0021)

A child ends their own turn from the launcher. The launcher runs
`session/bin/cairn-log-out`, which asks a running Steam client to shut down
and then runs `loginctl terminate-user` on the child's own uid. First,
from inside `ada`'s session scope (the setpriv method above), `loginctl
terminate-user ada` ran with no polkit prompt and left no process of hers:
logind lets any user end their own sessions, and the level rule (#35) does
not touch that.

In the VM on 2026-09-30 as `ada`, keys through `virsh send-key`, with the
Release launcher, `cairn-log-out` and the wrapper's `--log-out` installed:

| Case | Result |
|---|---|
| At the tiles, Up from the top row | Focus moves to Log out, above the tiles, level with their right edge. |
| Enter on Log out | "Log out now?" with Back and Log out, the focus on Back. |
| Right, then Enter on Log out | The login screen was back in about a second. `loginctl` listed no session for `ada` and `pgrep -u ada` found nothing. |
| A running Steam client | Not run in the VM: `ada`'s client had lost its sign-in (#25). `session/tests/test_log_out.py` checks with stand-ins that a running client is asked to shut down first and a stopped one is never started. |

## Remaining rows, and where

| Row | Where | Check |
|---|---|---|

Screenshots and logs from the run are in the maintainer's work area, not the
repository.
