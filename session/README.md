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
  fullscreen on sight; and rules that iconify Steam's own windows on
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
- `logind.conf.d/10-cairn-power.conf`: the power button (ADR-0018). A tap
  is ignored, a five-second hold powers off in order.
- `sessions/cairn.desktop`: the one session entry the greeter offers.
- `sddm/`: the display manager's configuration (ADR-0016). `SessionDir`
  names only Cairn's session directory; `pam.d/sddm` is Fedora's stock file
  plus the two lines that let `cairn-l1` and `cairn-l2` in without a
  password, never an account also in `cairn-guardian` or `wheel`
  (ADR-0023). Nobody is hidden (ADR-0020); the theme is `../greeter/`, and
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
  even from a level group (ADR-0023). The rule never says YES.

`tests/` checks that the labwc and logind files, the greeter's PAM file,
the polkit rule and `cairn-give-up` say what this README promises, runs the dispatcher with
stand-ins for `id`, `labwc` and `startplasma-wayland`, and runs
`cairn-log-out` with stand-ins for `id`, `pgrep`, `steam` and `loginctl`.
Where a systemd user manager is running, it also runs `cairn-give-up` for
real against a frozen stand-in in a scope of its own, with `pkill` stubbed
out so no real app is touched; CTest runs all six.
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
  tiles (`wlr-foreign-toplevel-management`, ADR-0026). cage is a measurement baseline only. Both
  halves held in the VM with a signed-out and a signed-in client
  (`docs/research/steam-containment.md`).

## Files expected here

`pam.d/` for any further rule.
