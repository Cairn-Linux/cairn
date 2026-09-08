# Kiosk containment: what a child can and cannot reach from the L1 session

**Date:** 2026-09-06
**For:** ROADMAP P0-4 (issue #3) and issue #41
**Result:** Every row that can run nested on the dev PC passes, two of them
only after changes to `session/labwc/` made in the same change. The three
rows that needed the VM ran there on 2026-09-08 during P0-10: the
shortcut and VT rows pass at the real console; `pkcheck` as the child
says polkit lets a child power off, mount and change Wi-Fi, so #35's rule
is needed. One row, a hung or trapped app (#41), fails with no mitigation
yet.

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
No key or pointer events were injected: no virtual-keyboard tool (`wtype`,
`wlrctl`) is installed, and under Plasma a real Alt-Tab or Super never reaches
a nested compositor because KWin takes it first.

Where a row depends on how labwc is written, the 0.9.6 sources were read
(`src/input/keyboard.c`, `src/xwayland.c`, `src/config/rcxml.c`,
`src/desktop.c`).

## Rows

| Row | Result | What happened |
|---|---|---|
| A launched window appears on top; closing it returns to the launcher | **Pass** | Tux Paint at `--fullscreen=native` mapped as a fullscreen X11 window over the launcher and took focus. When it ended, focus returned to the launcher and the tiles were up. See "Tux Paint" below for the manifest bug this found. |
| Alt-Tab, Super and other compositor shortcuts unreachable | **Pass** (by configuration, then by keypress in the VM, 2026-09-08) | The debug log shows no "load default key bindings" line, so the one no-op keybind did its job. At the VM's console Alt-Tab reached the launcher as a plain Tab and moved its focus ring one tile; Super and Alt-F4 did nothing, no switcher or menu appeared, and the launcher and compositor were still running. See "Default bindings" for why this is fragile. |
| Ctrl-Alt-Fn VT switching unreachable | **Pass** (VM, 2026-09-08) | labwc switches VTs in code, before any keybind, with no rc.xml option to stop it. The keymap option `srvrkeys:none`, set in `session/labwc/environment`, removes the keysyms it looks for. The keymap labwc handed its clients had 24 `XF86Switch_VT` entries before and none after. At the VM's console, Ctrl-Alt-F2 and Ctrl-Alt-F1 sent through the virtual keyboard left `/sys/class/tty/tty0/active` on the kiosk's `tty3`. |
| A focus-stealing X11 client cannot take focus from the launcher | **Fail, then pass** | A raw X focus change (`XSetInputFocus`) is reverted by wlroots and never reached the launcher. An activation request (`_NET_ACTIVE_WINDOW`) was honoured: labwc un-minimised the X window and gave it focus over the launcher. With `ignoreFocusRequest="yes"` on every window the request is logged and ignored. |
| Two X11 clients sharing one XWayland (#35) | **Same as above** | Between two xterms a raw focus change was reverted and an activation request switched focus. The same rule stops it. |
| `pkcheck` as the child for power-off, mount, network and package actions (#35) | **Fail, rule needed** (VM, 2026-09-08) | With the L1 account's labwc as the subject, polkit says yes to power-off, reboot, suspend, hibernate, udisks2 mount and eject, NetworkManager enable-disable-wifi, network-control and modify-own-connections, Flatpak app-update and runtime-install, and rpm-ostree upgrade; it wants an admin password only for modify-system, Flatpak app-install, set-user-linger and rpm-ostree install. No Cairn rule exists in `/etc/polkit-1/rules.d/`. The full table is in `steam-containment.md`. |
| A hung or trapped client can be exited without a reboot (#41) | **Fail, no mitigation** | With no bindings there is no way out but the app's own quit. Tux Paint ignored SIGTERM; only SIGKILL ended it. |
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

labwc loads its default key and mouse bindings when the config defines none.
`rc.xml` defines one keybind and one mousebind whose action is `None`, and
labwc 0.9.6 drops those after checking whether the list is empty
(`post_processing` before `deduplicate_key_bindings` in `rcxml.c`). The
order is what keeps the defaults out. A labwc release that swaps the order
would load Alt-Tab, Alt-F4, Super-Return and the window menu silently. The
debug line to watch for on any labwc upgrade is `load default key bindings`;
its absence is the pass.

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

### Hung or trapped apps (#41)

Nothing in the session lets a child leave a window that will not close on
its own. Tux Paint's quit is a button plus a dialog; ScummVM's is a key.
Tux Paint also ignores SIGTERM, so a future "give up on this app" path in the
launcher has to escalate to SIGKILL after a grace period. This row stays
open on #41; the design question there is the compositor-level exit, and
this note only adds the two facts above.

## Remaining rows, and where

| Row | Where | Check |
|---|---|---|
| A hung or trapped app can be exited (#41) | Design first | No mitigation yet; see the row above. |
| `HandlePowerKey` and friends in `logind.conf` | VM, once provisioning writes them | The power button ends the app or the session, never the machine mid-write. |
| The polkit rule for the level groups (#35) | `session/polkit/`, then the VM | Rerun the `pkcheck` list from `steam-containment.md` as the child: every row that says yes today says no. |

Screenshots and logs from the run are in the maintainer's work area, not the
repository.
