# ADR-0037: Wait while Steam updates itself, and tap again if it lost the tap

**Status:** proposed. It becomes accepted when a tap made while Steam
installs an update of itself has shown the starting screen on the kids'
laptop until the game, with no grown-up screen between, and a login during
which Steam updates itself has kept the tiles on the screen with Steam's
updater window hidden (#183).
**Date:** 2026-10-20; the at-login section 2026-10-30
**Closes:** (none) — amends ADR-0029 and ADR-0036; part of ROADMAP P1-13

## Context

ADR-0036 left one case open: Steam updating its own client at login. On
2026-10-20 at 21:31 a child account logged in on the kids' laptop, and
Steam began installing an update of itself that it had downloaded in the
background the session before. A tile was pressed 18 s after login. Steam
finished the update 21 s later and restarted. The restarted client never
took the tap: its log has no line about the game at all. The launcher
waited out ADR-0029's minute for Steam to start and showed the grown-up
screen. This time no game came later, because Steam no longer had it.

The laptop's logs from 2026-10-07 to 2026-10-20 show this is the rule, not
bad luck:

- Steam updated itself at login 5 times: 3 first installs of the client
  (about 5 minutes each) and 2 updates downloaded in an earlier session
  (39 and 55 s).
- 5 taps were made while Steam was downloading or installing an update of
  itself. All 5 were lost.
- 12 taps were made while Steam was starting with no update, or not yet
  running. Steam took all 12, 11 of them in the same second it ran its
  own start command.
- 81 taps were made with Steam already up. Steam took each one within 1 s.

The `steam` a tile runs hands its command to the running client through a
pipe and exits. Nothing queues the command across the updater's restart.

Steam's updater writes what it does to `logs/bootstrap_log.txt`:

```
[2026-10-20 21:31:33] Startup - updater built Sep  3 2026 01:31:48
[2026-10-20 21:31:33] Found pending update
[2026-10-20 21:31:33] Installing update...
[2026-10-20 21:32:10] Update complete, launching Steam...
[2026-10-20 21:32:10] Shutdown
[2026-10-20 21:32:10] Startup - updater built Oct 13 2026 18:44:25
```

The client writes `System startup time: 16.79 seconds` to
`console_log.txt` as it finishes starting. 0 to 21 s later it runs its own
start command, and it writes that and every command it takes, a tap
included:

```
[2026-10-20 21:32:29] ExecCommandLine: "'.../ubuntu12_32/steam' '-srt-logger-opened' '-silent'"
[2026-10-20 21:40:13] ExecCommandLine: "'~/.steam/root/ubuntu12_32/steam' '-applaunch' '2471160'"
```

## Decision

**While Steam is starting or updating itself, the launch waits and says the
game is getting ready. If the client that starts after the tap does not
take it, the launcher taps again, once.**

`SteamStartup` reads the last part of both logs. The client is **starting**
when the updater's last `Startup -` line is newer than the last time the
client ran its own start command. It is **updating** when the updater has also
written a line about downloading or installing an update since that
`Startup -`. A download while the client runs comes after the client has
started, so it does not count. A start older than 20 minutes that never
finished counts as no start: Steam was stopped or crashed.

Starting and updating are work, as in ADR-0036. The launch waits up to 10
minutes from the tap, the starting screen says "Getting it ready. This can
take a few minutes.", and the 2-minute limit for the game's window counts
from when the work ends. ADR-0029's fixed minute after login stays as a
floor for a Steam whose logs say nothing.

`SteamLaunchLog` now also reads two kinds of line after the tap:

- `ExecCommandLine:` naming `'-applaunch' '<appid>'` means Steam took the
  tap.
- `ExecCommandLine:` with `'-srt-logger-opened'` and no game means a client
  started after the tap and ran only its own start command.

A client that started after the tap and has not taken it is work too, so
the launch keeps waiting, with the sentence, until Steam takes the tap.
When a grace ends and that client has run its own start command at least
3 s before, the launcher runs the tile's command again. Steam takes a
held tap in the same second as its own start command, so 3 s is ample. It
does this once per tap. It never does it while a new reaper for the game
runs, or once the game's window has been up. The journal says "Steam
started again without taking Arthur's Reading Race; asking again."

## Consequences

- A child who taps a game right after login while Steam updates itself
  sees the starting screen and the sentence, then the game. On the laptop's
  update that is about a minute.
- The first install of the client on a new account takes about 5 minutes
  to download and up to 10 more to start. That fits inside the 10-minute
  work limit only on a quick line. Otherwise the launch ends on the grown-up
  screen, as before. Running Steam once for a new account before a child
  uses it is part of setting Steam up (#130).
- A tap sent again reaches a Steam that took nothing, so it cannot start
  the game twice. If Valve changes the `ExecCommandLine` wording, neither
  line matches, and the launcher never taps again: a lost feature, not a
  wrong one. If only the wording for the game changed, a second tap would
  reach a Steam already starting the game. On the laptop, 6 repeat taps of
  a game already starting or running each wrote that one line and nothing
  more.
- Both logs are read only when a grace ends, the last 256 KB of each.
- Steam starting now shows "Getting it ready" when it takes longer than the
  first 15 s grace. That was rare on the laptop: Steam started within 17 s
  of login at most logins.
- Tested in C++ with lines from the laptop's logs. An update, the restart
  and the started client each read as such. A background download while
  the client runs does not count, nor does a start that never finished long
  ago. A tap Steam took, a client started after the tap, and a client
  started with the game are each told apart. A lost tap is sent again
  once, never in the client's first moment, never with a reaper running.
  An `AppLauncher` test runs an update at login, the restart, the lost
  tap, the second tap and the game's window, with the sentence up
  throughout. Another has the game's window up, and nothing is sent
  again.

## Amendment: Steam updating itself at login, with no tap (#183)

### Context

On 2026-10-27 a child logged in on the kids' laptop. Before any tap, Steam
installed an update of itself that it had downloaded earlier, restarted,
and downloaded another 424 MB. Its updater put up windows of its own,
"Progress", "Steam setup" and "Steam", with a Cancel button. No launch was
under way, so `LaunchWindows` decided each had opened on its own, and the
launcher showed "Something needs a grown-up" with Steam's box on top. That
lasted about 13 minutes, and the child could press Cancel.

The window rules in `session/labwc/rc.xml` did not hide these windows,
though they hide every window whose X11 instance is `steam` or
`steamwebhelper`. Which instance the updater's windows carry is not known:
none was captured on the laptop. The dev PC's own Steam did the same pair
of updates the same evening, and its `bootstrap_log.txt` showed two things
the first version of this ADR missed:

```
[2026-10-27 23:06:09] Update complete, launching Steam...
[2026-10-27 23:06:09] Shutdown
[2026-10-27 23:06:09] Startup - updater built Sep  3 2026 01:31:48
[2026-10-27 23:06:09] Verifying installation...
[2026-10-27 23:06:16] Verification complete
[2026-10-27 23:06:27] Startup - updater built Sep  3 2026 01:31:48
[2026-10-27 23:06:27] Checking for update on startup
[2026-10-27 23:06:28] Downloaded new manifest: /steam_client_ubuntu12 version 1792618285, ...
[2026-10-27 23:07:21] Found pending update
```

- After an update the updater starts again at once to check itself, and
  Steam's start script starts it once more 17 or 18 s later, with no
  `Shutdown` between.
- A download at startup can write `Downloaded new manifest` and then
  nothing for almost a minute; another wrote `Downloading update...` in
  lower case.

### Decision

**While Steam is updating itself, a window that comes on the screen is its
updater's. It is neither the app's nor on its own, and the launcher asks
the compositor to hide it.**

- `SteamStartup` reads Steam as updating itself when its last start has
  written a line about downloading or installing an update (now also
  `Downloaded new manifest:` and `Downloading update...`), or when that
  start came no more than a minute after `Update complete, launching`.
  The second is a new state, Restarting. Either way it holds only until
  the client runs its own start command, as before. A start minutes after
  an update is plain Starting.
- `LaunchWindows` decides such a window is Steam's updater's. It keeps no
  launch waiting, opens no app, and brings up no grown-up screen. It stays
  Steam's until it closes, even if it is shown again after the update.
- The launcher asks the compositor to minimise it, through the same
  `wlr-foreign-toplevel-management` protocol it already listens to, and asks
  again if Steam shows it again. labwc honours the request. The window
  stays in the compositor's list, hidden, as a window rule's `Iconify`
  leaves it. The journal says "Steam is updating itself; its window
  Progress is hidden from the child."
- While a program other than a Steam game is launched, opening or open,
  the rule is off and its windows are decided as before. While it opens it
  owns every window on the screen, as ADR-0028 says. Once it is open, a
  dialog that belongs to one of its windows is its own, and a new window
  with no parent opened on its own and brings up the grown-up screen. A
  drawing app tapped during Steam's update is never hidden, so it can never
  sit stuck behind a dialog the child cannot see. A Steam game cannot draw
  a window while Steam updates itself, so during its launch the rule holds;
  even then a dialog that belongs to the game's window stays the game's.

So at login the child sees the tiles. A game tapped during the update
waits on the starting screen with "Getting it ready", as the first part of
this ADR says.

A window rule would hide the window before it is ever drawn, and would be
the plainer upstream mechanism. It is not used yet because the updater's
X11 instance is not known, and a rule matching titles such as "Progress"
would hide any program's window of that name. If a later update shows its
instance, a rule for it can join the two in `rc.xml`, and this part stays
as the fallback.

### Consequences

- At a login during which Steam updates itself the child sees the tiles,
  not the grown-up screen, and cannot reach Steam's Cancel button.
- The window is drawn for a moment, until the launcher hears of it and the
  compositor hides it. That moment was not measured; under nested labwc
  on the dev PC a screenshot 3 s after each window opened showed only the
  tiles. labwc's `ignoreFocusRequest` rule already stops it taking focus
  by asking.
- A window from anything else that opens with no launch, or during a Steam
  game's launch, while Steam updates itself is hidden too. Nothing else
  opens windows unasked in a child's session in those minutes.
- While a drawing app or another program that is not a Steam game is
  launched, a new window of the updater's is not hidden: with no way to
  tell it from the app's own window with no parent, the grown-up screen
  comes up for it, as before this amendment. A window of the updater's
  that was hidden before the tap stays the updater's and is hidden again
  if it comes back. An error box from Steam's updater is hidden
  as well; if the update fails, a game tapped afterwards ends on the
  grown-up screen when its wait ends, and the journal has the window's
  name.
- If Valve changed the wording of the client's start command, Restarting
  would last until the 20-minute limit, and a game's window in that time
  after an update would be hidden as the updater's. A changed wording
  already stops the tap being sent again, so both would show at once.
- Tested in C++. `SteamStartup` runs the dev PC's two updates in a row line
  by line, and a start minutes after an update. `LaunchWindows` has the
  three windows from the laptop's journal at the tiles, an updater window
  during a launch, and a window hidden during the update and shown after.
  `AppLauncher` has the login with no tap (the tiles stay, each window is
  asked to be hidden, again when shown again, and a window after the update
  still brings the grown-up screen), a Steam game tapped during the update
  (the starting screen and the sentence), and a drawing app tapped during
  it (its window is the app's). Checked by hand under nested labwc on the
  dev PC with a made-up `bootstrap_log.txt`: a Wayland window and an X11
  window, titled "Progress" and "Steam setup", were hidden and the tiles
  stayed; after the update a window opened on its own brought the grown-up
  screen. Not yet seen on a real Steam update in the kiosk.
