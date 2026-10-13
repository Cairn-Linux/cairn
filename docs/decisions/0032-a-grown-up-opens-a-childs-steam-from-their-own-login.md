# ADR-0032: A grown-up opens a child's Steam from their own login

**Status:** proposed. It becomes accepted when the helper lands and passes
the checks under Consequences in the VM.
**Date:** 2026-10-12
**Closes:** (none) — addresses issue #131 and leaves the in-session
step-in to #88; once accepted, amends ADR-0017 (cage is used for a visit's
window)

## Context

A child's Steam needs a grown-up now and then: to sign in or switch Steam
account, to answer Steam Guard or an expired sign-in (#25), to turn off the
overlay (#74) or Steam Cloud (#139), or to install a game. None of that can
happen in the child's session. The kiosk hides every Steam window by rule
(ADR-0017), and the launcher ignores hidden windows (ADR-0026). A game that
Steam holds behind a hidden dialog ends on the grown-up screen, with the
dialog still waiting (ADR-0030).

On the kids' laptop that work took a second computer, ssh and root. On
2026-10-11, switching both children's Steam accounts meant taking the Steam
rules out of the installed `rc.xml`, which loosened the kiosk for every
child at once. On 2026-10-07, adding games took a hand-made helper and a
root edit of the manifest. #131 asks for a way to sign a child's Steam in
without loosening the kiosk or using ssh, with Steam's first-run download
out of the child's sight. #88 asks for a Guardian's password to show Steam
inside the child's session. #130 asks for every family's Steam account
layout to be easy to set up.

ADR-0004 puts Steam at every level, gated per child by the Steam Family
allow-list and the Guardian's own per-child game allow-list. ADR-0027 runs
the Steam client in the child's own systemd user manager, under the
child's home, so the child's sign-in and settings live there. DESIGN §3.2
already has a path for work like this: full administration in the
Guardian's own login, with their password.

On 2026-10-12 the maintainer decided:

- Cairn encourages one Steam account per child, each a child member of the
  family's Steam Family. Steam Family's per-child allow-list decides what a
  child can play. The parent's own account in every child's Steam, or one
  shared child account, still work, but are not recommended.
- The parent installs a game once, in their own Steam in their own
  login, into one Steam library that every child's Steam uses (#130). No
  child's Steam downloads a copy of its own. The kids' laptop has run this
  way since 2026-10-07. On 2026-10-11 a Steam account that is a child
  member of the family started a game from it that the parent's account
  had installed.
- A child's game tiles show the installed games that the child's Steam
  account can play, as Steam Family's allow-list says, with no separate
  tick. How Cairn learns that from the child's Steam is still to be found.
- A parent-side setting takes a game off a child's tiles for a while. The
  word parents see is still to be chosen. The child never sees it as a
  punishment.
- Showing a child's Steam needs the Guardian's password, or a fingerprint
  on a computer whose reader works on Linux, never the PIN. The fingerprint
  may be asked before the security key (ADR-0020).
- Until a Guardian can step in inside the child's session (#88), fixing
  what Steam is waiting for means logging the child out. That is
  acceptable if the docs and screens say plainly what to do, and that the
  step-in is planned, so families do not report it as a bug.

A check in the VM tried the mechanism first
(`docs/research/steam-visit.md`). Giving the child's account the
Guardian's own Xwayland with `xhost` works, but every process of the child
can then capture, read and close the Guardian's X11 windows and read the
clipboard. cage, run as the child in a window on the Guardian's desktop,
on a socket that systemd opens and hands over, works too. In the tests
run, the child's processes on cage's own display could not reach the
Guardian's display, keys or clipboard. But any process of the child can
copy cage's own connection to KWin and use it as any other KWin client;
see Consequences. The run also found that a child can log in during a
visit and then shares its Steam, that the Guardian's logout makes cage
abort, and that the handle for a visit is the child's session scope.

## Decision

**A Guardian opens a chosen child's own Steam from their own Plasma
login, never inside the child's kiosk.** One such opening is a *visit*. In
a visit the Guardian does everything above in Steam's own window. A visit
never reads or writes Steam's files.

- **It runs as the child.** Steam runs under the child's account, against
  the child's home. Its sign-in, settings and installs are then the ones
  the kiosk's `steam -silent` uses (ADR-0027).
- **It runs in a nested cage window.** A root helper starts cage as the
  child, as a PAM-backed system unit
  (`systemd-run --uid=<child> -p PAMName=cairn-steam-visit`), and hands it
  the Guardian's Wayland socket already open (`OpenFile=` and
  `WAYLAND_SOCKET`). Steam draws on cage's own Xwayland. There is no
  `xhost` grant, no ACL on the Guardian's runtime directory, no
  `systemd-run --user -M` and no linger. The cage it starts is a copy of
  the image's that the child's account may run but not read (mode 0711).
  Linux does not let a process trace a program its account cannot read,
  or copy that program's open files, so no other program of the child can
  take cage's connection to KWin. That closes the gap below.
- **It needs the Guardian's password or fingerprint.** A Cairn polkit
  action with `auth_admin`, never `auth_admin_keep`, asks through the
  polkit agent Plasma already runs. PAM's `system-auth` counts wrong tries
  with faillock (ADR-0020). Inactive and remote sessions get no. Cairn's
  polkit rule answers no to a Cairn action, before any prompt, unless the
  asking account is an administrator (ADR-0023, rank 1). That covers every
  child at every level, and accounts Cairn did not make. The helper checks
  `PKEXEC_UID` the same way. It opens the visit only on the Wayland socket
  of that account, in its active local session, never on a path the
  caller supplies.
- **One root process per visit.** The Guardian's window starts the helper
  once, through `pkexec`, and that one root process lasts for the whole
  visit. Done, the screen lock and the logout all reach it, so closing
  never asks for the password again and still runs when no prompt can.
- **One visit at a time on the machine, and only while that child is
  logged out.** The helper acts only on a child account, checking for an
  administrator first (ADR-0023). It refuses while the child has a session
  in any state, or any process left from one: Steam runs once per
  account, so a leftover client would take the visit's Steam.
- **The child cannot log in during a visit.** While a visit is open or
  closing, the helper lists the child in a root-owned file under
  `/run/cairn/`. PAM refuses a listed child for the login screen's service
  (`session/sddm/pam.d/sddm`) and for the consoles' `login`. Upstream
  `pam_listfile` does this
  (`account requisite pam_listfile.so item=user sense=deny onerr=succeed
  file=/run/cairn/steam-visits`). The login screen reads the same file
  only to say plainly that a grown-up is setting something up and to try
  again soon. PAM is the gate (ADR-0020). PAM lets everyone in when it
  cannot read the list, and SELinux keeps the login screen from reading a
  plain file in `/run`: in the VM a listed child logged in. So
  provisioning labels `/run/cairn` `etc_t`, which the login screen and the
  consoles may read, and the helper opens no visit while the list has any
  other label. The visit's own session uses a PAM service of its own,
  `cairn-steam-visit`, so the refusal never refuses the visit. `/run` is
  empty after a restart, so no child stays locked out past one.
- **Every ending goes through one close.** These all run the same close:
  Done, the Guardian's screen lock, the Guardian's logout, the visit
  window's own close button, Steam or cage ending by themselves, and the
  Guardian's window or the helper ending unexpectedly. If a Steam process
  of the child is running, the helper runs `steam -shutdown` as the child,
  with a time limit (18 s was seen). Asked to shut down when it is not
  running, Steam starts, as `cairn-log-out` notes. The helper then stops the
  child's session scope (`session-N.scope`, not the unit that started it).
  It removes any X11 socket and lock file the child left in `/tmp`, and
  confirms that no process of the child's account is left. Any process
  still there is killed. If that fails, the child stays refused at login
  and the Guardian's window says to restart the computer.
- **The visit is labelled with the child's name** where the Guardian sees
  it. cage passes Steam's own title through, so the label is the
  Guardian's window: titled "Steam for <child>", with the menu entry's
  name and Steam's icon in the taskbar, beside the visit's window for the
  whole visit. With the Guardian's own Steam paused, the visit's is the
  only Steam window on the desktop.
- **The Guardian's own Steam is paused during a visit** if it is running.
  The Guardian's window does this, as the Guardian. It runs
  `steam -shutdown` before the visit opens, and after the visit closes it
  runs `bazzite-steam -silent`, as Bazzite's autostart does. The root
  helper never starts or stops anything in the Guardian's session, because
  `systemd-run --user -M` left stuck sessions in the VM. Before the visit
  opens, the window says that the Guardian's own Steam, with any game or
  download in it, closes until the visit ends.

**What this ADR builds first.** The root helper, its polkit action and
the change to the levels rule. The PAM refusal of a child whose visit is
open or closing, the visit's own PAM service, and the login screen's plain
sentence. A "Steam for <child>" entry for each child in the Guardian's
application menu, shown only to Guardians, so an L3 or L4 child's Plasma
menu never shows it. The entry's program may be run only by the
`cairn-guardian` group, and Plasma's menu leaves out an entry whose
`TryExec` program the account cannot run. A small Qt window in the
Guardian's session that names the child, has Done, and closes the visit.
It is the first piece of the Guardian tool (P1-5), in `guardian/`. The
steps for parents go in `docs/TRY.md`.

**What comes later, each with its own ADR:**

- One Steam library for every child, which the Guardian fills from their
  own Steam (#130). Provisioning makes the folder and its group and points
  each child's Steam at it.
- Tiles that follow the games each child can play. That amends ADR-0004
  (Steam Family's allow-list and the setting below take the place of the
  Guardian's own per-child game allow-list), ADR-0024 and ADR-0027, edits
  DESIGN §3.2's Permissions row, and answers #24.
- Taking a game off a child's tiles for a while.
- What the grown-up screen tells a grown-up until the step-in exists: log
  the child out and open their Steam from your own login, and that a way
  to step in is planned. That amends ADR-0030.
- The in-session step-in, which stays #88.

## Consequences

- Signing a child's Steam in, switching its account, answering Steam
  Guard, turning off the overlay or Cloud and installing a game need no
  ssh, no `rc.xml` edit and no terminal. The kiosk does not change:
  `rc.xml`, the keys and the launcher stay as they are.
- #131: a visit meets "no `rc.xml` edit and no ssh". Opened before a new
  child's first login, it also keeps Steam's first-run download out of the
  child's sight. In the VM that first start took about 83 s to reach the
  sign-in, and left 2.5 GB in the child's Steam folder before any game.
- The Guardian's computer password is typed only in the Guardian's own
  session, never into a process the child owns, and with the gap below
  closed no process of the child can draw a copy of that prompt.
  Everything inside the visit's window runs as the child, from the child's
  home, so a Steam password typed there is only as private as the child's
  account.
- A family that signs the parent's own Steam account in for a child puts
  that sign-in in the child's home. The kiosk hides Steam's windows at L1
  and L2. At L3 and L4, where the child sees Steam, the store and any
  saved payment details are the child's too, which is why Cairn encourages
  one account per child. After switching a child's Steam to another
  account, the parent removes their own account from Steam's account list
  in the same visit. `docs/TRY.md` says so.
- A visit is rarely where a game is installed. Games go into the shared
  library from the Guardian's own Steam (#130). Steam itself and its
  runtime still sit in each child's own folder: on the kids' laptop,
  Steam's runtime started in about 2 s for the account that owned it and
  in about 18 s for any other. Steam keeps a Windows game's Wine prefix in
  the library the game is in, and Wine refuses a prefix that another
  account owns. So a Windows game that runs through Proton may need a copy
  in each child's own folder, installed in a visit. That is not tried yet
  (#130).
- A game held behind a hidden Steam dialog still means logging the child
  out until #88. The parent docs say so plainly, and say that a way to
  step in without logging out is planned.
- A visit closes when the Guardian's screen locks, and Plasma locks after
  a few idle minutes by default. Games download in the Guardian's own
  Steam, outside any visit. What a lock stops is a download inside the
  visit: Steam's own first-run set-up, or a copy of a Proton game. The
  child's Steam is expected to carry it on at their next login, behind the
  tiles and out of sight (not tried). The Guardian's window says to stay
  until it finishes.
- The visit window's own close button is expected to end cage at once, so
  Steam stops without its shutdown, as at a logout. The Guardian's window
  and `docs/TRY.md` say to use Done.
- At a Plasma logout in the VM, cage ended with KWin within 3 s and Steam
  stopped without its shutdown. The helper's close then ran and left
  nothing of the child behind. For a signed-in child, a Steam stopped that
  way can lose the last cloud save. With the Guardian's window open,
  Plasma's logout asked it to close, and the logout waited until the
  window had closed the visit. But Plasma asks every window at once, and
  cage closed with the rest, so Steam had lost its display before its
  shutdown reached it (VM, 2026-10-12). So the window and `docs/TRY.md`
  say to press Done before logging out. A restart, a shut down and the
  power button's five-second hold (ADR-0018) are untested.
- In the image's authselect profile, a fingerprint is accepted before the
  password and before the security key that ADR-0020 offers. A Guardian
  with both can open a visit with the fingerprint alone, and the same is
  true for sudo, su and the lock screen. The first-boot wizard and the
  Guardian tool say so wherever they offer either one.
- **The gap, closed by the run-only copy of cage.** Anything the child's
  account can change runs during a visit. `/usr/bin/steam` runs the copy
  of Steam in the child's home, and the visit's session starts the child's
  own systemd user units. At L3 and L4 the child has a shell to change
  both. On this image `kernel.yama.ptrace_scope` is 0, so with the image's
  own cage any such process could copy cage's connection to KWin and
  become a full KWin client on the Guardian's desktop. On a connection of
  the child's to the Guardian's socket, KWin offered `ext_data_control`,
  which read the Guardian's clipboard. It also offered layer shell and Plasma's shell protocol,
  which can cover the whole desktop and take every key. A process of the
  child could then draw a copy of the password prompt and read what is
  typed into it. That was not tried. The connection also got output, DPMS
  and brightness control, an input method and the keyboard's state.
  Acceptance needed one of two results: no process of the child but cage
  can get the connection, or the connection is offered none of
  `ext_data_control_manager_v1`, `zwlr_layer_shell_v1`,
  `org_kde_plasma_shell`, `kde_output_management_v2`,
  `org_kde_kwin_dpms_manager`, `kde_external_brightness_v1`,
  `zwp_input_method_v1` or `org_kde_kwin_keystate`. The first holds since
  the helper runs the run-only copy (VM, 2026-10-12). `pidfd_getfd` on
  cage, from a process of the child outside cage's tree and from cage's
  own child, where Steam runs, failed with "Operation not permitted", and
  cage's environment could not be read. With the image's own cage both
  had copied every file cage held. Only cage held its connection to KWin,
  marked close-on-exec, so Steam never inherits it. The three candidates
  first listed here (a `wp_security_context_v1` listener, `ptrace_scope`
  1, cage under another account) are not needed. The copy is made again
  whenever the image's cage changes.
- **Also checked in the VM before acceptance:** a sign-in with "Remember
  me" in a visit, then the child's kiosk login with Steam silent and signed
  in, and a tile starting a game; a game installed in a visit; the overlay
  and Steam Cloud turned off in Steam's Settings window inside cage;
  removing a saved account in Steam's own account list during a visit; the
  child refused at the login screen during a visit, with the plain
  sentence; a child with a password refused at a text console during a
  visit; Done, screen lock, logout and restart each ending with
  `steam -shutdown` run before Steam stopped (from the journal), and with
  no process of the child left, a detached one included; the window's
  close button, Exit in Steam's menu and killing the Guardian's window
  each ending with nothing of the child left; the Guardian's own Steam
  paused and back; the label. The fingerprint needs a reader, so it is
  checked where one exists or stated as unchecked.
- **Checked so far** (VM, 2026-10-12, `docs/research/steam-visit.md`):
  the child refused at the login screen during a visit, with the sentence
  "A grown-up is setting something up for you. Try again soon.", and let
  in again once it closed; Done, the screen
  lock (`loginctl lock-session`), the Guardian's window's own close button,
  killing the Guardian's window, Steam quitting by itself and a signal to
  the helper, each leaving nothing of the child, with `steam -shutdown`
  first whenever Steam was still running; the logout above; the
  Guardian's own Steam paused (2 s) and back; the label; the entries in
  the Guardian's menu and not in a child's. Still to check: everything
  that needs a signed-in Steam, a child with a password at a console, a
  restart, cage's own close button, Exit in Steam's menu, the idle lock
  and a fingerprint.
- **Dependencies.** cage 0.3.1 is part of the Bazzite image
  (`cage-0.3.1-1.fc44`, not a layered package), so the helper adds no
  package, and Cairn's image has to keep it. It is a program the helper
  runs, not a library Cairn links. The Guardian's window asks polkit
  through `pkexec`, run with `QProcess`, which needs no new library.
  polkit-qt6-1 is in the image too, but linking it would be a new
  dependency, which needs its own ADR (CLAUDE.md, "Dependencies"). The
  login refusal uses upstream `pam_listfile`. The fingerprint comes from
  upstream: the image's authselect profile has `with-fingerprint`, and
  polkit's PAM service includes `system-auth`, where `pam_fprintd` sits.
  polkit 127 reads actions from `/etc/polkit-1/actions` too, so the action
  needs no write to the read-only `/usr`. pkexec finds the action by the
  helper's real path, and on Bazzite `/usr/local` is a link to
  `/var/usrlocal`, so provisioning writes that path into the action. The
  window hears the screen lock through Qt D-Bus, which is part of Qt.
- ADR-0017 said cage is not used. Once this ADR is accepted, cage is used
  for a visit's window, never as the kiosk compositor, and ADR-0017's
  status becomes "amended by ADR-0026, ADR-0032".
- When this ADR is accepted, DESIGN §3.2 and §8.3 are edited in the same
  change. In §3.2 that is the Games row, and "Two access paths", which
  records the second check, password or fingerprint, inside the
  Guardian's own login. In §8.3 it is a grown-up's way to see a child's
  Steam.
- systemd counted a memory peak of 1.7 GB for a visit's session scope in
  the VM, with Steam signed out, on llvmpipe. That figure includes page
  cache, so it is an upper bound, and it is not comparable with the
  proportional figures in `steam-containment.md`. P1-1 measures it on
  hardware.
- Nothing phones home. The only network traffic is Steam's own, for an
  account the family chose.
