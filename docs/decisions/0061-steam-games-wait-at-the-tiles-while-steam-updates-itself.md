# ADR-0061: While Steam updates itself, its games' tiles fade and build stones, and a press waits at the tiles

**Status:** proposed. The maintainer chose B on 2026-11-21, with the
stones over the middle of the tile, building. Two steps come before it
is accepted.
(1) On this branch, before it is merged, children try it in a staged
session on a test build with a made-up update, S1 to S3 under "Checks".
The maintainer decides on what the children did.
(2) Once merged, V1 to V8 pass in the cairn-min VM on an image built
with it, and K1 passes on the kids' laptop at a real update of a
child's Steam.
**Date:** 2026-11-21 (first drafted 2026-11-20)
**Closes:** (none). Answers the maintainer's request of 2026-11-20.
Part of ROADMAP P1-13.
**Amends:** ADR-0028 (a second use of the building stones, on a tile;
a press on a waiting tile is answered by the stones starting again, not
by the starting screen); ADR-0035 (a game's picture is drawn faded
while Steam updates itself); ADR-0037 (the launcher reads whether Steam
is updating itself while the tiles are in front, not only during a
launch, and a game's tile pressed then opens nothing). Each of the three
gains an "Amended by: ADR-0061, proposed" line.
**Changes in the same change:** DESIGN §6.1, §6.2 and §8.3; the overlay,
motion and Pebble notes in `brand/tokens.json`; `docs/TRY.md`; one
sentence on Family's Games page (`guardian/qml/GamesView.qml`);
`docs/research/child-test-02.md` (entry 19, its source row and a
question); ROADMAP §0 and P1-13.

Line numbers in this ADR are from main at 4879a47 (2026-11-20), the
commit this change starts from.

## Context

The maintainer asked on 2026-11-20: "I would also suggest steam game
tiles showing some indication that they are being updated if steam is
being updated and possibly making them unable to click till it is
done? I am not sure how to make this clear to a child who cannot read
yet though."

His goal for Steam on Cairn, the same day: "The goal is to make it as
easy as possible for parrents who come from windows or mac and don't
know how to troubleshoot these types of issues".

The first draft of this ADR offered two answers.
A: a small chip of building stones in each Steam tile's top-right
corner, and a press still opens the game and waits.
B: the same chip, the tile's picture faded, and a press opens nothing.
The maintainer chose on 2026-11-21: "Yeah let's do B but lets put the
stones over the middle of the tile and have them animate."

### What a child sees today

- A tap on any tile shows the starting screen at once. The Cairn mark
  builds itself, stone by stone, over "Putt-Putt Joins the Circus is
  starting." (ADR-0028:41-53).
- While Steam works on a game, or updates itself, the launch waits.
  After the first 15 s grace a second sentence says "Getting it ready.
  This can take a few minutes." (ADR-0036:76-82, ADR-0037:78-82).
- The launch gives up about 10 minutes after the tap
  (`launcher/src/SteamGameWait.h:188-192`). The child then sees
  "Something needs a grown-up." From that moment the grace timer is
  stopped (`launcher/src/AppLauncher.cpp:411-422`), so a tap Steam lost
  is never sent again (`AppLauncher.cpp:488-495`). The game comes only
  if Steam still had the tap (ADR-0030). The child can press Back.
- Keys and taps on the starting screen do nothing (ADR-0028:52-53).
  The child's leave key does nothing there either: `CloseRequest`
  answers Stay (`launcher/src/CloseRequest.cpp:33`). Only the grown-up's
  give-up key brings the tiles back (ADR-0026;
  `launcher/qml/Main.qml:61-65`).
- Before a tap, every Steam tile looks the same, whether Steam is
  ready or busy.

### When Steam updates itself

Each child's Steam is their own. It updates itself the first time it
runs after Valve ships a client update, and that is at the child's
login (ADR-0027; ADR-0048:22-23). It does not happen at every login.

- From 2026-10-07 to 2026-10-20 it happened at login 5 times on the
  kids' laptop: 3 first installs of about 5 minutes each, and 2
  installs of an update downloaded the session before, of 39 s and
  55 s (ADR-0037:30-32). The restarted client then took about 18 s more
  (`launcher/src/SteamStartup.h:22-24`). So on the laptop a tile would
  wait for about a minute at a login with an update, once the child's
  Steam was installed, and about 5 minutes at the child's first.
- All 5 taps made while Steam downloaded or installed an update of
  itself were lost. The `steam` a tile runs hands its command to the
  running client and exits, and nothing queues the command across the
  updater's restart (ADR-0037:33-34, :40-41). Steam took all 12 taps
  made while it only started (ADR-0037:35-37). ADR-0037 sends a lost
  tap again, once, after the restarted client has started.
- On 2026-10-27 one child's Steam installed an update, restarted and
  downloaded 424 MB more. Its updater was busy for about 13 minutes
  after the 17:58 login (ADR-0037:136-142; ADR-0036:514-516). The
  child's first tap came at 18:12, after the update, and then waited 2
  more minutes for two tools (ADR-0036:515-523). A tap in about the
  first 3 minutes of those 13 would have reached the 10-minute limit
  and ended on the grown-up screen with no game.
- A first install of the client took 15 minutes on the laptop's Wi-Fi
  (`launcher/src/SteamStartup.cpp:18-21`).
- No Steam game can start while the client updates itself. A tile that
  does not start in Steam is not held up: a drawing app tapped during
  the update opens as usual (ADR-0037:192-200).

### Which tiles are Steam tiles, and where they sit

- Today every game tile starts in Steam. The games file accepts only
  the command `steam -applaunch <appid>`
  (`launcher/src/GamesFile.cpp:20-34`). Draft ADR-0059, not decided,
  would move the Humongous games to ScummVM. Their tiles would then not
  wait for Steam.
- The game tiles follow the manifest's tiles
  (`launcher/src/TileModel.h:25-27`): Draw, Music, Build, Story and
  Practice (`provision/manifest.json`). On the laptop's 1366x768 screen
  the grid shows two whole rows and the top 131.6 px of a third
  (ADR-0015; `Main.qml:222-224`, `:261`). So at login the first screen
  shows five tiles that never wait for Steam, one whole game tile, and
  the tops of three more.

### What the launcher can know before a tap

- **Whether Steam is updating itself.** `SteamStartup::read(now)` keeps
  no state (`SteamStartup.cpp:47-82`). Each call reads the last 256 KB
  of `bootstrap_log.txt`. It reads `console_log.txt` only within 20
  minutes of the updater's last `Startup -` line
  (`SteamStartup.cpp:64-69`). Its answers are Ready, Starting, Updating
  and Restarting. Updating and Restarting are the long ones
  (`SteamStartup.h:49-51`).
  - A plain Starting ended within 17 s of login at most logins on the
    laptop (ADR-0037:118-120). On the first laptop the client took a
    game up 20 s after it started (`SteamGameWait.h:179-181`).
  - Restarting holds until the client runs its own start command
    (ADR-0037:176-182), so an update, its restart and the start read as
    one stretch.
  - A start more than 20 minutes old reads Ready
    (`SteamStartup.cpp:64-66`), even if the updater is still
    downloading. Only a `Startup -` line sets that clock. Whether the
    updater writes lines during a long download is not confirmed.
    `SteamStartup.cpp:14-16` says a first-time download writes a line
    per few hundred kilobytes; ADR-0037:166-168 says a download at
    startup wrote nothing for almost a minute.
  - Today `SteamStartup` is read only during a launch and when a window
    opens (`AppLauncher.cpp:256-266`). Its answer never reaches QML.
- **Whether a Steam is running at all.** A log left part-way through an
  update by a Steam that was stopped reads Updating for up to 20
  minutes. `SteamReapers` already reads `/proc` for processes of this
  user by name (`launcher/src/SteamReapers.h:15-18`). `SteamRestart`
  asks systemd about `cairn-steam` (`launcher/src/SteamRestart.h:36-37`),
  but a Steam that a tap started runs outside that service
  (`SteamRestart.h:19-22`).
- **Whether one game is being updated now.** Steam writes it to
  `content_log.txt`: a `state changed` line for the game with both
  `Update Required` and `Update Running`, until `finished update`,
  `update canceled` or `scheduler finished`. On the dev PC, Steam
  updated games played in the last 3 days within seconds of starting.
  For older games it wrote `Update delayed for N secs`, and a tap made
  the update due. No update of a storybook game itself has been seen on
  the laptop. Nothing in the launcher reads this file before a tap
  (`launcher/src/SteamContentLog.cpp:46-54` starts at the tap).
- **Not knowable: an update the tap itself makes due.** Valve updates a
  game not played for 3 days only when someone asks to play it
  (ADR-0048:39-47). A tool such as Proton or a Steam Linux Runtime is
  marked "Update Required" only at the tap (ADR-0046:82-84). The
  laptop's waits for a game were of this kind (ADR-0036:28-35).

### What a child who cannot read can take from a picture

- **The building stones** are the picture a child sees at every tap,
  even when a game opens in about 3 s (ADR-0028). So what the stones
  have taught so far is "this is starting", not "this will take a long
  time". The boot screen would use them too if ADR-0044 is accepted
  (ADR-0044:129-130).
- **What the research shows, and what it does not.** Hiniker and
  colleagues watched 34 children aged 2 to 5 with symbols such as
  progress bars ("Hidden symbols", Int. J. Human-Computer Studies 90
  (2016) 53-67, https://faculty.washington.edu/alexisr/HiddenSymbols.pdf).
  The progress bar filled in answer to the child's own tap on a faucet,
  and a researcher explained the task first (§4.2.1, p. 58). All the 4-
  and 5-year-olds managed it, and decoration was "an enormous barrier"
  (p. 64). Their advice is to point out the link between a symbol and
  what it stands for, or say it out loud, and to keep the two looking
  alike (p. 65). Stones on a tile are a symbol for something the child
  did not do, and Cairn neither shows the link nor says it. So the
  paper does not show that children will read it as "wait". Only
  watching children can (S1 to S3).
- **A faded tile with a progress mark is a known look.** On an iPhone
  or iPad, an app that is updating has a greyed icon with a progress
  ring, and a tap pauses the download rather than opening the app
  (AddictiveTips, "Fix a stuck app update on iOS",
  https://www.addictivetips.com/ios/fix-a-stuck-app-update-on-ios;
  Engadget, 2014-06-23, on the ring and the pause,
  https://www.engadget.com/2014-06-23-iphone-101-icons-used-to-represent-app-download-status.html).
  Many 5-year-olds and most parents know that look. No source found
  says a faded tile reads as broken to a child.
- **A faded picture hides what a child finds a game by** (ADR-0035).
  Until this change the only faded look in the brand was for "a
  grown-up's window only" (`brand/tokens.json:79-85`).
- A padlock says a grown-up said no. Hiniker and colleagues name the
  hourglass as a symbol a young child "will struggle" with (p. 65).
- **For a parent**, a Cairn mark on a game's picture may read as a
  Cairn badge, "approved" or "new", not "updating". The marks parents
  know for an update are rings and bars.

## Decision

**While Steam is updating itself, every tile that starts a game in
Steam waits. Its picture is faded, and the Cairn stones build in a
small chip over the middle of the tile, as on the starting screen. A
press on a waiting tile opens nothing: the stones start again from the
bottom. When Steam is done, the tile looks as before and opens as
before. A tile that does not start in Steam never waits.**

This is option B of the first draft, with the stones moved to the
middle of the tile. Option A is under "Alternatives considered".

### When

- A tile waits while two things are true:
  - `SteamStartup` reads Updating or Restarting. A plain Starting, a
    Ready, or a start more than 20 minutes old does not count.
  - A Steam of the child's is running: a process of this user whose
    name in `/proc` is exactly `steam`, read the way `SteamReapers`
    reads reapers. `/proc` is read only while the logs say Steam is
    updating. V8 checks that the updater runs under that name.
- A plain Starting does not bring it. So the usual start at login does
  not fade and then restore every Steam tile within 20 s.
- Restarting lasts until Steam is up, so a tile does not stop waiting
  and start again between an update, its restart and the start.
- Only tiles whose command starts a game in Steam wait. That is decided
  once, when the tiles are read, from `SteamLaunchLog::appIdOf(exec)`
  (`launcher/src/SteamLaunchLog.h:91`). Today that is every game tile.
  Draw, Practice and other apps never wait, nor the Terminal tile, Log
  out, or the grown-up screen's Back tile. If the maintainer agrees to
  draft ADR-0059, a game that starts in ScummVM never waits.
- One game's own update does not make its tile wait yet (Alternatives
  considered).

### What it looks like

- **The picture** of a waiting tile is drawn at `Tokens.coveredOpacity`
  (0.35). A tile with no picture, or whose picture could not be drawn,
  shows its title at that strength instead. The card, its edge and the
  focus ring stay at full strength.
- **The chip** sits over the middle of the tile, centred on it.
  - It is the Cairn mark with its clear space: one base-stone height on
    every side (`brand/tokens.json:186`), so 120 by 114 units of the
    mark's box.
  - It is half the starting screen's mark: `Tokens.displaySize * 3 / 2
    / Tokens.markHeight` px per unit. The starting screen uses
    `displaySize * 3 / markHeight`
    (`launcher/qml/StartingScreen.qml:56`). The chip is about 79 by
    75 px. Each stone is about 10.5 px tall, and the bottom one 58 px
    wide.
  - Its fill is Pebble (`Tokens.pebble`), with `radiusSm` corners and no
    edge.
- **The stones** are the mark's, in Ink (`Tokens.text`). Ink on Pebble
  is 9.99:1.
- **The motion** is the starting screen's: one stone more each
  `motionStone` (500 ms), bottom first, then the whole mark for one
  step and no mark for one. Every change is a fade over `motionStone`
  (`StartingScreen.qml:21-25`, `:40-45`, `:77-83`).
  - The stones and their building move out of `StartingScreen.qml` into
    `BuildingStones.qml`. The starting screen and the tile both draw
    it, each at its own size, so the two can never differ. The starting
    screen looks and moves exactly as before.
  - One clock drives every waiting tile, so the grid shows one thing
    building, not many things out of step.
  - The clock runs only while at least one tile waits and the tiles are
    in front.
- The picture's fade and the chip's coming and going each take one
  `motionStone`.
- There are no words on the tile.
- **The partly shown row.** On the laptop's screen the third row shows
  only its top 131.6 px, so a centred chip is cut at the bottom there.
  That is a known cost. The faded picture still marks those tiles, and
  scrolling shows the whole tile. The chip does not move to dodge the
  cut.
- **A tile that shows its title.** On a game tile with no picture, or
  whose picture failed, the chip covers the middle of the faded title
  while Steam updates itself. That is a known cost too. The tile's
  `Accessible.name` is the whole title.
- **Screen readers:** the chip has `Accessible.ignored: true`. The
  tile's `Accessible.name` stays as it is. While the tile waits, its
  `Accessible.description` is "Waits for a Steam update. It opens when
  Steam is done." (`qsTr()`).
- No new colour, size or duration. The notes in `tokens.json` gain the
  new uses: the overlay note (`:84`) gains a Steam game's tile while
  Steam updates itself, the motion note (`:153`) gains "and on a Steam
  game's tile while Steam updates itself", and Pebble's role (`:35`)
  gains the chip on a waiting game tile.

### A press on a waiting tile

- A press is a tap, a click, Enter, Space, or a screen reader's press
  action.
- A press on a waiting tile opens nothing and changes no screen.
- It is answered at once: the shared clock starts again from no stones,
  so every waiting tile's stones build again from the bottom, together.
- C++ decides. A tile's press goes to `AppLauncher::press(title, exec)`
  in place of `launch()`.
  - If the command starts a game in Steam and the watch says Steam is
    updating, `press()` first asks the watch to read again at once.
  - If Steam is still updating, it refuses. Nothing is launched. The
    journal gets one line, at most once per game per update. It emits
    `heldForSteam`, which `Main.qml` answers by starting the clock
    again.
  - If that fresh read says Steam is done, the watch turns off, the
    tiles return to normal, and the launch goes ahead as today.
  - Otherwise `press()` calls `launch()` as today.
- The decision uses the same answer the tiles draw. A tile that looks
  normal never refuses. A faded tile opens only if that fresh read has
  just ended the wait.
- The Terminal's `open` keeps calling `launch()` and is not refused: a
  Steam game opened there during an update waits on the starting
  screen, as today (`Main.qml:84`). The Terminal tile still opens the
  Terminal.

### When it ends

- Within 5 s of Steam reading Ready, of the last start passing 20
  minutes, or of no Steam running, every waiting tile stops waiting. The
  chip fades out and the picture returns to full strength, each over
  one `motionStone`.
- A press on a waiting tile can end it sooner, by its fresh read.
- No sound, no tick, no "Ready", no highlight. The tile looks like its
  neighbours again.

### What gets built

The names here are the ones the change is built with. If the code
needed another, the code's name is the one that counts, and this list
is brought in line before the ADR is accepted.

- `SteamUpdateWatch` (`launcher/src/SteamUpdateWatch.h` and `.cpp`), a
  small C++ class exposed with `QML_ELEMENT`.
  - `active` (`Q_PROPERTY bool`, read and write) is bound in `Main.qml`
    to the tiles being in front: the tile window is shown
    (`Main.qml:238`) and `launcher.state` is Idle. So it is false during
    a launch, while an app runs, on the grown-up screen, in the
    Terminal and at the Log out question.
  - While not active, it reads nothing and keeps its last answer. When
    it becomes active, it reads at once. While active, it reads every
    5 s.
  - `updating` (`Q_PROPERTY bool`, read only, with `updatingChanged`)
    is the answer, as under "When".
  - `processRoot` (`Q_PROPERTY QString`, `/proc` unless set) is where
    it looks for a `steam` process. The tests give it a process list of
    their own.
  - `holdsBack(title, exec)` makes the fresh read at a press and writes
    the "did not open" line, at most once per game per update. It
    answers whether the press is refused.
  - It writes to the journal only when `updating` changes, the way the
    launcher already writes journal lines. Each line uses the times in
    Steam's own logs, so a pause never repeats a line or stretches one:
    - When it turns on: "Steam started updating itself at HH:MM:SS by
      its log, so its games wait on the tiles." The time is the
      updater's last `Startup -` line.
    - When it turns off because the client ran its own start command:
      "Steam finished updating itself at HH:MM:SS by its log, after N
      minutes. Its games open again." Under two minutes it says "after
      N seconds". The time is the start command's, and the minutes run
      from the `Startup -` line to it.
    - When it turns off because the last start is more than 20 minutes
      old: "Steam's log has shown no start for 20 minutes, so its games
      open again on the tiles. Steam may still be updating."
    - When it turns off because no Steam runs: "Steam is not running,
      so its games open again on the tiles."
    - When it turns off because the logs no longer show an update, with
      no start command after it (a later plain start, or a log that is
      gone): "Steam's log no longer shows an update, so its games open
      again on the tiles."
  - Nothing leaves the computer.
- `SteamStartup` gains a read, `readWithTimes(now, pastLongestStart)`,
  that also gives the two times it already finds: the updater's last
  `Startup -` line and the client's own start command. Only when asked
  does it look for the start command after a start over 20 minutes
  old. The watch asks only while its tiles wait, so a watch that was
  not active through the end of an update still writes the "finished"
  line, and every other read of an old start skips the console log.
  `read(now)` and its callers do not change.
- `TileModel` gains a role, `startsInSteam`, fixed when the tiles are
  read, so QML never looks at a command, and `anyStartsInSteam`, so the
  clock runs only when some tile can wait.
- `BuildingStones.qml` holds the stones and their building, lifted out
  of `StartingScreen.qml:52-86`.
- `Tile.qml` gains `waitsForSteam`, false unless set, `stoneStep`, the
  faded picture and the chip. The Log out tile and the grown-up screen's Back tile
  never set it.
- In `Main.qml` the grid's tiles bind `waitsForSteam: startsInSteam &&
  steamUpdate.updating`, the way `Main.qml:238` and `:312` already join
  C++ answers. One `Timer` there, `stoneClock`, steps every chip.
- `AppLauncher` gains `press(title, exec)`, the signal `heldForSteam`,
  and a property `steamUpdate` (a `SteamUpdateWatch *`) set in
  `Main.qml`, the way `DoorsFromTiles` is given `tiles`. `press()` asks
  the watch's `holdsBack()`, and either emits `heldForSteam` or calls
  `launch()`. A refused press writes "<title> did not open: Steam is updating
  itself." to the journal, the first time per game per update.
- `tools/make-steam-update.sh` makes up a Steam update for a
  throwaway account or a throwaway HOME, for the staged session and the
  VM checks. It sits in `tools/` with the other tools for a
  developer's own computer; the Containerfile copies only
  `tools/kidscan`. It writes the updater's and the client's log lines,
  with shapes from the launcher's own test lines and ADR-0037, and runs
  a stand-in process whose name in `/proc` is exactly `steam`. It refuses
  a Steam folder that looks like a real install, so it can never
  confuse a child's real Steam. It is never installed into the image.

### What a grown-up sees

- The same tiles as the child.
- The journal lines above, with Steam's times.
- `docs/TRY.md`'s paragraph on Steam updating itself at login
  (`docs/TRY.md:286-290`) now says that the games that start in Steam
  look faded, with small stones building in the middle, and open when
  Steam is done; that on the laptop an update took about a minute
  twice, a child's first install about 5 minutes, and once 13 minutes; that other tiles open as usual; that no
  stones is not a promise; and which journal lines to look for.
- Family's Games page, after its first paragraph
  (`guardian/qml/GamesView.qml:60`), gains: "While Steam updates
  itself, the games that start in Steam look faded on a child's screen,
  with small stones building in the middle. They open again when Steam
  is done, often about a minute after login, and longer the first time
  a child uses Steam. Other tiles open as usual." (`qsTr()`).
- Updating a child's Steam out of the child's sight is ADR-0048's.

### Why B

- **A child is never held on the starting screen for up to 10 minutes
  with no way back.** Under A a press during a long update waits on the
  starting screen, where keys, taps and the child's leave key do
  nothing, and may end on the grown-up screen with no game. Under B
  the child stays at the tiles.
- **A press can never start that long wait.** The 10-minute give-up
  and the lost tap of a long update (Context) cannot be reached from a
  tile while it waits.
- **The look is one many parents and children know.** A faded picture
  with a progress mark is how phones and tablets show an app that is
  updating.
- **The child stays free to open Draw or Practice**, which never wait.
- The maintainer moved the stones to the middle and asked for them to
  build. A mark in the middle is larger in the eye than one in a corner,
  and building stones are the one picture Cairn already uses for "on
  its way".

## Consequences

- At a login during which Steam updates itself, a child sees which
  games are not ready, and can pick something else. Whether children
  read the faded tiles that way is what S1 to S3 check.
- **The picture a child finds a game by is hidden** while Steam updates
  itself (ADR-0035). At 0.35 a game may be hard to tell from its
  neighbour. S3 checks whether a child can still find a game.
- **A wrong "updating" shuts every Steam game on the tiles for up to 20
  minutes.**
  - If Valve rewords the client's start command, Restarting holds for
    20 minutes after every update, and ADR-0037 already hides a game's
    window in that time (ADR-0037:234-237). Every Steam tile waits for
    those 20 minutes, and a press opens nothing. The journal's "no
    start for 20 minutes" line shows it. The fix belongs in
    `SteamStartup`.
  - An update that fails, with an error box ADR-0037 hides: the tiles
    wait for up to 20 minutes.
  - Both are bounded by the 20-minute rule and by the check that a
    `steam` process runs.
- **The stones go at 20 minutes even if Steam is still downloading.**
  If a download of the client runs on more than 20 minutes after the
  updater's last `Startup -` line, `SteamStartup` reads Ready
  (`SteamStartup.cpp:64-66`). A first install took 15 minutes on the
  laptop, so a slower evening could pass 20. The tiles stop waiting,
  and the journal says "Steam may still be updating", not "finished".
  A press then launches as today. Steam's update no longer counts as
  work, so the launch likely ends on the grown-up screen when the first
  grace ends, and nothing is sent again. That is inferred from
  `SteamGameWait::stillComing()` and not tried (V7). If the updater
  writes lines during a long download, counting the 20 minutes from its
  last line would fix both (What comes later).
- **The partly shown row cuts the centred stones.** On the laptop's
  screen the bottom of those chips is out of sight until the grid
  scrolls. The faded picture still shows.
- **A tile that shows its title loses the middle of it** while Steam
  updates itself, under the chip. A child who finds a game by its
  title may not find it then. A screen reader still reads the whole
  title.
- **Most waits still come with no stones.** The laptop's waits for a
  game were for tools, or for game updates the tap made due. Neither
  can be seen before a tap. A child who learns "stones mean wait" may
  also learn "no stones means now", and that is often wrong. "No
  stones" is not a promise, and the parent text says so.
- **The Terminal's `open` is not refused.** A Steam game opened there
  during an update waits on the starting screen as today, with the
  10-minute give-up.
- **Late:** the watch reads every 5 s. A press in the first 5 s of an
  update, before the watch has read it, launches as today: the
  starting screen, "Getting it ready" after the first grace, and the
  tap sent again after the restart (ADR-0037).
- **A Steam stopped part-way through an update:** no Steam process, so
  no tile waits, and the journal says Steam is not running. A press
  starts Steam, as today.
- Offline, an update Steam has already downloaded still installs, and
  the tiles are right to wait. With nothing to install, nothing waits.
- Steam updates itself only after Valve ships a client update. A child
  may see the waiting tiles too seldom to learn them.
- **Motion.** Four building marks at once on the first screen, and up
  to nine once the grid scrolls past the manifest's tiles, may pull at
  a child's eyes. Building marks that are the only motion on the grid
  may also read as "new" or "pick me". Under B a press that follows
  costs the child nothing but a restart of the stones. S1 and S3 watch
  for both by what children do.
- **Meaning.** The stones meant "starting". They now also mean "wait".
  The starting screen and a waiting tile share one picture for "on its
  way", which may teach "this is coming, not yet", or may blur. The
  staged session cannot settle this. Later evenings will show it.
- **For parents**, a Cairn mark on a game may read as a badge. The
  sentence on Family's Games page and in `docs/TRY.md` says what it
  means. A parent who reads neither may not know.
- There is no reduced-motion setting yet (ADR-0028:116-118). In Phase 3
  a child who needs less motion sees the stones held still.
- **Cost:** while the tiles are in front, the watch reads the last
  256 KB of `bootstrap_log.txt` every 5 s. It reads the last 256 KB of
  `console_log.txt` within 20 minutes of a start, or while its tiles
  wait, and `/proc` only while the logs say Steam is updating. The cost was not
  measured. If it matters, reading only when a log's size or time has
  changed is the next step. The chips' clock runs only while a tile
  waits.
- Tested in C++ and QML: T1 to T5.

## Alternatives considered

### A: stones in the corner, and a press opens and waits

Each Steam tile would show the same chip in its top-right corner while
Steam updates itself. The picture would stay at full strength. A press
would show the starting screen at once, with "Getting it ready. This
can take a few minutes." from the first second, and the launch would
wait as ADR-0037 says.

- For: every press answers in the same way, with the starting screen.
  In the usual case, an update over about a minute after login once
  the child's Steam is installed, a press
  costs the child that minute on the starting screen, and the lost tap
  is sent again. A wrong "updating" costs less: a press waits and ends
  on the grown-up screen, as today, and no game is shut. The picture a
  child finds a game by stays whole, and the faded look stays a
  grown-up's.
- Against: a press during a long update holds the child on the
  starting screen, with no way back, for up to 10 minutes from the tap.
  If the update outlasts that, as the 13 minutes of 2026-10-27 would
  have for a press in its first 3 or so, the launch ends on the
  grown-up screen with no game, because the lost tap is never sent
  again.
- Not chosen: the maintainer chose B on 2026-11-21.

### The chip in the top-right corner

The first drafts put the chip in a corner. Bottom-right was dropped
because the partly shown row cuts that corner off. Top-right showed on
every game tile in view. The maintainer moved the stones to the middle
of the tile. The faded picture now marks the tiles in the partly shown
row, so the cut at the bottom is a known cost.

### Two stones held still

The first draft held a calmer version ready: every chip shows the
bottom two stones, still, and only the focused tile builds. Dropped:
the maintainer asked for the stones to build. Stones held still remain
the reduced-motion look for Phase 3.

### A "not yet" screen on a press

A child who cannot read sees a screen that is not the game, and has to
leave it to try something else. It is still a no. Rejected.

### Only the sentence at the tap, with nothing on the tiles

The smallest change: a Steam press during an update shows "Getting it
ready" from the first second, and the tiles do not change. It is
cheaper, and nothing on the tiles can be wrong. Rejected, because
Steam updates itself at login, the moment a child picks their first
game, and that is when a choice before the press is worth most. It
also leaves the long wait on the starting screen that B is chosen to
avoid.

### The small stones growing into the big ones

A short motion from the chip to the starting screen's mark would show
the link Hiniker and colleagues advise pointing out. Under B a press on
a waiting tile shows no starting screen, so there is nothing to grow
into at that press. Not now. If S2 shows that children do not connect
the stones with waiting, a link between the two is the next thing to
weigh.

### Marking a tile for one game's own update

The reader would follow `content_log.txt` at the tiles: the game's
last `state changed` line since the client started, with both `Update
Required` and `Update Running`, until `finished update`, `update
canceled` or `scheduler finished`. Not now:

- No update of a storybook game itself has been seen on the laptop.
  The waits there were for the client and for tools.
- Its false marks are known. Shader downloads write `Update Running`
  without `Update Required`, and rewrite the game's download sizes. A
  suspended download keeps `Update Started`. A Steam killed mid-update
  may leave its flags set.
- Tools are not marked per game, and which tool a game uses is known
  only after its first run (ADR-0046).
- So the mark would usually be missing when a wait came, and its
  absence would read as a promise.
- The tap is what makes a held-back game update start (ADR-0048:39-47),
  so such a tile could not refuse a press the way B's tiles do.
- It needs tile roles that change while the tiles show. `TileModel`
  only ever resets the whole list (`TileModel.cpp:140-143`).

### Marking during a plain start

Every Steam tile would fade and come back within about 20 s at almost
every login. Rejected.

### Another picture

A padlock means a grown-up said no. An hourglass, a clock or a spinner
has to be taught. A progress ring or bar, as phones use, is the look
parents know, but it needs a size, and Steam's log gives one for a
download and not for an install. Rejected for now. The stones need no
new shape, and the staged session tests them.

## Checks

None of these has run.

### Staged session with children (on this branch, before merging)

On a throwaway build of the branch, in nested labwc on the dev PC or in
the cairn-min VM, never on the laptop's own build. The tiles are
today's: the manifest's five, then Steam games with pictures. The
update is made up with `tools/make-steam-update.sh`, for a throwaway
account, and it ends about 30 to 60 s after the child
first presses a faded game. Nothing is explained first.

Judged by what children do, not by the words they use:

- **S1.** While the tiles wait, which tile does the child press first:
  a faded game, a plain tile, or do they ask? How many presses go to
  faded tiles, and how many to plain ones?
- **S2.** After a press on a faded tile, what does the child do next:
  press it again, pick another tile, look away, or fetch a grown-up?
  How long until then? Anything said, such as "broken" or "stuck", is
  written down word for word.
- **S3.** While the tiles wait, can the child still find a game by its
  faded picture, such as "Show me the Arthur game"? Do they watch the
  stones rather than choose?
- Afterwards, "What are the little stones doing?" is asked once and
  the answer written down. It is not scored.

What the maintainer weighs:

- If children press faded tiles over and over, or say "broken", or
  fetch a grown-up for it, the refusal is not reading as "wait". A
  comes back to the maintainer, or the sentence at the tap alone.
- If children cannot find a game by its faded picture, the strength of
  the fade comes back to the maintainer.
- If children watch the building stones rather than choose, the motion
  comes back to the maintainer. The calm variant was dropped, so any
  change is his to choose.
- If children pick a plain tile, or wait and press again once Steam is
  done, B stands.

`docs/research/child-test-02.md` has this as entry 19, with a question
under "Not decided yet" on whether it runs at that test.

### In the cairn-min VM (after merging, on an image)

With ada (L1) and ben (L2), the manifest's tiles and a games file with
Steam games, and, if ADR-0059 has landed, ScummVM games. Updates are
made up with the same tool, `tools/make-steam-update.sh`, with lines in the shapes of the dev PC's of
2026-10-27. If Valve ships a client update during the checks, V1, V3
and V4 run once more on the real one.

- **V1.** At the tiles, `Startup -` then `Found pending update`, with
  the stand-in `steam` running. Within 5 s every Steam tile is faded
  and shows the stones in its middle, building in step, also on the
  partly shown row, where the chip is cut at the bottom. No other tile
  changes. The accessibility tree gives the title and "Waits for a
  Steam update. It opens when Steam is done." for a waiting tile, only
  the title for the others, and nothing for the chip.
- **V2.** A plain start: `Startup -`, then the client's start command
  18 s later. No tile changes at any moment.
- **V3.** The 2026-10-27 chain: install, restart, verify, start again,
  download, start command. The tiles wait throughout, with no fade out
  and back. They stop within 5 s of the start command, over one fade.
  Partway through, Draw is opened for a minute and closed, and the
  tiles are still waiting when it closes. The journal has one
  "started" line and one "finished" line, with Steam's times and the
  right minutes.
- **V4.** A press on a faded tile opens nothing and changes no screen.
  The stones on every waiting tile start again from the bottom,
  together. The journal has one "did not open" line for that game, and
  no second one for a second press. A press right after Steam has
  finished, before the watch's next read, opens the game.
- **V5.** A press on Draw during the update opens it as today. `open`
  of a Steam game in the Terminal during the update waits on the
  starting screen as today.
- **V6.** A recording at 10 frames a second of the grid with waiting
  tiles, and scrolled by a row. Nothing flashes, the stones stay in
  step, the card, its edge and the focus ring do not change, and
  nothing moves once the tiles stop waiting. A recording of the
  starting screen looks the same as one from main.
- **V7.** A log held at Updating past 20 minutes, with the stand-in
  running. The tiles stop waiting at 20 minutes, and the journal says
  Steam may still be updating. A press then: write down what happens
  (expected from the code: the grown-up screen when the first grace
  ends).
- **V8.** A log left part-way through an update with no Steam running:
  no tile waits, and the journal says Steam is not running. A press
  starts Steam. On a real update, the updater's process is called
  `steam` in `/proc`.

### On the kids' laptop

- **K1.** One real update of a child's Steam at login. The Steam tiles
  wait while the updater works and stop when Steam is up. The journal's
  lines match `bootstrap_log.txt` and `console_log.txt`. A press on a
  faded tile during it opens nothing, and the game opens from a press
  once Steam is done.

### Tested in C++ and QML

- **T1.** `tst_steamupdatewatch`: Updating and Restarting with a
  `steam` process turn it on. A plain Starting, Ready, a start older
  than 20 minutes and no `steam` process do not. It turns off once the
  start command is in the log. It reads nothing while not active, keeps
  its answer, and reads at once when it becomes active. A read asked
  for at once gives the fresh answer. The journal has one "started"
  line per update across a pause, the "finished" line with Steam's
  times, in minutes and under two minutes in seconds, also when read
  again over 20 minutes after the last start, and the 20-minute and
  no-Steam lines.
- **T2.** `tst_tilemodel`: `startsInSteam` is set for Steam commands
  only.
- **T3.** `tst_applauncher`: `press()` on a Steam tile while the watch
  says updating launches nothing, emits `heldForSteam`, and writes one
  "did not open" line per game per update. When the fresh read says
  Steam is done, it launches. A press on a drawing app launches. A
  press on a Steam tile with Steam ready launches. `launch()` itself,
  as the Terminal's `open` calls it, is never refused
  (`aLaunchWhileSteamUpdatesIsNeverRefused`).
- **T4.** `tst_steamstartup`: the new read gives the last `Startup -`
  time and the start command's time, from the kids' laptop's lines of
  2026-10-20, and the start command's time also after a start over 20
  minutes old.
- **T5.** QML: only a Steam tile waits, and only while Steam updates
  itself. Its picture, or its title when it has no picture, is at
  `coveredOpacity`, and the card, edge and focus ring are not. Every
  chip shows the same step. `heldForSteam` starts the clock again from
  no stones. The description is set, and the chip is ignored by screen
  readers. The clock stops when no tile waits. The starting screen
  draws `BuildingStones` and builds as before.

## What comes later

- If a log shows the updater writing lines during a long download,
  `SteamStartup` counts its 20 minutes from the last of them, so the
  tiles and ADR-0037's wait both hold until the download ends.
- A press in the first 5 s of an update, and the Terminal's `open`,
  still wait on the starting screen and can end on the grown-up screen
  with no game. A Back that ends such a launch while Steam still reads
  Updating, or a 10 minutes counted from the tap sent again, would need
  an ADR of its own, after a check in the VM that taps during a real
  update are lost.
- Marking a tile for one game's own update, once a child's Steam has
  been recorded updating a game in the background at login, with its
  lines and flags written down (ADR-0048's E12). Its reader then joins
  `SteamUpdateWatch`.
- Tools: marking the tiles whose games use a tool that is updating,
  once the map from game to tool is known (ADR-0046).
- ADR-0048: a grown-up updates a child's Steam while the child is
  logged out. Most waiting tiles then never show.
- Reduced motion, in Phase 3: the stones held still.
- A window rule for the updater's windows, once their X11 instance is
  known (ADR-0037).
