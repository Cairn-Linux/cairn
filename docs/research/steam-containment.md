# Steam containment: what the kiosk does with the client's own windows

**Date:** 2026-09-08
**For:** ROADMAP P0-10 (issue #25), the premises in #34, and D3
**Result:** With one labwc window rule, every window the Steam client
opened in the L1 session stayed off the screen from its first frame, and
the launcher showed "Something needs a grown-up" naming it. Without the
rule the sign-in window sat centred on top of that screen. The rule has
to match the WM_CLASS *instance*, `steamwebhelper`; matching the class
`steam` did nothing. The client is about 800 MB of proportional memory
whether signed in or out. The rows that need a signed-in client wait for
the maintainer's account; they are listed at the end with the exact check.

## How it was run

The Bazzite VM `cairn-min` (4 GB, 2 vCPUs, virtio GPU on llvmpipe,
Bazzite 44.20260902, labwc 0.9.6, Steam RPM 1.0.0.87 from Terra, client
build 1788652215), provisioned by `provision/cairn-provision.sh` with the
Guardian `guardian` and the L1 child `ada`. Two sessions:

- **Guardian, Plasma, signed in.** The maintainer signed in at the VM's
  console on 2026-09-07 and installed four Humongous titles. Settings were
  opened with `steam steam://open/settings` from ssh with the session's
  environment, and driven with `xdotool` through KDE's remote-control
  portal. Window properties were read with `xprop`.
- **ada, the kiosk, signed out.** Logged in from the SDDM greeter with
  Enter on the empty password field (ADR-0016). The client was started
  from ssh as ada with the session's `WAYLAND_DISPLAY`, `DISPLAY` and
  `XDG_RUNTIME_DIR`, by the same command Bazzite's autostart entry uses:
  `/usr/bin/bazzite-steam -silent`. X11 windows were listed with
  `xprop -root _NET_CLIENT_LIST` and read with `xprop` and `xwininfo`
  **as ada**; as root, Xwayland refuses the connection and the list is
  silently empty, which cost the first run its window log. The display
  was captured with `virsh screenshot`. Window rules were changed in
  `/usr/local/share/cairn/labwc/rc.xml` and reloaded with SIGHUP to labwc,
  which applies them to windows mapped after that point.

Memory is proportional set size (`Pss` from `/proc/PID/smaps_rollup`)
summed over a process tree, because the client is a dozen processes that
share most of their pages and summing RSS overstates them by a third.

## Rows

| Row | Result | What happened |
|---|---|---|
| Settings › Family: what the client offers (#34) | **Recorded** | Steam Families only. The page shows the family's name, a "Manage your Steam Family" link, and the members with their roles (the maintainer is ADULT). There is no Family View, no PIN, and nothing else on the page. Family View is gone from this client build; the mechanism DESIGN §8.3 called mitigation 1 does not exist. |
| Client packaging (#34) | **Recorded** | The `steam` RPM 1.0.0.87 from the Terra repo, behind `/usr/bin/bazzite-steam`, a bash wrapper that adds `-testoobeupdater`, on Deck images `-steamdeck`, on Wayland an `LD_PRELOAD` of `extest` (which the 32-bit client binary refuses with "wrong ELF class", harmlessly), and on a first run a KDE dialog. `/etc/skel/.config/autostart/steam.desktop` runs `/usr/bin/bazzite-steam -silent %U`, so every new account, ada included, has the entry; labwc runs no XDG autostart, so in the kiosk it is inert and the session must start the client itself. Plasma sessions get a resident client from it. |
| Client starts with `-silent` and shows no window | **Not wired; observed by hand** | `cairn-session` does not start the client yet. Started by hand with the autostart's command, the signed-out client opened no main window; it opened the sign-in window, below. A signed-in `-silent` client is the row that matters and needs the account, but see "The silent main window" under findings. |
| The first thing on screen is Bazzite's, not Steam's | **Fail, mitigation named** | Within ten seconds of the wrapper starting, a KDE dialog appeared: "Steam is opening for the first time. Please wait, downloading the Steam client may take a few minutes." It is a native Wayland client titled "Steam", so no `WM_CLASS` rule reaches it, and the wrapper waits for it to be dismissed before starting the client at all. The launcher put up "Something needs a grown-up. Steam opened on its own." under it, with the dialog centred on top. The session should run `/usr/bin/steam -silent` directly, or pass `--disable-firstrun`, and never go through the wrapper; recorded for P1-13. |
| `steam -applaunch <appid>` brings a game up and returns to the launcher | **Not run signed in** | Signed out, `steam -applaunch 283920` returned exit 0 in under a second and nothing visible happened; the hidden sign-in window stayed hidden. That is #42 exactly: the exit status says nothing about the launch. |
| Force the client window open: `steam steam://open/main` | **Pass** | Signed out, the command produced no new window and did not un-hide the sign-in window. Still to run signed in. |
| Force the client window open: a pending client update | **Not reproducible today** | The bootstrap log ended "Download skipped: version 1788652215, installed version 1788652215 … Nothing to do". The bootstrapper's own window, if it showed one, was not captured because of the root `xprop` mistake above. |
| Force the client window open: a signed-out client | **Fail, then pass** | The sign-in window ("Sign in to Steam", `WM_CLASS` `"steamwebhelper", "steam"`, type NORMAL, 700 by 440, centred, focused) mapped on top of the grown-up screen. With the rule below it mapped straight into the iconic, hidden state at t = 3.6 s after the client started and stayed there for the rest of the run, through `steam://open/main` and `-applaunch`. The child sees "Something needs a grown-up. Sign in to Steam opened on its own." and nothing else. |
| The launcher learns of the window and shows the grown-up screen | **Pass** | For the KDE dialog and for the sign-in window, hidden or not. The text names the window's title. |
| A window that slips through is locked by a PIN | **Not applicable** | There is no PIN. What remains is the child role in a Steam Family, which Valve sets on the account, on the web, not on this machine. Whether a child-role account's client hides the store, community and friends has not been checked and needs a child member in the family. |
| Steam Guard and login expiry: what the child sees | **Half recorded** | The signed-out state is the sign-in window: account name, password, "Remember me" ticked, a QR code, "Help, I can't sign in" and "Create a Free Account" links. With the rule the child never sees it; the grown-up screen names it. The Steam Guard prompt itself needs a sign-in. |
| Idle memory of the client against the 4 GB floor | **Recorded, see below** | About 800 MB proportional in both states, on this VM, not the laptop. |
| Reboot, client update, Steam Guard persistence (#34) | **Not run** | Need a signed-in client. |
| Alt-Tab, Super, Alt-F4 by keypress (P0-4) | **Pass** | Sent through the VM's keyboard at the kiosk. Alt-Tab reached the launcher as a plain Tab and moved the focus ring one tile; Super and Alt-F4 did nothing. No switcher, menu or on-screen display appeared and the launcher and compositor were still running afterwards. |
| Ctrl-Alt-Fn reaches no VT (P0-4) | **Pass** | Ctrl-Alt-F2 and Ctrl-Alt-F1 at the kiosk: `/sys/class/tty/tty0/active` stayed `tty3` throughout. |
| `pkcheck` as the child (P0-4, #35) | **Fail, rule needed** | With ada's labwc as the subject, polkit answers **yes** to power-off, reboot, suspend, hibernate, udisks2 mount and eject, NetworkManager enable-disable-wifi, network-control and modify-own-connections, Flatpak app-update and runtime-install, and rpm-ostree upgrade. It asks for an admin password for NetworkManager modify-system, Flatpak app-install, set-user-linger and rpm-ostree install. The PackageKit actions are not registered on Bazzite. There was no Cairn rule in `/etc/polkit-1/rules.d/`; a stray keypress that reaches `systemctl poweroff` is not the failure mode we want a five-year-old to find. `session/polkit/` closed this the same day; the after-table is in `kiosk-containment.md`. |

## Findings

### The rule, and why it is `steamwebhelper`

labwc's `identifier` attribute matches `app_id` for Wayland clients and
"WM_CLASS" for X11 ones, says the manual. WM_CLASS has two halves, and
labwc matches the first, the instance. Steam's windows are instance
`steamwebhelper`, class `steam`. A rule for `steam` left the sign-in
window mapped, normal and focused; a rule for `steamwebhelper` put it in
the iconic, hidden state on its first map, checked every quarter second
from before the window existed. A control window (`zenity` under
`GDK_BACKEND=x11`, whose instance and class are both `zenity`) confirmed
that a reloaded rule with `Iconify` does apply to X11 windows. Both rules
ship in `session/labwc/rc.xml`; `steam` is kept for the bootstrapper's
window, which comes from the `steam` binary before the web helper exists
and was not captured today.

### Iconify or skipTaskbar: two different policies

`Iconify` hides the window and keeps its foreign-toplevel handle, so the
launcher hears of it and shows the grown-up screen. `skipTaskbar="yes"`
drops the handle: with it set, the same client restart hid the window
**and** the launcher showed the tiles as if nothing had happened. Both
were run. DESIGN §8.3 wants the grown-up screen, so `skipTaskbar` is not
set. It is the lever to reach for if the signed-in silent client turns
out to keep a hidden main window mapped at all times, below.

### The silent main window

In the Guardian's Plasma session the signed-in `-silent` client had a
main window titled "Steam" mapped in the **iconic** state (KWin's doing,
at the client's request), 2030 by 1252, hidden and skipped. If labwc maps
that window too, the launcher will show "Steam opened on its own" at every
login with nothing wrong. That is the first thing to look at once a
signed-in client runs in the kiosk. If it happens, the choices are
`skipTaskbar` for the client's main window by title, or the launcher
ignoring a window that maps straight into the hidden state. Neither is
decided here.

### Memory

| What | Where | Proportional set size |
|---|---|---|
| Kiosk alone: launcher, labwc, Xwayland | ada, before the client | 269 MB |
| Steam client, signed out, idle, sign-in window hidden | ada | 777 to 795 MB |
| Steam client, signed in, idle, `-silent` | guardian, Plasma | 820 MB |
| Whole L1 session with the signed-out client | ada | 1.11 GB |
| Whole Guardian session with the signed-in client | guardian, Plasma | 1.75 GB |

The machine sat at 1.3 GB used with the kiosk and the signed-out client,
and at 2.6 GB used plus 1.3 GB of swap with the Guardian's Plasma session
and the signed-in client. A game has about 2.5 GB left on the 4 GB floor
in the kiosk. A threshold for #34: the idle client under 1 GB and the
whole L1 session under 1.5 GB proportional, measured on the laptop. These
are VM numbers on llvmpipe; the laptop run is still owed.

### The launcher, under a real seat

Everything the launcher promised under nested labwc held under the real
session: fullscreen on map, the grown-up screen for a window nobody
launched, the tiles back when it closes, and the title in the sentence.
The one thing it cannot know is whether a hidden window is one a grown-up
needs to see; today every foreign window is.

## Remaining rows, and where

| Row | Where | Check |
|---|---|---|
| Signed-in `-silent` client in the kiosk | VM, the maintainer signs in as ada once | Does a main window map? What does the launcher show at login with nothing wrong? |
| `steam -applaunch` of a Humongous title, and back | VM | Window-based lifecycle (#42): the game's window over the launcher, tiles back when it ends. |
| `steam steam://open/main` signed in | VM | Hidden; the grown-up screen names it. |
| Pending client update, Steam Guard, login expiry, reboot | VM | What appears, whether the rule hides it, what the text says. |
| Child-role account: store, community, friends | Needs a child member in the Steam Family | Which surfaces the client locks on its own. |
| Idle memory on the Minimum-tier laptop | Laptop | The two thresholds above. |
| D3 | After the rows above | ADR naming labwc, with this file as its evidence. |

Screenshots and window logs from the run are in the maintainer's work
area, not the repository. They show the maintainer's Steam account.
