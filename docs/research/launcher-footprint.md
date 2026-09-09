# Launcher footprint: memory and latency of the L1 session

**Date:** 2026-09-08
**For:** ROADMAP P0-5 (issue #4), ADR-0003
**Result:** The launcher idles at about 145 MB proportional, the whole L1
session at about 235 MB, and the machine has 2.9 GB of its 4 GB free with
the child at the tiles. With Tux Paint or GCompris up it still has
2.7 GB. The launcher draws its first frame 0.2 s after exec, the tiles are
on screen 1.75 s after Enter at the greeter, and a launched app is back to
the tiles 0.2 s after it exits. Tux Paint takes 6.4 to 6.9 s from the tile
to a usable screen on this VM; GCompris 2.1 to 2.2 s. Measured in the VM,
not on the laptop, so every number is an upper bound on memory and no
number is a promise about speed.

## How it was run

The Bazzite VM `cairn-min` (3895 MB, 2 vCPUs of an Intel Core Ultra 9
185H, virtio GPU on llvmpipe with Mesa 26.2.1, Bazzite 44.20260902,
labwc 0.9.6, Qt 6.11.1), provisioned by `provision/cairn-provision.sh`.
The launcher is the release build of main at `8a111d9`, byte-identical to
the one installed at `/usr/local/libexec/cairn/cairn-launcher`. Apps are
the Phase 0 Flatpaks: Tux Paint 0.9.35 on the freedesktop 25.08 runtime,
GCompris 26.1 on the KDE 6.11 runtime.

Memory is proportional set size (`Pss` from `/proc/PID/smaps_rollup`) for
every process under `user-1002.slice`, the child's slice. That matters:
the session scope holds labwc, the launcher and `sddm-helper`, but each
Flatpak app runs in a scope of its own beside it, and the first Flatpak
launch also starts pipewire, wireplumber and the portals under the user's
`systemd --user`, which then stay. A first pass that read only the session
scope missed all of that. `MemAvailable` is from `/proc/meminfo`.

Latency is wall clock on the host: `virsh send-key` for the keypress and
`virsh screenshot` about four times a second until a pixel that belongs to
the expected screen appears (Tux Paint's green Tools button, GCompris's sky,
the launcher's Sand background). The 0.25 s poll is the resolution. The
launcher's own start was measured under a second, headless labwc as the
child, with `WAYLAND_DEBUG=1` on the launcher and the exec time written
beside it; "first frame" is the first `wl_surface.commit` after a buffer was
attached.

Everything here is software-rendered. On the Minimum tier's Intel HD 4000
the GPU does the drawing and the scene-graph buffers live in GEM objects
rather than in the process's heap, so the launcher's number should be
lower there and the app numbers different; the laptop run is still owed.

## Memory

Proportional set size in MB. "Session" is every process in the child's
slice. Idle means sixty seconds without input.

| State | launcher | labwc | app | session total | MemAvailable |
|---|---|---|---|---|---|
| Fresh login, idle 60 s | 144.5 | 73.7 | — | 235 | 2925 |
| The same session after hours idle (the P0-10 session) | 146.3 | 75.5 | — | 230 (session scope only) | 2952 |
| Terminal tile open (Footpath) | 146.6 | 75.5 | — | 231 | 2944 |
| Back from the terminal | 146.7 | 75.5 | — | 231 | 2946 |
| Tux Paint running, 20 s | 142.0 | 78.8 | 235.6 | 549 | 2722 |
| Back from Tux Paint, 5 s (Xwayland still up) | 146.6 | 79.2 | — | 305 | 2876 |
| GCompris running, 25 s | 146.2 | 86.7 | 303.8 | 595 | 2739 |
| Back from GCompris, idle 60 s | 146.6 | 79.2 | — | 284 | 2900 |
| Launcher alone, nested headless, 15 s after exec | 95 | — | — | — | — |

Resident set size is larger and less honest: the launcher's `Rss` is
197 to 201 MB against a `Pss` of 145, and 113 MB of it is private. The
in-session launcher is 50 MB heavier than the same binary started under
the headless compositor; the fullscreen 1280 by 800 surface and its
swapchain account for some of that and the rest was not chased.

What stays behind after the first Flatpak launch, and never leaves:
wireplumber (17 MB), pipewire and pipewire-pulse (14 MB), and the
portals, document portal and dbus proxies (about 6 MB). That is the 50 MB
gap between "fresh idle" and "back from GCompris". An Xwayland (33 MB)
comes with Tux Paint, which is an X11 client in its Flatpak, and goes
away a few seconds after it. None of it is the launcher's.

The launcher did not grow: 146.3 MB after hours, 146.6 MB after the
terminal and four launch-and-return cycles.

Against the floor: the whole L1 session with one app running leaves
2.7 GB of 3.9 GB. Add the Steam client's 800 MB from
`steam-containment.md` and it is still above 1.9 GB. Memory is not the
constraint on the Minimum tier; the software renderer's CPU time is the
thing to watch on the laptop.

## Latency

| From | To | Seconds |
|---|---|---|
| `exec` of the launcher | first Wayland message | 0.026 to 0.036 |
| `exec` of the launcher | xdg toplevel created and configured | 0.12 to 0.14 |
| `exec` of the launcher | first frame committed | 0.21 to 0.23 |
| Enter at the SDDM greeter | tiles on screen | 1.75 |
| Enter on Draw | something on screen (Tux Paint's white window) | 0.49, 0.54 |
| Enter on Draw | Tux Paint's tools drawn | 6.91, 6.38 |
| Enter on Practice | something on screen (GCompris's splash) | 0.87 |
| Enter on Practice | GCompris's home screen | 2.11, 2.24 |
| App killed | tiles back on screen | 0.16 to 0.19 |

Three runs of the launcher's own start agreed to within 20 ms. The greeter
number includes PAM, SDDM's session start, labwc and the launcher.

Tux Paint's six seconds are the number to remember for the child test:
half a second of white, then nothing changes for six seconds while SDL
and the stamps load under llvmpipe. That is the "taps an icon and stares
at nothing" case DESIGN §4.2 worries about, and the launcher does nothing
during it: the tiles stay up with the focus ring where it was. Whether the
laptop's GPU makes that short enough, or the launcher needs a "starting"
state, is a child-test question (P0-9).

## Things the run showed

- **The five-second settle window is real.** GCompris killed 4.5 s after
  the tile press showed "Something needs a grown-up" with a Back tile,
  because the launcher treats an exit within five seconds as a failed
  launch. Correct by design, and worth knowing when driving tests by hand.
- **Xwayland is lazy both ways.** It is not in the session until the
  first X11 client asks for it, and it exits once the last one is gone, so
  an idle kiosk has none.
- **`memory.current` on the slice is not memory.** It read 848 MB after
  GCompris had exited, because the cgroup keeps the page cache of the
  Flatpak runtimes it touched. `Pss` and `MemAvailable` are the numbers to
  quote.
- **The launcher is not the cost.** The compositor is half its size, the
  apps are twice it, and the Steam client is five times it.

## Left for the laptop

The same table on the Minimum-tier machine, once one exists: memory with
GPU rendering, Tux Paint's time to a usable screen, and CPU while idle,
which was not measured here and which llvmpipe would misreport anyway.
