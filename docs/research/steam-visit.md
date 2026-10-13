# A child's Steam on the Guardian's desktop

**Date:** 2026-10-12
**For:** ROADMAP P1-5 and ADR-0032 (proposed), issues #131 and #88
**Result:** A child's own Steam, run as the child while they are logged
out, can be shown and used on the Guardian's Plasma desktop. Two routes
were tried. Route A gives the child's account the Guardian's own Xwayland
with `xhost`. It works, but while the grant lasts every process of the
child can capture the Guardian's X11 windows, read keys typed into them,
read the clipboard and close them. It is rejected. Route B runs cage, a
compositor that shows one program, as the child, in a window on the
Guardian's desktop. cage reaches KWin through one socket that systemd
opens and hands over, and Steam draws on cage's own Xwayland. Steam's
sign-in came up in about 30 s and took clicks and typing. In the tests
run, the child's processes could not reach the Guardian's X display, keys
or clipboard through cage. Route B is not safe yet: with `ptrace_scope=0`
any process of the child can copy cage's connection to KWin, and KWin
offers any client a protocol that reads the clipboard, and others that can
draw over the whole desktop. The run also found that a child can log in
during a visit and then shares its Steam, and that the handle for a visit
is the child's session scope, not the unit that starts it. Nothing was
signed in, so a sign-in made in a visit reaching the kiosk is still to be
seen.

Later that day the helper itself (`session/bin/cairn-steam-visit`) ran in
the VM; see "The helper in the VM". A copy of cage that the child may run
but not read closed the gap: no other process of the child could take
cage's connection. PAM refused a listed child at the login screen once
SELinux let it read the list; before that, it let her in.

A *visit*, below, is one opening of a child's Steam on the Guardian's
desktop.

## Why

On the kids' laptop on 2026-10-11, moving each child's Steam to a
different Steam account took a second computer, ssh and root. The kiosk's
two Steam rules came out of the installed `rc.xml` by hand, which loosened
the kiosk for every child at once. Steam's main window was forced open
inside each child's session with `systemd-run` over ssh. After the account
change the rules went back. The parent's saved sign-in, left behind in one
child's Steam, was removed from Steam's files with a throwaway script.

On 2026-10-07, adding the family's games took a terminal too. A hand-made
helper pointed each child's Steam at a shared library, and the tiles went
into `/usr/local/share/cairn/manifest.json` by a root edit.

