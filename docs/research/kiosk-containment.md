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
Super+Q closes the app in front (ADR-0019), checked in the VM. On
2026-10-31 a row for ScummVM's own menu was added after a user test
(#215): the kiosk keeps Ctrl+F5 from every app, checked in the VM, but
plain F5 in SCUMM games, and F5 held down before Ctrl, still reach the
menu.

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
| The first app after login comes up (#87) | **Fail, then pass** (VM, 2026-09-30) | Tux Paint launched right after login hung before drawing a window, and the launcher, handling one app at a time, ignored every other tile. PipeWire had been started on demand by that first app. `cairn-session` now starts it before the kiosk; see "Sound at login" below. |
| X11 windows carry no server decorations | **Fail, then pass** (new row) | An xterm got a titlebar with minimise, maximise and close buttons: `<decoration>client</decoration>` only governs Wayland clients. `serverDecoration="no"` on every window removes it. |
| A game cannot open ScummVM's own menu (#215) | **Fail** (VM, 2026-10-31); Ctrl+F5 **passes** headless and in the VM (2026-10-31); two ways in stay open | In Pajama Sam 2, Ctrl+F5 opened ScummVM's menu, whose Return to Launcher showed ScummVM's game manager. A binding in `rc.xml` now keeps Ctrl+F5 from every app, checked headless with labwc 0.9.6 and then in the VM: Ctrl+F5 in Pajama Sam 2 showed nothing and the game went on. Plain F5 opens the same menu in SCUMM games and is not taken; in the VM it still opened the menu. F5 held down before Ctrl reaches a game through Xwayland's key repeat, seen headless. See "ScummVM's own menu" below. |

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

Run again on 2026-09-30 with every launch in a scope of its own (ADR-0025,
#98), the launcher started with `--scope-apps`:

| Case | Result |
|---|---|
| **Frozen native ScummVM** (Putt-Putt from kidscan's tile, SIGSTOP, `T` state) | In `cairn-app-4858-1.scope` with the speech-dispatcher it had started for text-to-speech. Both ended 0.7 s after Ctrl-Alt-Home, the scope was gone, and the launcher, labwc and PipeWire stayed up. Before this the game survived the key. |
| Frozen Tux Paint (Flatpak) | Flatpak moved it into its own `app-flatpak-…scope` and the launch scope emptied and went away. It ended through the `bwrap` match as before; an arrow key then moved the focus ring. |
| A game opened from the Terminal | In a scope of its own, `cairn-app-4858-3.scope`, the session's third launch. |
| Nothing running | The helper exited 0 and the frame was untouched. |

Run again on 2026-10-01 after #117. A Steam client update had moved Steam's
own interface into a sandbox called `srt-bwrap`, which the helper's old
`bwrap` match caught as part of a command line, so every give-up ended
Steam's interface. The helper now matches Flatpak's sandbox by its whole
name, `bwrap`, and finds a Steam game's container as the child of the
game's reaper, a process called exactly `reaper` started with
`SteamLaunch`, since a TERM to the reaper alone left the container and the
game running:

| Case | Result |
|---|---|
| Steam idle, signed in | Ctrl-Alt-Home left all 10 web-helper processes and the client running. Before the fix it ended them. |
| **Frozen Steam game** (Putt-Putt through Steam, its bundled `scummvm` stopped) | The reaper and the game's `srt-bwrap` container ended; Steam's `pv-adverb` then ended the game 1.3 s after the key. Steam's interface and client stayed up; the tiles came back. |
| Frozen Tux Paint (Flatpak) | Ended through the exact `bwrap` match; the launcher, labwc and Steam's interface stayed up. |

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

### Sound at login (#87)

PipeWire is socket-activated in the user's session. Plasma starts it; the
kiosk session did not, so the first program to open audio started it, and
at login that was the child's first app. On 2026-09-30 in the VM, Tux Paint
launched within seconds of login sat for four minutes on 117 ms of CPU
with no window, its main thread waiting beside its two PulseAudio threads,
and the journal showed `pipewire`, `wireplumber` and `pipewire-pulse`
starting in the same second as its scope. A second launch worked.

`cairn-session` now runs `systemctl --user start pipewire-pulse.service`
before labwc, for L1 and L2 only; the unit's dependencies bring PipeWire
and WirePlumber. A failure is ignored, so no sound never means no login.

| Case | Result |
|---|---|
| Fresh login, Draw as soon as the tiles show, three times | PipeWire, WirePlumber and pipewire-pulse were already running when the launcher started. Tux Paint started every time; in the two runs checked by screenshot its splash was up within a second and its tools within a few. |
| Super+Q at that Tux Paint | "Do you really want to quit?", as in ADR-0019's run. |

One thing is left. In one of those runs Tux Paint's SDL audio thread ran
at a full core, although the app drew, answered keys and took Super+Q
normally. It was seen only in the VM, whose sound card is an emulated
Intel HDA, so it needs checking on real hardware before it counts against
the Minimum tier (ADR-0003). An earlier Tux Paint that stopped answering
altogether had PipeWire started by hand from outside the session, which is
the likeliest cause; it did not happen again once the session started
PipeWire itself.

### ScummVM's own menu (#215)

A user test in the VM on 2026-10-31 found a way from a game to
ScummVM's own game manager. Steam's Pajama Sam 2 brings its own ScummVM
2.0.0, and in any ScummVM game Ctrl+F5 opens ScummVM's menu over the
game. Its Return to Launcher button ends the game and shows the game
manager. There, Add Game... browses everything the child's account can
read, the family's shared game folder included. Remove Game would change
the child's own ScummVM settings.

The kiosk now binds Ctrl+F5 in `rc.xml`. labwc keeps a key it has a
binding for from the window in front, the press and the release. A
binding whose only action is `None` is no use: labwc 0.9.6 throws it
away once the config is read, and the key reaches the app again. The
action is `DisableScrollWheelEmulation`, from labwc-actions(5), which
turns off a setting for trackballs. Only an action turns that setting
on, and nothing in `rc.xml` does, so the keys change nothing. Other
actions were ruled out:

- `Execute` with `true` starts a program at every press, and 25 times a
  second while the keys are held.
- `Raise`, `Focus`, `Unshade` and their like act on the game's window.
- `HideCursor` hides the pointer, which a child sees.
- `ToggleKeybinds` would hand every kiosk key to the game, Super+Q
  included.

The binding acts on press. On release labwc keeps back whichever key
comes up first, so a game could miss Ctrl being let go.

Whether a program can get past a labwc binding, from the labwc 0.9.6
sources and a headless run on the dev PC:

- labwc offers no keyboard-shortcuts-inhibit protocol, through which a
  Wayland program asks for the compositor's keys. A maintainer has said
  they would rather not add it (labwc issue #1046).
- labwc offers no Xwayland keyboard grab protocol either, so an X11
  program that grabs the keyboard does so only inside Xwayland. labwc
  still sees every key first.
- The one switch that hands a window the kiosk's keys is labwc's own
  `ToggleKeybinds` action. Nothing in `rc.xml` uses it, and a test keeps
  it that way.
- A program already running as the child can send keys into Xwayland
  with XTEST, which labwc never sees. That takes a program of the
  child's own, not a key press.

The headless run used labwc 0.9.6 with this `rc.xml`. Keys went with
`wtype` to an X11 window that had grabbed the keyboard, and to foot, a
Wayland terminal. Neither got Ctrl+F5, and both got a plain F5. With the
action changed to `None`, labwc logged one more cleared binding, and
both got Ctrl+F5. `wayland-info` listed 51 globals, neither protocol
among them. `xdotool`, through XTEST, did get Ctrl+F5 to the X11
window, and labwc saw nothing. `wtype`'s keys are matched by keysym
only, so the VM run covers the path a real keyboard takes.

It ran in the VM on 2026-10-31 as `ada`, with this `rc.xml` installed
and keys sent with `virsh send-key` through the VM's own keyboard.
Pajama Sam 2 started from its tile, on the ScummVM 2.0.0 that Steam
ships with it. The kiosk's journal had nothing to say about the config.
Ctrl+F5 during the opening cartoon showed nothing, and the cartoon went
on. Three more presses and a two-second hold showed nothing either.
Then R and Q, each alone, reached no game manager and did not quit the
game, so ScummVM was not left thinking Ctrl was held. In the first
room, plain F5 opened ScummVM's menu, and Escape closed it. Ctrl+F5 in
the same room straight after, pressed and then held, showed nothing.
F5 held down before Ctrl opened the menu, but in this game F5 alone
does that, so it says nothing about the key repeat below. `ada` has no
other ScummVM game to try it in: Arthur's Birthday runs through Proton.
Afterwards ScummVM's config for the game still held only Pajama Sam 2.

What the binding does not close, from ScummVM 2.0.0's source, the
headless setup and the VM:

- **Plain F5 in SCUMM games**, which the Humongous games are, opens the
  same menu (`engines/scumm/input.cpp`). An older game's scripts can
  turn that key off. Pajama Sam 2 and the other Humongous games from HE
  version 72 on have no way to. In the VM, F5 opened the menu in Pajama
  Sam 2. The decision on #215 took only Ctrl+F5, and a binding for F5
  would keep it from every app in the kiosk.
- **F5 held down before Ctrl, in any game on ScummVM 2.0.0.** labwc
  matches a binding when a key is pressed, and F5 went down before
  Ctrl. Xwayland, not labwc, repeats a held key for an X11 window, and
  once Ctrl is down the repeats carry it. ScummVM 2.0.0 opens its menu
  on a repeated key as on a first press (`common/EventMapper.cpp`). In
  the same headless setup, F5 held for 1.7 seconds, with Ctrl pressed
  after 0.2, gave the X11 window 28 presses of F5 with Ctrl down, and
  labwc ran nothing. A real Ctrl key does not stop the repeat either:
  Ctrl does not repeat in Xwayland's keymap, and the X server moves the
  repeat only to a key that does. A binding for plain F5 would close
  this too.
- **ScummVM's debugger console** opens in SCUMM games with Ctrl+D, `~`
  or `#`. It is a text console of ScummVM's own, not a shell. Its
  `openlog` command asks the system to open ScummVM's log file, with
  `xdg-open`, or else `xterm` running `less`. In the VM, `~` in Pajama
  Sam 2 slid the console down over the game, and Escape closed it.
  `openlog` was not tried.

ScummVM's other shortcuts reach none of the game manager, a file
browser or a terminal: Ctrl+Q to quit, Ctrl+U to mute, Ctrl+M for the
mouse, the Ctrl+Alt picture keys, Alt+Enter and Alt+S. Ctrl+F7 and
Ctrl+F8 open a virtual keyboard and a key editor only in builds made
with them, and a default build is not.

## Remaining rows, and where

| Row | Where | Check |
|---|---|---|
| A game cannot open ScummVM's own menu (#215) | A real computer, in a child's session, keys from its own keyboard | Ctrl+F5 in a ScummVM game shows nothing and the game goes on, as it did in the VM. The decision on #215 asked for this try on real hardware. |

Screenshots and logs from the run are in the maintainer's work area, not the
repository.
