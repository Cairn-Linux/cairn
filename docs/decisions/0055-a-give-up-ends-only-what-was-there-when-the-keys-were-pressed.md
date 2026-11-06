# ADR-0055: A give-up ends only what began before the keys were pressed

**Status:** proposed. It becomes accepted when the checks under "Checks
in the VM" have passed on cairn-min.
**Date:** 2026-11-06
**Closes:** (none) — part of issue #287. Amends ADR-0018 (the give-up
key) and ADR-0026 (the file that tells the launcher).

## Context

Ctrl-Alt-Home runs `cairn-give-up` (ADR-0018).
It sends SIGCONT, then SIGTERM, waits two seconds, sends SIGKILL, and
then writes `$XDG_RUNTIME_DIR/cairn/give-up` so the launcher goes back
to the tiles (ADR-0026).

A game's window goes at the SIGTERM, in about half a second, so the
tiles are back well before the SIGKILL.
A child who has just been handed back the computer taps a tile at once.
On cairn-min on 2026-11-06, with a Steam game (Freddi Fish 2):
- A tile tapped 1 to 2 s after the keys (4 of 4 tries): the file was
  written at 2.05 s and the launcher forgot the new launch.
  The tiles came back while Steam was still starting the game, then the
  game opened over them, and the journal called it a window "opened on
  its own".
- GCompris tapped 0.95 s after the keys was ended by the SIGKILL at
  2.0 s, because the helper searched again for `bwrap` and for
  `cairn-app-*.scope` at the SIGKILL, and found the new app.

To the child, the tap did nothing, or the game came up wrong.
A tap 3 s or more after the keys worked every time (8 of 8).

The relaunch that failed on the kids' laptop on 2026-11-04 (#287) came
9 s after the keys, so it is not this; its cause is still open.

## Decision

**The helper ends only what began before the keys were pressed.**
- It notes the time first, on the clock behind `/proc/uptime`: time
  since the computer started, which a network clock sync cannot set
  back.
- It looks for the `bwrap` processes, the Steam reapers with their
  containers, and the `cairn-app-*.scope` and `app-flatpak-*.scope`
  units, before any signal, and again after the two seconds, just
  before the SIGKILL.
- Each time, it takes a process only if it began before the press, by
  its start in `/proc/<pid>/stat`, and a scope only if the oldest
  process in it did. A reaper's container is taken with its reaper,
  even if the reaper started it since.
- SIGCONT and SIGTERM go to what the first look found, SIGKILL to what
  both found, by process id or unit name.
- *Why look again:* an app the grown-up gave up on can still be
  starting at the press. `flatpak run` moves into its
  `app-flatpak-*.scope` before it starts the app's `bwrap`, and
  `systemd-run` takes a moment to register a `cairn-app` scope. Neither
  is there at the first look; both began before the press, so the
  second look ends them, and the app's window does not open later over
  the tiles.
- *Why not everything at the second look:* that is what the helper did
  before, and it ended the app of a tile the child tapped in those two
  seconds.

**The launcher is told when the keys were pressed.**
- The helper writes that time into the file, in place of the time it
  finished.
- `GiveUpWatch` reads it and passes it on. An empty file is the moment
  between the helper emptying the file and writing it, and is not a
  press. Anything else that is not a time counts as a press now, which
  gives up on everything, as before.
- `AppLauncher::giveUp` keeps a launch that began after that time, and
  says so in the journal: "<tile> was started after the give-up key, so
  it goes on."
  Any other launch, and every window, is forgotten as before.

## Consequences

- A tile tapped at any moment after the keys starts its app, and the
  tiles do not come back under it.
- A process id the helper found can end and be used again by a new
  process of the same child within the two seconds. The SIGKILL would
  then reach that new process. Linux hands out process ids in order up
  to `pid_max` (4194304 on Fedora), so this needs millions of new
  processes in two seconds. The Steam reapers were already signalled by
  process id.
- The launcher and the helper compare times on the boot clock
  (`CLOCK_BOOTTIME` in the launcher, `/proc/uptime` in the helper), so a
  change to the wall clock between a tap and the keys cannot decide
  wrongly. `/proc/uptime` has hundredths of a second, so the press can
  read up to 10 ms early; only a tap within 10 ms before the keys could
  be kept.
- A Steam game whose reaper had not started by the press, in the
  roughly 8 s Steam takes to start one, is in no scope of ours and is
  not ended. That was so before this decision too.
- A launch kept this way leaves the windows the launcher had before it
  as they were. A window still on the screen at a tap cannot be the old
  app's, since the child could reach the tiles.
- A game that quits by itself in its first seconds, as on the laptop,
  still leaves the starting screen up until the launch grace ends, with
  no word in the journal (#276). That is the rest of #287.

## Checks in the VM

On cairn-min with this change installed in a child's session:
1. A Steam game, Ctrl-Alt-Home, its tile tapped 1 s later: the game
   comes up, the starting screen stays until it does, and the journal
   says it "was started after the give-up key, so it goes on", never
   "opened a window on its own".
2. GCompris, Ctrl-Alt-Home, a Flatpak tile tapped 1 s later: it stays
   up past 3 s.
3. A SIGSTOP-frozen game and a frozen Tux Paint still end with
   Ctrl-Alt-Home, and the tiles come back.
4. Ctrl-Alt-Home with nothing running changes nothing.
5. A Flatpak tile tapped and Ctrl-Alt-Home pressed while its starting
   screen is up, before its window: the app never opens over the
   tiles, and `systemctl --user list-units 'app-flatpak-*'` shows its
   scope gone.

## Record

2026-11-06, cairn-min on `dev.20261106-be6bb57` (this change at 06a0033,
merged with #290 and #291; ctest 104 of 104 in the image build). Every
"tap" was Enter on the focused tile: the USB tablet's clicks did not
reach the launcher in that boot.
1. Passed, 5 of 5: Freddi Fish 2 started 0.53, 1.03 (twice), 1.53 and
   2.03 s after the keys came up by +3 to +6 s and was still up at
   +20 s, and the journal said "goes on" each time. In only two rounds
   (0.53 s and the first 1.03 s) was the new game already running
   while the helper took its second look; the other three tested only
   the launcher's rule.
2. Passed, 4 of 4: GCompris started 0.53 to 2.03 s after the keys, and
   each new copy was alive during the second look and kept.
3. Passed: the frozen Steam game was gone by +1 s with Steam's client
   left up; the frozen Tux Paint was killed at 2 s.
4. Passed: no process or unit changed.
5. Passed: keys 0.05 and 0.15 s after Enter ended `flatpak`, `bwrap`
   and `gcompris-qt` within 0.1 s, and no `app-flatpak-*` scope was
   left. But all three were already running, so the first look found
   them. An app not there yet at the first look, the case the second
   look exists for, was not made to happen.
The old app or game was gone 0.17 to 0.51 s after the keys in every
round. The ADR stays proposed: the second look's own case, and a real
tap, are not yet seen.