The same steps are today the only way to turn off Steam's overlay (#74) or
Steam Cloud (#139), to answer Steam Guard or an expired sign-in (#25), or
to sign in a new child (#131). No family can be asked to do that.

On 2026-10-12 the maintainer decided that the fix lives outside the kiosk:
a Guardian opens a child's own Steam from their own login (ADR-0032).
This check asked whether that works at all, before anything is built on
it.

## How it was run

The VM `cairn-min` (4 GB, 2 vCPUs, virtio GPU on llvmpipe; see
`docs/DEVELOPMENT.md`, "Virtual machines"), provisioned with the Guardian
`guardian` (uid 1001), the L1 child `ada` (uid 1002) and the L2 child
`ben`. The offline snapshot `before-steam-visit-2026-10-12` was taken
first. The Guardian logged in at the Cairn login screen to Plasma
(`DISPLAY=:0`, `WAYLAND_DISPLAY=wayland-0`). ada stayed logged out.
Commands ran as root over ssh. The display was read with
`virsh screenshot`, and input came from `virsh send-key` and a hot-plugged
USB tablet.

There were two runs, the same afternoon. The first, 14:00 to 14:25, tried
both routes. The second, 14:29 to 14:48, checked the first independently,
got Route B onto the screen, and corrected three of its conclusions. Where
they differ, this note follows the second.

The image changed between them. The first run started on Bazzite
44.20260902 (KWin 6.7.4, systemd 259.8). Bazzite's own updater, `uupd`,
began downloading a 3.6 GB update at 14:08 and had staged 44.20261012 by
14:18. The reboot at 14:32 booted it (KWin 6.7.5, systemd 259.9). Route A
and the first try at Route B ran on the older image, and so did the second
run's first check, ada's kiosk after those visits (14:29). Route B on
screen, and every check of the second run after the reboot, ran on the
newer one. cage 0.3.1 is the same in both. Steam's RPM went from
1.0.0.87-1 to 1.0.0.87-5 and selinux-policy from 44.8 to 44.11. None of
them is a layered package. On the newer image `bin_steam.sh` is still
labelled `lib_t`, but starting `/usr/bin/steam` directly was not tried
again there.

ada's Steam had never been started before. Only test strings were typed
into it, and nothing was signed in.

## What was tried

### Route A: the child's Steam on the Guardian's Xwayland

The Guardian's Xwayland, `:0`, refuses other accounts. Run in the
Guardian's session, `xhost +SI:localuser:ada` lets processes of ada's
account connect. Steam then runs as ada with `DISPLAY=:0`, and its windows
appear among the Guardian's.

### Route B: cage in a window

cage is a Wayland compositor that shows one program. Run inside Plasma, it
is itself a client of KWin, so it appears as one window on the Guardian's
desktop. It starts its own Xwayland (`:1` here), and Steam, an X11
program, draws there. The child's processes then reach the Guardian's
desktop only through cage's one Wayland connection to KWin.

The first run gave cage that connection with ACLs:
`setfacl -m u:ada:x /run/user/1001` and `u:ada:rw` on `wayland-0`. cage
came up nested ("Starting Wayland backend", "Starting Xwayland on :1") and
Steam ran in it. The Guardian's display had blanked before a screenshot,
and the run stopped there. The ACLs also let ada look up names in the
Guardian's runtime directory. They were removed afterwards. The second run
used systemd's `OpenFile=` instead, with no ACL.

## Commands

### What worked

Route B, as the second run started it:

```sh
sudo systemd-run --uid=ada --unit=ada-visit-b --collect \
  -p PAMName=login \
  -p OpenFile=/run/user/1001/wayland-0:wayland \
  --setenv=WLR_RENDERER=pixman \
  /usr/bin/bash -c 'export WAYLAND_SOCKET=3; unset LISTEN_FDS LISTEN_PID LISTEN_FDNAMES; exec /usr/bin/cage -- /usr/bin/bash -c "exec /usr/bin/steam"'
```

- `PAMName=login` has the unit open a logind session for ada, as a login
  would. pam_systemd starts ada's user manager, sets
  `XDG_RUNTIME_DIR=/run/user/1002`, and moves the unit's processes into
  that session's `session-N.scope`.
- `OpenFile=` has systemd, as root, connect to the Guardian's socket and
  pass the connection to the unit as file descriptor 3. `WAYLAND_SOCKET=3`
  tells cage to use it. ada gets one connection, not a path into the
  Guardian's runtime directory.
- `WLR_RENDERER=pixman` is software rendering. cage's Xwayland also ran
  without glamor ("GBM Wayland interfaces not available"). That is slow on
  llvmpipe. Real hardware is untested.
- `bash -c 'exec /usr/bin/steam'` gets past SELinux; see below.

Route A, started the way a helper would start it:

```sh
# In the Guardian's session:
xhost +SI:localuser:ada
# As root:
sudo systemd-run --uid=ada --unit=ada-steam-visit-sys --collect -p PAMName=login --setenv=DISPLAY=:0 /usr/bin/bash -c 'exec /usr/bin/steam'
# Afterwards, in the Guardian's session:
xhost -SI:localuser:ada
```

The run reached the Guardian's session for `xhost` with
`systemd-run --user -M guardian@`, one of the calls that left stuck
sessions, below. Route A also worked through `machinectl`, which is how
ada's Steam first started:

```sh
sudo machinectl shell --setenv=DISPLAY=:0 ada@.host /usr/bin/bash -c 'exec /usr/bin/steam'
```

Stopping Steam, in both routes:

```sh
sudo setpriv --reuid=ada --regid=ada --init-groups env -i HOME=/var/home/ada USER=ada LOGNAME=ada PATH=/usr/bin XDG_RUNTIME_DIR=/run/user/1002 /usr/bin/steam -shutdown
```

It took 3 to 18 s, and then no Steam process of ada was left. In Route B
cage exited with Steam. What ends the rest of a visit is
`systemctl stop session-N.scope`; see "The handle for a visit".

### What failed

| Command | What happened |
|---|---|
| `machinectl shell --setenv=DISPLAY=:0 ada@.host /usr/bin/steam` | On the older image, SELinux refused it: `avc: denied { entrypoint }` for `/usr/lib/steam/bin_steam.sh`, labelled `lib_t`. `/usr/bin/steam` is a symlink to that script. A PAM-backed `systemd-run` meets the same denial. Started as `/usr/bin/bash -c 'exec /usr/bin/steam'` it works: bash is the program entered, and it then runs the script. |
| `systemd-run --user -M ada@ --unit=ada-steam-visit --collect --setenv=DISPLAY=:0 /usr/bin/steam` | With ada logged out, systemd 259 started her user manager for the call through a PAM session that closed at once. logind stopped the user manager ten seconds later, and Steam with it: the unit started at 14:12:02.7, "Stopping user@1002" came at 14:12:12.8, and Steam stopped at 14:12:14. With `loginctl enable-linger ada` Steam stayed up, but a helper would have to turn linger off again every time. On the kids' laptop on 2026-10-11 this form worked because the child was logged in. |
| Any `systemctl --user -M <user>@` or `systemd-run --user -M <user>@` | Each call left a logind "background" session stuck in `State=closing`, with a dead leader and no scope. `loginctl terminate-session` and `terminate-user` did not clear them. ada's kept her user manager running, with 11 processes and no Steam. Only a reboot removed them. |
| `systemd-run --user -M ada@ --wait --pipe steam -shutdown` | "Remote peer disconnected". `setpriv`, above, works. |

### The handle for a visit

The first run took the `systemd-run` unit for the visit's handle: stop the
unit and the visit is over. With `PAMName=login` that is wrong.
pam_systemd moves every process (cage, Steam, cage's Xwayland and
`sd-pam`) into `session-N.scope`, and the unit shows `Tasks: 0`.

To test it, the launch line ran `setsid -f sleep 7777` before Steam,
standing in for a game that detaches. `systemctl stop ada-visit-b2` ended
cage and Steam, but the `sleep` survived. `session-57.scope` stayed
"active (abandoned)", the session stayed `closing`, and ada's user manager
kept running. `loginctl terminate-session 57` returned 0 and changed
nothing. `systemctl stop session-57.scope` ended the `sleep`, removed the
session, stopped ada's user manager and cleared her `/dev/shm` files.

## Timings

| Start | Route | To "Sign in to Steam" |
|---|---|---|
| ada's first start ever, `machinectl` and the bash wrapper | A | About 83 s. Steam's runtime update and client download took about 30 s (14:02:34 to 14:03:06), extracting and installing about 40 s more, and the window came at 14:03:56. |
| A later start, with linger | A | 31 s |
| A later start, `PAMName=login` | A | 67 s |
| `PAMName=login`, cage and `OpenFile=` | B | About 30 s |

The two later Route A starts ran while Bazzite's updater was writing its
new image, so their times are rough. All of them are on llvmpipe.

Other numbers from the runs:

- After its first start, ada's `~/.local/share/Steam` was 2.5 GB, with no
  game installed and nothing signed in.
- systemd counted a memory peak of 1.7 GB, page cache included, for the
  Route B visit's session scope: cage, its Xwayland and a signed-out
  Steam, over 9 minutes.
- `steam -shutdown` took 3 to 18 s.

## What Route A leaks

Before the grant, ada's connection to `:0` was refused ("Authorization
required"). The socket file `/tmp/.X11-unix/X0` belongs to the Guardian,
mode 0755, so ada cannot connect through it. But Xwayland also listens on
an abstract socket that any local account can reach, so X's own
authorization check is the only gate.

With `xhost +SI:localuser:ada` in place, any process of ada could do this
on the Guardian's display. That means Steam, its web helper, and anything
a game starts:

- **See and capture X11 windows.** It listed the Guardian's X11 windows
  with their titles. It captured the whole of a Guardian KWrite, forced to
  X11, with ImageMagick's `import -window`, even with a Wayland window on
  top of it.
- **Read keys.** An XI2 raw-key listener running as ada received
  everything typed into a Guardian X11 window. From the Wayland-native
  Konsole, plain letters did not arrive, but modifier keys, Ctrl
  combinations and BackSpace did. That matches KWin's default for what
  X11 programs may read of keys typed in Wayland programs.
- **Read the clipboard** whenever any X11 window had focus, the child's
  own Steam window included. KWin copies the Wayland clipboard into
  Xwayland then.
- **Close the Guardian's X11 programs.** `xdotool windowkill`
  (XKillClient) closed the Guardian's KWrite with no prompt. Unsaved work
  would be lost.
- **Ask to inject input.** `xdotool` through XTEST brought up KWin's
  "Remote Control: xdotool is asking to control input devices", which was
  denied. The prompt does not say which account is asking. Its "Always
  allow" choice trusts a name the program gives itself.

`xhost -SI:localuser:ada` refused new connections, but Steam, already
connected, kept all of this until it exited. A grant is also easy to
leave behind: one from a Route A test was still in place when the first
Route B try began, and was taken back about a minute later.

The Guardian's own Steam is an X11 program, and Bazzite starts it
silently at login. In real use it will often be running on the same
display, and any window it opens is an X11 window in reach.

What stayed closed to ada: `/run/user/1001` (mode 0700), and with it the
Guardian's Wayland, D-Bus, PipeWire and ssh-agent sockets; the Guardian's
home (0700); and the X authority cookie. ksmserver's ICE socket is
reachable but needs a cookie ada cannot read.

**Route A is rejected.** For as long as the visit's processes run, they
can read and close the Guardian's X11 windows and read the clipboard, and
taking the grant back does not stop them.

## What Route B isolates

Checked in the second run, with ada's Steam in cage on the Guardian's
desktop:

- ada connecting to `:0` got "Authorization required".
- On cage's `:1`, ada saw only Steam's own windows, and no clipboard
  owner.
- An XI2 raw-key listener running as ada on `:1` saw 0 keys while
  "secret", Ctrl+U and Shift+K were typed into the Guardian's X11 KWrite.
  As a control, it saw 4 of 4 keys typed while cage had focus.
- `wl-paste` as ada, through cage's own Wayland socket, printed "Nothing
  is copied".
- ben, the Guardian and `bazzite` were all refused on `:1`.
- Steam's sign-in filled a maximised cage window. Clicks and typing
  ("xyz") reached its account field.

## What Route B does not isolate yet

- **Any process of the child can take cage's connection.**
  `kernel.yama.ptrace_scope` is 0 on this image. A plain process of ada,
  not one of cage's, copied cage's live KWin connection with
  `pidfd_getfd`. The same call as ben failed with EPERM. So a game, or
  ada's own kiosk session if she logs in, can hold a full KWin client
  connection.
- **KWin offers any client the clipboard.** KWin 6.7.5 offered
  `ext_data_control_manager_v1` to a client running as ada on a
  pre-opened Guardian socket. That client read the Guardian's clipboard
  while the Guardian's own KWrite had focus. It is not limited to a
  focused window. KWin also offered it `zwlr_layer_shell_v1`,
  `org_kde_plasma_shell`, `kde_output_management_v2`,
  `org_kde_kwin_dpms_manager`, `kde_external_brightness_v1`,
  `zwp_input_method_v1` and `org_kde_kwin_keystate`.

Together, that is enough to read the Guardian's clipboard at any time
during a visit. Both halves were shown, but not chained: the clipboard was
read on a fresh connection to the Guardian's socket, opened in advance as
cage's is, not on one copied from cage. Nothing else KWin offered was
tried. Layer shell on the same connection would let a process of the
child draw over the Guardian's desktop, a copy of a password prompt
included. That was not tried either.

Possible fixes, none of them tried:

- Connect cage through a `wp_security_context_v1` listener. KWin offers
  `wp_security_context_manager_v1` and treats such a client as sandboxed,
  which may mean fewer privileged globals.
- Raise `ptrace_scope` to 1. That changes it for every program on the
  machine, which Bazzite may not want.
- Run cage under an account other than the child's, so the child's
  processes cannot trace it.

## Lifecycle risks

1. **The child can log in during a visit.** The Guardian's lock screen
   has a Switch User button. It opened the Cairn login screen with ada's
   tile first and focused, and ada logged in. Her kiosk then shared her
   user manager and `/run/user/1002` with the visit. cage already held
   `wayland-0` and `:1` there, so labwc got `wayland-1` and `:2`.
2. **The kiosk's Steam goes to the visit.** Steam runs once per account.
   `steam steam://open/main`, run as ada in her kiosk with the launcher's
   environment, exited 0 at once. The request went to the visit's Steam,
   and nothing appeared in the kiosk. By the same path, a Steam tile
   (`steam -applaunch`) or the launcher's `--start-steam` would reach
   cage, on the Guardian's locked screen, out of the child's sight. That
   was not run.
3. **The child's Log out ends the visit.** `cairn-log-out` finds Steam by
   account, runs `steam -shutdown`, then `loginctl terminate-user`. The
   journal shows `+ steam -shutdown` at 14:43:35, the visit's unit ending
   at 14:43:38, and then `terminate-user`. From reading the code, not run:
   `cairn-give-up` also matches by account, so Ctrl-Alt-Home in the kiosk
   would end a game started in the visit.
4. **A game that detaches outlives the visit's unit.** It stays in the
   abandoned session scope and keeps the child's session and user manager
   alive. `loginctl terminate-session` does not end it; see "The handle
   for a visit".
5. **The Guardian logging out kills Steam uncleanly.** cage aborted
   (SIGABRT, with a core dump) when KWin went away. Steam was killed
   without `steam -shutdown`, which for a signed-in child puts the last
   cloud save at risk. No sessions were left behind. ada's
   `/tmp/.X11-unix/X1` and `/tmp/.X1-lock` were, and were removed by hand.
6. **Locking the screen leaves Steam running but unusable.** The lock
   screen covered cage. Keys went to its password field, and a listener
   on ada's `:1` saw none. Steam ran on behind it.
7. **Bazzite starts the Guardian's own Steam.**
   `~guardian/.config/autostart/steam.desktop` runs
   `/usr/bin/bazzite-steam -silent`, and on its first run a dialog says
   "Steam is opening for the first time". In real use the Guardian's Steam
   will often be running during a visit: two Steam clients on one
   machine, and two Steam windows a parent can mix up. The first run
   closed the Guardian's Steam before Route A. The two together were not
   tried.
8. **Nothing says whose Steam it is.** In Route A the window is titled
   "Sign in to Steam" and groups under the taskbar's Steam icon. In Route
   B cage passes the same title through, with wlroots' generic "W" icon.
   Nothing names the child.

Also seen:

- After a visit, with the stuck sessions still there, ada logged in at the
  login screen and her tiles came up. Her Log out then stopped the
  leftover user manager. One file stayed,
  `/dev/shm/u1002-ValveIPCSharedObj-Steam`: the stuck sessions kept ada
  from being fully logged out, so systemd's `RemoveIPC` never ran. The
  reboot cleared it.
- Starting ada's user manager also ran Bazzite's per-user setup
  (`pkexec bazzite-privileged-user-setup ada`, which failed on a missing
  `steam-web-debug-portforward` unit), PipeWire, AT-SPI and gvfs for her.
  `xdg-desktop-portal` failed to start for her.
- In Route A, from its start, ada's Steam wrote only under
  `/var/home/ada` (Steam, `.cache/mesa_shader_cache`, `.config/pulse`,
  `.local/state/wireplumber`), `/run/user/1002` and `/dev/shm/u1002-*`.
  Nothing was written in the Guardian's home.

## Rows

| Row | Result | Evidence |
|---|---|---|
| A logged-out child's Steam shows on the Guardian's desktop (Route A) | **Pass** | "Sign in to Steam", owned by `steamwebhelper` running as ada, on the Guardian's desktop at 14:03:56. |
| Clicks and typing reach it (Route A) | **Pass** | A click put the cursor in the password field. Three keys showed as three dots, then were deleted. |
| The same inside cage (Route B) | **Pass**, second run | A maximised cage window with Steam's sign-in about 30 s after the start; "xyz" in the account field. |
| A visit writes only the child's files (Route A) | **Pass** | ada wrote only under her home, `/run/user/1002` and `/dev/shm/u1002-*`. Route B's cage also makes `/tmp/.X11-unix/X1` and `/tmp/.X1-lock`, owned by the child. Route B's other writes were not listed. |
| `steam -shutdown` ends Steam | **Pass** | 3 to 18 s; no Steam process of ada left; cage exited with Steam. |
| Steam starts from a PAM-backed process | **Fail, worked around** | SELinux `entrypoint` denial on `bin_steam.sh` (`lib_t`), on the older image. `bash -c 'exec /usr/bin/steam'` works. |
| `systemd-run --user -M ada@` for a logged-out child | **Fail** | Steam killed after 10 s; stuck `closing` sessions until a reboot. |
| The `systemd-run` unit is the visit's handle | **Fail** | The processes are in `session-N.scope`. A `setsid` process survived stopping the unit. |
| Stopping `session-N.scope` ends the visit | **Pass** | The detached process ended, the session went, the user manager stopped, `/dev/shm` was cleared. |
| Route A: the Guardian's X11 windows stay private | **Fail** | Window list with titles; whole KWrite captured under a Wayland window. |
| Route A: the Guardian's keys stay private | **Fail** | Every key in X11 windows; modifiers, Ctrl combinations and BackSpace from Konsole. |
| Route A: the clipboard stays private | **Fail** | Read whenever an X11 window had focus. |
| Route A: the Guardian's programs cannot be closed | **Fail** | `xdotool windowkill` closed KWrite without a prompt. |
| Route A: input injection | **Asked, denied** | KWin's Remote Control prompt, which does not name the account. |
| Route A: taking the grant back ends access | **Fail** | Only new connections refused. |
| Route B: the child is refused on `:0` | **Pass** | "Authorization required". |
| Route B: no Guardian keys on cage's `:1` | **Pass** | 0 keys on `:1` while typing in the Guardian's KWrite; control: 4 of 4 in cage. |
| Route B: the clipboard stays private through cage | **Pass** | `wl-paste` through cage: "Nothing is copied". No clipboard owner on `:1`. |
| Route B: other accounts are kept off cage's display | **Pass** | ben, the Guardian and `bazzite` refused on `:1`. |
| Route B: cage's KWin connection stays cage's | **Fail** | `ptrace_scope=0`. `pidfd_getfd` as ada copied it; ben got EPERM. |
| Route B: the Guardian's clipboard stays private on a connection like cage's | **Fail** | `ext_data_control_manager_v1` read it while the Guardian's KWrite had focus, on a fresh connection, not one copied from cage. |
| Route B: what else a copied connection can do | **Not run** | Offered layer shell, Plasma shell, output management, DPMS, brightness, input method and keystate; none tried. |
| The child cannot log in during a visit | **Fail** | Switch User on the lock screen; ada's kiosk shared `/run/user/1002` with the visit. |
| The kiosk's Steam stays in the kiosk | **Fail** | `steam://open/main` from the kiosk went to the visit. |
| The child's Log out leaves the visit alone | **Fail** | `steam -shutdown` at 14:43:35; the visit ended at 14:43:38. |
| The Guardian's logout closes the visit cleanly | **Fail** | cage aborted; Steam killed without `steam -shutdown`; stale X11 socket and lock left. |
| Screen lock during a visit | **Recorded** | The lock screen covers cage. No keys reach `:1`. Steam keeps running. |
| The visit says whose Steam it is | **Fail** | "Sign in to Steam", with Steam's or a generic icon. |
| The child's kiosk works after a visit | **Pass** | Tiles came up; Log out stopped the leftover user manager. |
| A sign-in made in a visit reaches the kiosk | **Not run** | Nothing was signed in. |
| The Guardian's own Steam alongside a visit | **Not run** | Bazzite starts it at login; it was closed for Route A. |

## The helper in the VM

2026-10-12, evening. Provisioning's new step installed the helper, the
run-only copy of cage, the visit's PAM service, the console's `login`, the
polkit action and the SELinux rule. The Guardian logged in to Plasma at the
Cairn login screen. A shell of the Guardian's, inside their session, ran
`pkexec /usr/local/libexec/cairn/cairn-steam-visit ada`, as the Guardian's
window will, and the VM-only password went into Plasma's own prompt.
Nothing was signed in to Steam.

### The run-only copy of cage

cage was copied to `/usr/local/libexec/cairn/cage-visit` with mode 0711,
root-owned. A program its account may run but not read is not dumpable
(`fs.suid_dumpable` is 2). The kernel then refuses another process of the
same account `ptrace`, `pidfd_getfd` and the program's `/proc/<pid>/`
files, unless it has `CAP_SYS_PTRACE`. The probe tried `pidfd_getfd` on
fds 0 to 63 of cage, and reading its `/proc/<pid>/environ`.

| cage | From | Result |
|---|---|---|
| `/usr/bin/cage` (0755), headless | ada, outside cage's tree | Copied every fd: `/dev/null`, two sockets, the event loop's fds. `environ` read. |
| `/usr/bin/cage` (0755), headless | cage's own child, as ada | The same. |
| the copy (0711), headless | ada, outside cage's tree | "Operation not permitted" for every fd; `environ` refused. |
| the copy (0711), headless | cage's own child, as ada | The same. |
| the copy (0711), nested on a KWin of the Guardian's (`kwin_wayland --virtual`) | both | The same. Steam's sign-in mapped on cage's Xwayland. |
| the copy (0711), a real visit | ada, outside cage's tree | The same. Of every process on the machine, only cage held its socket to KWin, and that fd was close-on-exec (`flags: 02000002`), so Steam never inherits it. |
| any of them | ben | "Operation not permitted". |

### The login refusal and SELinux

The first try of `pam_listfile` in `/etc/pam.d/sddm` let a listed ada in.
The journal had `pam_listfile(sddm:account): Couldn't open
/run/cairn/steam-visits` and an AVC, `denied { getattr } for
comm="sddm-helper" path="/run/cairn/steam-visits"`, from `xdm_t` on
`var_run_t`. With `onerr=succeed`, a list PAM cannot read lets everyone
in. `onerr=fail` would instead refuse everyone, Guardians too, whenever
the list is missing. With `semanage fcontext -a -t etc_t
'/run/cairn(/.*)?'` and `restorecon`, the same login gave
`pam_listfile(sddm:account): Refused user ada for service sddm`, and the
login screen showed "Something needs a grown-up." During a real visit,
after Switch User (SDDM's `SwitchToGreeter` over D-Bus), ada was refused
the same way and the visit stayed open. So provisioning adds that label,
and the helper opens no visit while the list has any other.

### pkexec and polkit

- pkexec finds an action by the program's real path. On Bazzite
  `/usr/local` is a link to `/var/usrlocal`, so an action naming
  `/usr/local/libexec/cairn/cairn-steam-visit` never matched. pkexec fell
  back to its own action and its own message ("Authentication is needed
  to run ... as the super user"), and Cairn's polkit rule did not apply.
  With the real path in the action, Plasma's prompt said "A grown-up's
  password is needed to open a child's Steam."
- polkitd did not see `/etc/polkit-1/actions` when the directory was made
  after it started. `systemctl restart polkit` fixed that, and Plasma's
  agent registered again by itself.
- pkexec asks polkit about its parent process. When the parent had
  already exited, pkexec found no session and no agent, and failed with
  "Error creating textual authentication agent". The Guardian's window has
  to stay running as pkexec's parent.
- A logged-in ada asking for the action (`pkcheck` from inside her
  session) got `polkit.result=no`, "Not authorized", with no prompt. The
  generic pkexec action, as a control, gave `auth_admin`.
- The visit's own PAM service, `cairn-steam-visit`, opened the visit: a
  logind session of class `background` for ada.

### How each ending closed

| Ending | Result |
|---|---|
| "done" on stdin | Closed in 5 s: `steam -shutdown` as ada, then `loginctl terminate-user ada`. No process of ada left, no `pkill` needed, the list and cage's X11 socket gone. |
| The end of stdin, as when the window goes | The same. |
| Steam quitting by itself (`steam -shutdown` from outside, as Exit in Steam's menu would) | cage ended when Steam did, and the close ran. Nothing left. |
| SIGTERM to the helper | First run: bash crashed ("stack smashing detected") partway through the close and left ada listed, so she would have stayed refused until a restart. The trap left the script from inside `read -t`, and the read's timer went off later. A signal now only sets a flag; the rerun closed cleanly. |
| The Guardian logging out of Plasma | cage ended with KWin within 3 s and Steam stopped without its shutdown. The close then ran: nothing of ada left, the list gone. |
| ada logging in during a visit (that run's `sddm` file lacked the refusal line) | The helper saw her login session and closed the visit: Steam shut down, then ada was logged out, kiosk and all. Nothing left. |

## The Guardian's window in the VM

2026-10-12, late evening. Provisioning's new step installed the window,
`/usr/local/bin/cairn-steam-for`, and an entry for each child, "Steam for
ada" and "Steam for ben", in `/usr/local/share/applications`. The VM still
had an earlier draft of the helper, without its journal lines, so the
merged one was installed first. The window was started as Plasma's menu
starts an app, with `kioclient exec` on the entry. It then ran in an app
unit of the Guardian's own service manager, not in their session's scope,
and polkit still found the Guardian's session: Plasma's agent asked. For
the pause, the Guardian's own Steam was a copy of ada's signed-out client,
started by Bazzite's autostart entry. Nothing was signed in.

### The menu

`kbuildsycoca6 --noincremental --menutest` as the Guardian listed both
entries under Games, and as ada, neither. The entries name the window's
program as their `TryExec`, which is `root:cairn-guardian` with mode 0750.
A test entry with the same `TryExec` behaved the same, and one without
showed for both. A second run of the step changed nothing.

### How the window went

| What | Result |
|---|---|
| Open, the Guardian's Steam not running | Plasma's prompt, then "ada's Steam is open". Steam's sign-in filled the screen in cage's window, titled "Sign in to Steam" with wlroots' "W" icon. The window, "Steam for ada" with Steam's icon, was behind it in the taskbar. |
| Done | Closed in 5 s, `steam -shutdown` first. The journal had "guardian opened ada's Steam visit" and "guardian closed ada's Steam visit", which the Guardian can read without `sudo`. |
| The window's own close button during a visit | "Closing ada's Steam", the same close, then the window went. |
| `loginctl lock-session` during a visit | KWin sent `ActiveChanged(true)` on `/org/freedesktop/ScreenSaver` and `/ScreenSaver`. The close ran, `steam -shutdown` first, in 3 s. After the unlock the window said "ada's Steam is closed". |
| The Guardian's own Steam running | The first screen said it closes first, with any game or download in it. On Open the window said "Closing your own Steam", and it was gone in 2 s; then the prompt. After Done, `bazzite-steam -silent` started it again, the window said so, and it ran on after the window closed. |
| Cancel in the password box | pkexec ended with "Not authorized", exit 127, as for a wrong password. The Guardian's own Steam came back. |
| Killing the window (`SIGKILL`) during a visit | The helper read the end of its input and closed: `steam -shutdown`, nothing of ada left, the list gone in 7 s. |
| Plasma's logout during a visit | Plasma asked the window to close. It closed the visit, and the session ended 2 s after the window did. But cage's window was asked at the same moment: cage logged "failed to write to Xwayland fd: Broken pipe", and Steam's last line was from 9 s before. The helper's `steam -shutdown` found a Steam process on its way out, and so started a new client, which ended with "Unable to open X11 display". Nothing of ada or the Guardian was left. |

### The login screen's sentence

With ada's visit open, Switch User brought up the login screen (it had
started after the new theme was installed), and Enter on ada's tile asked
for her login. PAM refused it (`pam_listfile(sddm:account): Refused user
ada for service sddm`), and the screen said "A grown-up is setting
something up for you. Try again soon." The greeter runs as `xdm_t`, and
the list's `etc_t` label let it read the list with no SELinux denial. Once
the visit had closed, the same press logged ada in to her tiles.

### Found and fixed

- The window's row of buttons could hide for good: the row's visibility
  followed the main button's, and a hidden row hides its button too. The
  window's own test caught it.
- After the password box closed, the window kept its shorter size and cut
  off its buttons: KWin's resize when the window became active again won
  over the window's own. The window now sets its least and its most
  height to fit its words.
- Plasma's agent gives pkexec's "not authorized" for Cancel, so the
  window blamed the password. Both now get one sentence: it needs your
  password, the box was closed, or the password was not accepted.
- The restarted Steam's output went into the window's journal. Steam
  keeps its own logs, so that output now goes nowhere.

## What the helper must do

From all of the above, for ADR-0032's helper:

- [x] Start only from the Guardian's own Plasma session, through a polkit
  action that asks for the Guardian's password or fingerprint every time:
  `auth_admin`, never `auth_admin_keep`, and no for inactive or remote
  sessions. Any account that is not an administrator (ADR-0023, rank 1)
  gets no, before any prompt: every child at every level, and accounts
  Cairn did not make.
- [x] Start the helper once per visit, through `pkexec`, and keep that one
  root process for the whole visit, so closing never asks again.
- [x] Open the visit only on the Wayland socket of the account that
  asked, in its active local session, never on a path the caller
  supplies.
- [x] Act only on a child account, checking for an administrator first
  (ADR-0023).
- [x] One visit at a time on the machine, and only while that child is
  logged out. Refuse while the child has a session in any state, or any
  process left from one: Steam runs once per account, so a leftover
  client would take the visit's Steam.
- [ ] Keep the child out until the visit is over. While a visit is open
  or closing, PAM refuses that child, for the login screen and for the
  consoles' `login`, from a root-owned list under `/run/cairn/`. The
  login screen reads the same list only to say plainly that a grown-up is
  setting something up and to try again soon. Closing the visit on screen
  lock is not enough on its own: `steam -shutdown` took up to 18 s, the
  child's tile is one press away, and other ways to Switch User were not
  looked for. Both are recommended, with PAM as the guarantee. Done for
  the login screen, with its sentence; the console, with a child who has
  a password, is not run.
- [x] Start the visit as a PAM-backed system unit for the child, under a
  PAM service of its own so the login refusal does not refuse it
  (`systemd-run --uid=<child> -p PAMName=cairn-steam-visit`; the VM used
  `login`). Never `systemd-run --user -M <child>@`, never linger.
- [x] Start Steam as `bash -c 'exec /usr/bin/steam'`, or through another
  wrapper SELinux allows.
- [x] Give cage the Guardian's Wayland socket already open (`OpenFile=`
  and `WAYLAND_SOCKET`). No `xhost` grant, and no ACL on the Guardian's
  runtime directory.
- [x] Close the gap before ADR-0032 is accepted. Anything the child's
  account can change runs during a visit: the copy of Steam in the
  child's home, and the child's own systemd user units. With
  `ptrace_scope=0` any such process can copy cage's connection to KWin
  and become a full KWin client on the Guardian's desktop. Re-run the
  probes: `pidfd_getfd` on cage's connection, from a process of the child
  outside cage's tree and from Steam inside it, and the list of globals
  KWin offers on that connection. Acceptance needs one of two results: no
  process of the child but cage can get the connection, or the connection
  is offered none of `ext_data_control_manager_v1`,
  `zwlr_layer_shell_v1`, `org_kde_plasma_shell`,
  `kde_output_management_v2`, `org_kde_kwin_dpms_manager`,
  `kde_external_brightness_v1`, `zwp_input_method_v1` or
  `org_kde_kwin_keystate`. Done with the run-only copy of cage: no process
  of the child but cage can get the connection.
- [x] Record the session the unit opened. The visit's handle is
  `session-N.scope`, not the unit. The helper ends the child with
  `loginctl terminate-user` instead, which ends that scope and the child's
  service manager: the child has no other session during a visit.
- [x] Label the visit with the child's name where the Guardian sees it.
  cage passes Steam's title through, so the label has to be the helper's
  own. It is the Guardian's window, "Steam for <child>", beside the
  visit's window.
- [x] Pause the Guardian's own Steam if it is running, from the
  Guardian's window, as the Guardian: `steam -shutdown` before the visit
  opens, and `bazzite-steam -silent` after it closes, as Bazzite's
  autostart does. The root helper never starts or stops anything in the
  Guardian's session, because `systemd-run --user -M` left stuck sessions.
  Before the visit opens, say that the Guardian's own Steam, with any game
  or download in it, closes until the visit ends.
- [ ] Close the visit the same way however it ends: Done, the Guardian's
  screen lock or logout, the visit window's own close button, Steam or
  cage ending by themselves, and the Guardian's window or the helper
  ending unexpectedly. On logout the close has to run before KWin exits,
  or cage aborts and takes Steam with it. How to run it in time is
  untested. The helper's part is done: Done, the end of its input, cage
  or Steam ending, a signal and a logout all close it. The window adds
  the screen lock and its own close button. At a logout Plasma asks every
  window to close at once, so cage still goes before Steam's shutdown,
  and the window says to press Done first. cage's own close button is
  not tried yet.
- [x] Close in this order. If a Steam process of the child is running,
  `steam -shutdown` as the child, with `HOME` and `XDG_RUNTIME_DIR` set,
  waiting long enough for it (18 s was seen); asked to shut down when it
  is not running, Steam starts. Then `systemctl stop session-N.scope`.
  Then remove any stale X11 socket and lock file the child owns, and
  confirm no process of the child's account is left, killing any that
  is. If that fails, keep the child refused at login and say to restart
  the computer.
- [x] Write one journal line when a visit opens and one when it closes,
  naming the Guardian and the child (#85).
- [x] Never read or write Steam's files, and never log or pass a
  password.
- [x] Fail closed. If anything fails before Steam starts, change nothing.
  After it starts, every ending goes through the same close.

## Not yet seen

- A visit that signs in with "Remember me", then the child's kiosk login
  with Steam silent, hidden and signed in. Nothing was signed in here.
- The Guardian's own Steam paused with a game or a download running. In
  the VM it was signed out.
- The screen locking by itself after idle minutes. Only
  `loginctl lock-session` was tried.
- Sound and GPU acceleration in a visit, and any of this on real
  hardware.
- A fingerprint through polkit. The image has `fprintd` and authselect's
  `with-fingerprint`, and polkit's PAM service includes `system-auth`, but
  the VM has no reader.
- A way to Switch User other than the lock screen.
- Installing a game, and Steam's Settings window, inside cage.
- Closing cage's window with its title-bar close button instead of Done.
- A download stopped by the screen lock, and where Steam carries it on.
- A child with a password refused at a text console during a visit.
- A restart, a shut down or the power button's hold during a visit.

Screenshots and probe scripts from both runs are in the maintainer's work
area, not the repository. Two of the first run's screenshots do not show
what their names say: the Guardian's Steam starting at login, and Steam's
"Updating" window. Those rows rest on the process list and the journal
instead.
