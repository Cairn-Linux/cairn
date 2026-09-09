# Steam containment: what the kiosk does with the client's own windows

**Date:** 2026-09-08
**For:** ROADMAP P0-10 (issue #25), the premises in #34, and D3
**Result:** With one labwc window rule, every window the Steam client
opened in the L1 session stayed off the screen from its first frame, and
the launcher showed "Something needs a grown-up" naming it. Without the
rule the sign-in window sat centred on top of that screen. The rule has
to match the WM_CLASS *instance*, `steamwebhelper`; matching the class
`steam` did nothing. The signed-in rows ran the same evening after the
maintainer signed in as ada: the silent client maps no window at all, so
the child sees the tiles at login; a forced main window goes straight to
the hidden state and the grown-up screen names it; a tile running
`steam -applaunch` brings the game up and the tiles are back within two
seconds of it quitting; the login survives a reboot. The client is about
800 MB proportional signed out and 1.4 GB signed in. Left: the child-role
rows, which need a child member in the Steam Family, and the laptop run.

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
| Client starts with `-silent` and shows no window | **Pass, signed in** (evening run) | `cairn-session` does not start the client yet, so it was started by hand as ada with the session's environment, `/usr/bin/steam -silent`, bypassing the wrapper. Signed in, the client connected in one second, `_NET_CLIENT_LIST` stayed empty and the launcher showed the tiles: unlike KWin, labwc gets no main window at all from a silent client, so the worry under "The silent main window" does not arise. Signed out, the same command opened the sign-in window, which the rule hides. |
| The first thing on screen is Bazzite's, not Steam's | **Fail, mitigation named** | Within ten seconds of the wrapper starting, a KDE dialog appeared: "Steam is opening for the first time. Please wait, downloading the Steam client may take a few minutes." It is a native Wayland client titled "Steam", so no `WM_CLASS` rule reaches it, and the wrapper waits for it to be dismissed before starting the client at all. The launcher put up "Something needs a grown-up. Steam opened on its own." under it, with the dialog centred on top. The session should run `/usr/bin/steam -silent` directly, or pass `--disable-firstrun`, and never go through the wrapper; recorded for P1-13. |
| `steam -applaunch <appid>` brings a game up and returns to the launcher | **Pass, signed in**, with #42 on show | A test tile `["/usr/bin/steam", "-applaunch", "283920"]` (Putt-Putt Joins the Parade, copied into ada's library from the Guardian's) was pressed at the kiosk. `steam -applaunch` exited 0 at once, so the launcher went back to Idle; 31 s later the game's window (`WM_CLASS` `scummvm`, title "Putt-Putt Joins the Parade") mapped fullscreen over the launcher, which by its own rules then treated it as a window nobody launched. Ctrl-Q in the game ended it and the tiles were on screen within 1.5 s. The 31 s were Steam fetching the 673 MB Steam Linux Runtime 2.0 (soldier) the title depends on, with nothing on the child's screen until a Steam toast said "Download Complete". Signed out, the same command returned 0 with nothing visible. |
| Force the client window open: `steam steam://open/main` | **Pass**, signed out and in | Signed out, the command produced no new window and did not un-hide the sign-in window. Signed in, it mapped the main window ("Steam", `steamwebhelper`, type NORMAL) straight into the iconic state and the launcher showed "Something needs a grown-up. Steam opened on its own." The window never closes by itself, so that screen stays until a grown-up ends the client (`steam -shutdown` brought the tiles back). |
| Force the client window open: a pending client update | **Not reproducible today** | The bootstrap log ended "Download skipped: version 1788652215, installed version 1788652215 … Nothing to do". The bootstrapper's own window, if it showed one, was not captured because of the root `xprop` mistake above. |
| Force the client window open: a signed-out client | **Fail, then pass** | The sign-in window ("Sign in to Steam", `WM_CLASS` `"steamwebhelper", "steam"`, type NORMAL, 700 by 440, centred, focused) mapped on top of the grown-up screen. With the rule below it mapped straight into the iconic, hidden state at t = 3.6 s after the client started and stayed there for the rest of the run, through `steam://open/main` and `-applaunch`. The child sees "Something needs a grown-up. Sign in to Steam opened on its own." and nothing else. |
| The launcher learns of the window and shows the grown-up screen | **Pass** | For the KDE dialog and for the sign-in window, hidden or not. The text names the window's title. |
| A window that slips through is locked by a PIN | **Not applicable** | There is no PIN. What remains is the child role in a Steam Family, which Valve sets on the account, on the web, not on this machine. Whether a child-role account's client hides the store, community and friends has not been checked and needs a child member in the family. |
| Steam Guard and login expiry: what the child sees | **Half recorded** | The signed-out state is the sign-in window: account name, password, "Remember me" ticked, a QR code, "Help, I can't sign in" and "Create a Free Account" links. With the rule the child never sees it; the grown-up screen names it. The maintainer signed in at the VM's console with the rules lifted; whatever Steam Guard asked was answered there and left no line in the client's logs. Login expiry cannot be forced. |
| Idle memory of the client against the 4 GB floor | **Recorded, see below** | About 800 MB proportional signed out; **1.36 to 1.41 GB signed in**, silent, idle, over 16 processes, of which two `steamwebhelper` renderers are 400 to 500 MB each. With it resident the VM has 1.9 GB of 3.9 GB available at the tiles. On this VM, not the laptop. |
| Reboot, client update, Steam Guard persistence (#34) | **Reboot: pass** | `systemctl reboot`, Enter at the greeter, the client started silently by hand: "Logged On" one second later, no window, tiles on screen. A client update could not be forced (the bootstrapper found nothing to do). |
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
at the client's request), 2030 by 1252, hidden and skipped. The worry was
that labwc would map it too and the launcher would show "Steam opened on
its own" at every login. It does not: in the kiosk the signed-in silent
client creates no X11 window at all (`_NET_CLIENT_LIST` empty, checked as
ada, 45 s after start and again after a reboot) and the launcher shows the
tiles. The window only exists once something asks for it, and then the
rule hides it and the launcher names it, which is the designed outcome.

### What `steam -applaunch` looks like to the launcher (#42)

The exit status says nothing, as the signed-out run already showed. Signed
in, the sequence from a tile is: the process exits 0 within a second, the
launcher's five-second settle window treats a clean exit as a finished
program and goes back to Idle, then the game's window maps and the
launcher, seeing a window it did not launch, would show the grown-up
screen under it. The game is fullscreen on top, so the child sees the
game; when it quits the launcher is Idle again and the tiles are back
within two seconds. That is workable by accident and P1-13 should make it
deliberate: a Steam tile needs a window-based lifecycle (the tile is
"running" while a window with the game's class exists) rather than a
process-based one.

Two more things this run put on the child's screen. Steam draws its own
**toast** ("Download Complete, Steam Linux Runtime 2.0 (soldier), Your
game is ready to play") at the bottom right, over the game, and it was
not in the X11 client list afterwards, so a WM_CLASS rule may not reach
it; not yet chased. And a first launch of a title can mean a **download
the child cannot see**: 31 s and 673 MB here before anything happened,
with the tiles sitting still. Both belong to P1-13 alongside the wrapper's
first-run dialog.

### Memory, signed in

`steampss.py` summed `Pss` over every process matching the client: 1284 MB
just after the main window was forced open, 1358 MB idle and silent one
minute after a restart, 1408 MB idle and silent after the reboot, against
about 800 MB signed out. Signed in, the client keeps two web renderers of
400 to 500 MB each warm even with no window. With the client resident the
L1 session idle is about 1.7 GB in total and `MemAvailable` reads 1.9 GB
of 3.9 GB; the VM began to swap by a few tens of MB during the run. The
laptop numbers are still owed, and P1-13 owns the budget.

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
| Pending client update, Steam Guard prompt, login expiry | VM, when Valve obliges | What appears, whether the rule hides it, what the text says. None could be forced today. |
| Child-role account: store, community, friends | Needs a child member in the Steam Family | Which surfaces the client locks on its own. |
| Steam's toast over the game | VM, P1-13 | Which surface draws it and whether a rule or the launcher can keep it off a child's screen. |
| Idle memory on the Minimum-tier laptop | Laptop | The two thresholds above, now against 1.4 GB signed in. |

D3 closed with ADR-0017 on the strength of this file and
`kiosk-containment.md`.

Screenshots and window logs from the run are in the maintainer's work
area, not the repository. They show the maintainer's Steam account.
