# session

Login-to-session plumbing. Makes "level is a property of the account"
(DESIGN §3, §4.3) true on a real Linux system.

What is here:

- `labwc/`: the kiosk compositor's configuration. `rc.xml`: no titlebars on
  any window, Wayland or X11; no default key or mouse bindings, only
  Super+Q, the child's way out of an app (ADR-0019), Ctrl-Alt-Home,
  the grown-up's way out of a stuck one (ADR-0018), and a laptop's volume
  keys through PipeWire's `wpctl`, up to 100% and no louder (#43); no window gets focus
  by asking for it; a window rule that makes the launcher
  fullscreen on sight; one that maximizes any ScummVM window, so a game
  whose Steam entry loses its config still fills the screen; and rules
  that iconify Steam's own windows on
  sight while leaving the launcher able to hear about them
  (`docs/research/steam-containment.md`). `environment`: the XKB option
  that takes the Ctrl-Alt-Fn VT-switch keysyms out of the keymap, because
  labwc switches VTs in code and offers no other way to stop it.
  Fullscreen is the compositor's decision, made per session, never a
  launcher option.
- `bin/cairn-session`: the dispatcher (ADR-0012). Reads the account's
  groups and execs labwc with the launcher for `cairn-l1` and `cairn-l2`,
  or `startplasma-wayland` for everyone else. An administrator
  (`cairn-guardian` or `wheel`) gets Plasma whatever else they are in
  (ADR-0023); an account in two level groups gets the more restricted
  session. Before the kiosk it starts the
  child's sound (`pipewire-pulse.service`, which brings PipeWire and
  WirePlumber), because the first app of a session hung while PipeWire
  started on demand (#87); Plasma starts its own. The kiosk's output goes
  to the journal as `cairn-kiosk`, so it outlasts the next login (#122);
  without a journal the session starts all the same.
- `bin/cairn-give-up`: the grown-up's way out of a stuck program
  (ADR-0018, #41). Ctrl-Alt-Home in the kiosk runs it; it SIGCONTs, then
  SIGTERMs, then SIGKILLs the launcher's app scopes (`cairn-app-*.scope`,
  ADR-0025, #98), which hold a native program such as ScummVM and whatever
  it started, and the roots a Flatpak app or a Steam game moves into: a
  process called exactly `bwrap` for a Flatpak app, and for a Steam game
  Steam's reaper (a process called exactly `reaper`, started with
  `SteamLaunch`) with the container it started. Steam's own
  interface runs in a sandbox called `srt-bwrap` under no reaper, so it is
  never matched (#117). All of it is in the child's own session, so a frozen
  app ends and the frame and the Steam client stay up. Then it writes
  `$XDG_RUNTIME_DIR/cairn/give-up`, which the launcher watches, so the tiles
  come back even if a window it was waiting on never closes (ADR-0026).
- `bin/cairn-log-out`: the child's Log out (ADR-0021, #44). The launcher
  runs it when the child confirms. It asks a running Steam client to shut
  down, then ends the child's own account with `loginctl terminate-user`,
  so no app or Steam client is left for the next person.
- `bin/cairn-steam-visit`: a grown-up's way into a child's own Steam from
  their own Plasma login (ADR-0032, proposed). The Guardian runs it through
  `pkexec`, which asks for their password or fingerprint each time; one
  root process then lasts for the whole visit. It checks that an
  administrator asked, from their active session, about a child who is
  logged out, lists the child in `/run/cairn/steam-visits` so PAM refuses
  their login, and starts cage as the child, in a session of the child's
  own, on the Guardian's Wayland socket, with Steam inside. The cage is a
  copy the child may run but not read, so no other program of the child
  can take its connection to the Guardian's desktop. Done on stdin, the
  end of stdin, cage or Steam ending, a login of the child's and a signal
  all run the same close: Steam's own shutdown, then the child's session
  ended, then nothing of the child's left running. It says how the visit
  is going on stdout and writes a line to the journal as
  `cairn-steam-visit` when a visit opens and closes. The Guardian's
  window, "Steam for <child>" in their menu, runs it (`../guardian/`); a
  Guardian can also run it from a terminal in their own session:
  `pkexec /usr/local/libexec/cairn/cairn-steam-visit CHILD`, then type
  `done`. Its second mode, `--copy-games CHILD --all` or `--copy-games
  CHILD APPID...`, runs as root and never through `pkexec`: provisioning
  runs it from a terminal, and on the image `cairn-family save-games`
  runs it as a system job with `systemd-run` (ADR-0041). It takes the same list, runs
  `../provision/cairn-copy-games.py` as the child, and passes on only that
  program's fixed lines (ADR-0033). It notes each line but `have`, and why
  a copy was refused or failed, in the journal. A visit tried meanwhile is
  refused with `copying`, and the Guardian's window says games are being
  copied. `--copy-games --log-out CHILD APPID...` is the same copy for a
  child who is logged in: once the child is listed it says `logging-out`,
  lets Steam save, ends the child's account as a visit's close does, says
  `logged-out` and copies. It refuses `stuck` when a program of the
  child's would not end, and frees the list however it ends (ADR-0041).
- `pam.d/`: `cairn-steam-visit`, the session a visit runs in, which no one
  logs in through; and `login`, Fedora's console login with the line that
  refuses a child on a visit, as the greeter's file has.
- `logind.conf.d/10-cairn-power.conf`: the power button (ADR-0018). A tap
  is ignored, a five-second hold powers off in order.
- `systemd/`: the copy of a child's waiting games at their logout
  (ADR-0041). `user-runtime-dir@.service.d/50-cairn-games.conf` adds one
  line to systemd's own unit, which stops about ten seconds after an
  account's last session ends: `systemctl start --no-block
  cairn-finish-games@UID.service`, with `-` so a failure never breaks a
  logout. `cairn-finish-games@.service` runs `cairn-family finish-games
  UID` once, as root, only while `/var/lib/cairn/games-waiting` holds a
  file; for any account but a child with games waiting it does nothing.
  Both are on the image only, in `/usr/lib/systemd/system`.
- `sessions/cairn.desktop`: the one session entry the greeter offers.
- `sddm/`: the display manager's configuration (ADR-0016). `SessionDir`
  names only Cairn's session directory; `pam.d/sddm` is Fedora's stock file
  plus the two lines that let `cairn-l1` and `cairn-l2` in without a
  password, never an account also in `cairn-guardian` or `wheel`
  (ADR-0023), and one that refuses a child whose Steam a grown-up has open
  (ADR-0032). Nobody is hidden (ADR-0020); the theme is `../greeter/`, and
  `GreeterEnvironment` puts Cairn's QML directory on its import path.
- `security/faillock.conf`: the lockout after wrong passwords (ADR-0020).
  Fedora's defaults, but the count kept in `/var/lib/faillock`, which a
  reboot does not clear. Provisioning turns on `authselect`'s
  `with-faillock` and labels the directory for SELinux.
- `polkit/rules.d/10-cairn-levels.rules`: what a child's session may ask
  the system to do (#35). Fedora lets any user at the console power off,
  mount a USB stick, change Wi-Fi and update Flatpaks. This rule answers
  first, keyed off the level groups: NO at every level for Wi-Fi and
  network changes, Flatpak, PackageKit and rpm-ostree, and malcontent's own
  parental-control settings, which DESIGN §3.2 gives the Guardian; NO at
  L1 and L2 for power-off, reboot, suspend, hibernate and udisks2, which
  nothing in the kiosk offers. L3 and L4 keep Fedora's defaults for power
  and removable media, since Plasma's menu offers them. An administrator
  (`cairn-guardian` or `wheel`) is checked first and always falls through,
  even from a level group (ADR-0023). Cairn's own actions
  (`com.cairnlinux.`) are refused to every other account, before any
  password prompt (ADR-0032). The rule never says YES.
- `polkit/actions/com.cairnlinux.steam-visit.policy`: the action pkexec
  finds by the helper's path, which asks an administrator for a password
  every time, and only at the machine (ADR-0032).
- `polkit/actions/com.cairnlinux.family.policy`: the same for
  `cairn-family`, the command that makes children, sets their levels and
  saves their games (`../guardian/`, ADR-0039, ADR-0041). It has one
  action for each subcommand, found by the command's path and its first
  argument, so a Save asks "to change which games a child has". It asks
  for every change.

`tests/` checks that the labwc and logind files, the greeter's PAM file,
the polkit rule and `cairn-give-up` say what this README promises, runs the dispatcher with
stand-ins for `id`, `labwc` and `startplasma-wayland`, and runs
`cairn-log-out` with stand-ins for `id`, `pgrep`, `steam` and `loginctl`,
and `cairn-steam-visit` with stand-ins for `id`, `getent`, `loginctl`,
`pgrep`, `systemd-run` and the rest. It checks the systemd unit and
drop-in line by line, against the tmpfiles folders, and with
`systemd-analyze verify` where systemd is installed.
Where a systemd user manager is running, it also runs `cairn-give-up` for
real against a frozen stand-in in a scope of its own, with `pkill` stubbed
out so no real app is touched; CTest runs all nine.
What the configuration was tested against, and what is left for the VM, is in
`docs/research/kiosk-containment.md` (P0-4). Phase 0 task **P0-3** wires
this into the VM.

## Decided

- **Level = supplementary group (ADR-0011).** `cairn-l1`, `cairn-l2`,
  `cairn-l3`, `cairn-l4`, `cairn-guardian`; exactly one per account.
  Level change is a group change.
- **One session entry (ADR-0012).** `cairn.desktop` runs `cairn-session`,
  which reads the account's level group and execs either the kiosk
  compositor with the launcher (L1/L2) or `startplasma-wayland` (L3/L4,
  Guardian).
  The greeter offers only this session.

## Decided (continued)

- **SDDM with a Cairn theme (ADR-0016).** Fedora 44 KDE and stock Bazzite
  KDE `44.20260902` both ship Plasma Login Manager, whose configuration
  cannot hide users, limit sessions or take a theme, so provisioning swaps
  it for SDDM explicitly. The Cairn theme replaced Breeze on 2026-09-30
  (ADR-0020, P0-11): every family member has a tile, Guardians included.

- **labwc is the kiosk compositor (ADR-0017).** Window rules keep Steam's
  forced windows off the screen, and the compositor's window list tells the
  launcher when one appears and that it is hidden, so it never holds the
  tiles (`wlr-foreign-toplevel-management`, ADR-0026). cage was a measurement baseline, and
  is the window of a grown-up's Steam visit (ADR-0032, proposed), never
  the kiosk compositor. Both
  halves held in the VM with a signed-out and a signed-in client
  (`docs/research/steam-containment.md`).

## Files expected here

`pam.d/` for any further PAM service.
