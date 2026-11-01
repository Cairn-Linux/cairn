# ADR-0048: A grown-up updates a child's Steam out of sight

**Status:** proposed. Mason decided on 2026-10-30 to build #181's
button, and on 2026-10-31 answered the plan's seven questions, each as
the plan recommended. It becomes accepted when the checks under "Before
anything is built" have run without overturning a bullet of the Decision
(E11 only if memory is tight), with what E12 and E13 found written into
"Updates Steam puts off", and when the pieces, once built, have passed
the checks under "Once the pieces are built".
**Date:** 2026-11-01
**Closes:** (none) — part of issue #181, its piece after ADR-0046; part of
ROADMAP P1-5 and P1-13. Once accepted, amends ADR-0027, ADR-0032,
ADR-0033, ADR-0041 and ADR-0046.

## Context

### What a child waits for

Each child's Steam is their own: the client, the tools it runs games
with (the Steam Linux Runtimes and Proton), and a copy of each game
(ADR-0033).
Each of them updates itself the first time that child's Steam runs after
a while away, and that is at the child's login (ADR-0027).

On the kids' laptop on 2026-10-27 (#181):

- At one child's login, Steam installed an update of itself that it had
  downloaded earlier, then downloaded another 424 MB.
  The first tap then waited for two tools, soldier and scout, to update.
  The game started 2 minutes after the tap, about 15 minutes after login.
- The tester account's Steam downloaded a 378 MB update of itself at
  login before the tapped game could start.

The launcher keeps this calm: "Getting it ready" while Steam works
(ADR-0036, ADR-0037), and Steam's updater windows hidden (issue #183).
But the child still waits many minutes on the laptop's Wi-Fi, and every
child's Steam downloads the same update for itself.

Valve says why the wait comes at a tap.
Since 2020 "only games played within the last 3 days will be updated
immediately" (Valve's post of 2020-03-30, "Managing Steam Bandwidth
During COVID-19 Pandemic").
"For games that you haven't played in a while, Steam might wait a few
days to bundle multiple updates together or choose to download the
update while you are asleep" (Valve's post of 2024-12-11, "New Settings
for Game Updates").
A game updates at once when someone asks to play it.
On the kids' laptop, tool updates waited for days until a tap made them
due (ADR-0036).
And a child's Steam runs only while the child is logged in, or in a
grown-up's visit (ADR-0032).

### What is fixed already

Issue #206 had three fixes.
The launcher starts a child's Steam again once when it stops part-way
(ADR-0036).
The grown-up screen says "Log out and back in, then try it again."
(ADR-0045).
A Save copies the parent's Steam tools into a child's Steam with their
new games, so a first tap does not wait for a tool Cairn has a copy of
(ADR-0046, proposed).

What is left is the long downloads: Steam's update of itself, newer
builds of the tools, and the games' own updates.
ADR-0046's section "Later (#181)" kept them for a button that runs the
child's Steam out of sight, after VM experiments, with an ADR of its
own.
This is that ADR.

### What Mason decided

On 2026-10-30 Mason decided to build it, with two additions (#181).
The parent is told it will take a while, and sees how far it has got.
And each tool is asked for by name, with the tools it needs: Steam does
not do that itself, and for scout it never asks for soldier, the runtime
scout runs in (#206).
The second became ADR-0046's copy, for a tool a child's Steam lacks, a
change Mason confirmed that day.
For a newer build of a tool, answer 3 below decides.
The plan of 2026-10-31 runs the child's own Steam, as the child, in a
session of its own with a screen nobody sees.
A new mode of the visit helper starts it, and a watcher reads Steam's
logs as the child.
The plan asked seven questions.
On 2026-10-31 Mason answered each as the plan recommended:

1. **Stop:** yes. Stop ends the run, Steam keeps what it downloaded, and
   the child can log in at once. In a window opened again later, Stop
   asks for the password again.
2. **Time limit:** 90 minutes.
3. **Pushing updates Steam put off:** tools only, and only if E13 shows
   that validating them works. Never games. Writing Steam's
   "Immediately" value needs an ADR line first.
4. **Power:** it starts only on mains power. If it is unplugged midway
   it carries on, and a closed lid still suspends.
5. **The parent's own Steam:** it stays open, unless a test on a 4 GB
   computer shows it swapping.
6. **One child per press:** yes. "Every child" and night-time updates get
   an ADR of their own.
7. **Place:** on the Games page beside "Open Steam for Ada", greyed while
   Ada is logged in.

### What the VM showed

The experiments are numbered after ADR-0046's E1 to E9.
On 2026-10-31, in the cairn-iso VM, the test child `ada` was logged out
and her Steam was signed in from before.

- **E10: a hidden Steam starts, signs in and stops.**
  `ada`'s own Steam ran as her, from the visit helper's PAM service
  (`cairn-steam-visit`), in a headless cage (`WLR_BACKENDS=headless`,
  `WLR_RENDERER=pixman`) in place of the Guardian's screen, with
  `steam -silent`.
  It was signed in 10 s after the start.
  logind gave it a session of class `background` with no seat, and no
  window showed anywhere.
  Steam's own check for an update of itself came 2 minutes after the
  start and found nothing to do.
  Nothing else was due, so no download started.
  Two minutes after the sign-in each process used 1 to 3 % of the CPU,
  and `ada`'s processes held 2.16 GB, 2.26 GB at the peak, most of it in
  Steam's two web helpers (592 and 520 MB).
  The visit's own close (`steam -shutdown`, then stopping the unit and
  ending `ada`'s processes) had Steam down in 3 s and nothing of hers
  left after 4 s, and Steam's update log ended `Shutdown`.
- **E16(a): no internet from the start.**
  With `ada`'s network cut alone, the one clear line was `Connectivity
  test: result=NoLAN` in Steam's `connection_log.txt`, 4 s after the
  start.
  In 10 minutes there was no `LogonFailure` line and no sign-in.
  Steam opened two hidden windows named "Unexpected Transport Error",
  which wait for a click.
  `steam -shutdown` did not close it; after its 25 s, ending `ada`'s
  session did, in 1 s.
  A Steam that was signed in and then lost the internet wrote
  `LogonFailure No Connection` within 6 s, in #211's check that morning.
- **E17(a): the lock.**
  With `ada` listed and her hidden Steam running, the login screen
  refused her tile (`pam_listfile`) with "A grown-up is setting
  something up for you. Try again soon."
- **E17(b): the clean-up.**
  A transient unit's `ExecStopPost=` removed the list when the unit's
  main process was killed outright, as the kernel does when memory runs
  out.
  When the whole unit was killed (`systemctl kill -s KILL`), systemd
  killed the `ExecStopPost=` command too, and the list stayed.

E11 was not needed.
E12 to E14 and E16(c) need a newer build of a tool from Valve, E15 an
update of Steam itself, Valve's next or a forced one, and E16(b) a child
whose Steam never signed in.

## Decision

**A Guardian can update one child's own Steam while that child is logged
out. "Update Ada's Steam" on the Games page warns that it can take a
while and asks for the password. Then Ada's Steam runs as Ada, in a
session of its own with a screen nobody sees, while a card shows what
Steam is doing. It ends when Steam has nothing more to do, when the
parent presses Stop, or after 90 minutes, and says plainly what Steam did
and what it left for later. Ada cannot log in meanwhile. Cairn asks
Steam for an update it put off only for Steam's tools, never for a game,
and only if E13 shows that validating a tool works.**

### The button

- On the Games page, for the child picked, **Update Ada's Steam** sits
  beside **Open Steam for Ada** (answer 7).
- While Ada is logged in it waits, greyed with her games and **Open Steam
  for Ada**, under the card that says she is still logged in (ADR-0041,
  answer 7).
- One press updates one child (answer 6).
  "Every child", and night-time updates, off unless a parent turns them
  on, get an ADR of their own.
- It starts only on mains power (answer 4), as `systemd-ac-power` reads
  it, which is how `ConditionACPower=` reads it: a computer that knows of
  no mains connector, as a desktop may not, counts as plugged in.
  The page asks before the warning opens, and the root command asks
  again.
  On battery the page says "The computer is running on its battery. Plug
  it in, then try again."

### The warning

Focus starts on **Not now**, so a second Enter never starts an update:

> **Update Ada's Steam now?** Steam will update itself and Ada's games
> while Ada is logged out, so Ada does not wait for them later. This can
> take a while, sometimes more than an hour on slow internet. Ada cannot
> log in until it is finished or you stop it. Keep the computer plugged
> in.
>
> **Not now** · **Update Ada's Steam**

The password box comes next, with the message "A grown-up's password is
needed to update a child's Steam."
It cannot name the child (ADR-0041), so the warning does.

### The card

The card is titled "Updating Ada's Steam".
It shows one line for the stage, a bar when the sizes are known, "Ada
cannot log in until this is finished." and **Stop**.
The stage lines:

- Starting Ada's Steam. Nothing opens on the screen.
- Steam is updating itself: 120 of 469 MB.
- Steam is signing in.
- Steam is checking for updates.
- Steam is getting 2 updates: 0.4 of 1.6 GB.
- Closing Ada's Steam.

Steam's updater writes the sizes of its own update as it goes.
For the other updates, Steam writes what each will download before it
starts, but whether it says how far each has got is not checked
(inference; E12 records it).
Without that, the line is a count, "Steam is getting 2 updates.", with
no bar.
A game is named as the full games list names it (ADR-0041).
Steam's tools are "the tools Steam runs games with", as in the Save's
window, never one by one (ADR-0046).
An app that is neither is counted, not named.

### The results

The card ends with one of these sentences, then **Close**:

- Ada's Steam is up to date.
- Ada's Steam is updated. Steam left updates for 2 games until Ada plays
  them, so they may take a few minutes to start the first time.
- Ada's Steam needs to be signed in again. Open Steam for Ada and sign
  in, then try again.
- Ada's Steam could not reach the internet. Check the Wi-Fi, then try
  again.
- The computer is nearly out of space, so Steam could not finish. Free
  some space, then try again.
- Ada's Steam stopped at a question only a person can answer. Open Steam
  for Ada to see it.
- Ada's Steam was still busy after 90 minutes, so Cairn closed it. Steam
  carries on next time.
- You stopped the update. Steam keeps what it downloaded. Ada can log in
  now.
- Ada's Steam ran for 40 minutes. Cairn could not tell whether everything
  is up to date.

"Up to date" comes only when the run ended by the end rule below, and
no installed app's manifest in Ada's libraries still says it needs an
update.
A tool Steam left for later is named in the second sentence as the tools
Steam runs games with.

Before Steam starts, the update can also be held:

- Ada's Steam has not been signed in yet. Open Steam for Ada and sign
  in, then try again.
- The computer is nearly out of space, so Ada's Steam was not started.
  Free some space, then try again.
- A child's Steam is being updated on this computer. Try again when it
  is finished.

When the helper ends without its last word, the card says "The update
stopped before it finished. Steam keeps what it downloaded."
The held words a copy or a logout already has (`logged-in`, `busy`,
`copying`, `logging-out`, `still-running`, `not-set-up`, `not-started`)
get sentences like the page's own for them, which the change that builds
the card writes.

### Stop

- **Stop** writes `stop` on `cairn-family`'s standard input, as the
  visit's window writes `done` on the visit helper's.
  `cairn-family`, as root, stops the update's unit.
  systemd signals the helper, and its close runs within the job's
  `TimeoutStopSec=` (below): Steam is asked to save and quit, it keeps
  what it downloaded, and Ada can log in at once (answer 1).
  `SystemJob` learns to read its input while it waits for the job;
  today it waits only for the job.
- Closing the window does not stop it.
  The update is a system job outside the Guardian's session, as a copy
  is (ADR-0041), so the window closing, the screen lock and the
  Guardian's logout leave it running.
  Nothing it runs may end because nobody reads its lines.
- A Family window opened again later learns from systemd that the
  update's unit runs (inference: systemd tells any account a unit's
  state; check 7 under "Once the pieces are built").
  It shows "Ada's Steam is being updated." with **Stop**, and no
  progress in the first version.
  That Stop asks for the password again (answer 1).
  It runs `cairn-family stop-update LOGIN` through pkexec, with the
  message "A grown-up's password is needed to stop updating a child's
  Steam."

### The time limit

- A run lasts 90 minutes at most (answer 2), time for about 1.5 GB at
  3 Mbit/s.
  At 90 minutes the helper closes Steam and says so.
- Every wait inside the run has a limit of its own under the 90
  minutes.
  Steam's start-up check can hang, and on the kids' laptop the updater's
  windows were once up for 13 minutes (ADR-0037).
  The change that builds the watcher sets those limits from E12 and E15.
- systemd's `RuntimeMaxSec=` on the job backs up the helper's own limit,
  with room for the close after it.

### Power, sleep and memory

- An update starts only on mains power (above), and carries on if the
  computer is unplugged midway (answer 4).
- The job holds `systemd-inhibit --what=sleep:idle --mode=block
  --who=Cairn --why="Updating a child's Steam"`, so the sleep timer does
  not suspend the computer halfway.
  It does not hold the copy's `handle-lid-switch`, so that a closed lid
  still suspends the computer (answer 4).
- That holds where logind handles the lid itself, as at the login screen
  and after the Guardian logs out.
  logind then ignores the `sleep` lock for the lid, by its default
  `LidSwitchIgnoreInhibited=yes`, which the image keeps.
  While a Guardian is logged in to Plasma, Plasma handles the lid and
  asks logind to suspend, and logind may refuse while the `sleep` lock is
  held, as ADR-0041's step 6 expects for a copy (inference; the sleep
  check).
  If the sleep check shows that, this ADR is amended before it is
  accepted.
  Either the lock becomes `idle` alone, if the check shows that Plasma's
  sleep timer still waits for it, or Mason changes answer 4.
- Steam should carry on its downloads after the computer wakes (not
  tried).
- The parent's own Steam stays open (answer 5).
  If the 4 GB check shows the computer swapping, this ADR is amended so
  that an update closes it first, as a visit does (`OwnSteam`,
  ADR-0032).

### One child at a time

- An update takes the list that a visit, a copy and a logout take
  (`/run/cairn/steam-visits`, ADR-0032), which names one child for the
  whole computer.
  So PAM refuses Ada at the login screen and at the consoles while it
  runs, and the login screen says "A grown-up is setting something up for
  you. Try again soon." (E17(a)).
- It writes a marker of its own first, `/run/cairn/steam-update`, as a
  copy and a logout do.
  While it runs, a visit, a copy (so a Save, for any child), a logout and
  another update are refused with a new busy word, `updating`.
- Nine places learn the word: the helper's `busy_reason`, the held words
  of `CopyJob` and `LogOutCommand`, the Save's results (`SaveResults`),
  `ChildLogOut`, `SteamForChild`, and the sentences in
  `SaveResultWords.qml`, `LoggedInCover.qml` and `SteamForWords.qml`,
  each with its tests.
  Without it each of them would say "failed", "unknown" or that a visit
  is open.

### The root side

`cairn-family update-steam LOGIN` is a new subcommand of the one root
command (ADR-0039, ADR-0041), shaped like `log-out`.

- Its polkit action is `com.cairnlinux.family.update-steam`, on
  `cairn-family`'s path, `auth_admin`, with
  `org.freedesktop.policykit.exec.argv1` `update-steam`.
  The levels rule already answers no to it, before any password box, for
  any account that is not an administrator (ADR-0023).
- It checks the caller (root, or an administrator through pkexec), that
  it was given exactly one login, and that the login is a child Cairn
  made.
  Then it asks whether the computer is on mains power, and holds
  `on-battery` when it is not.
- It runs `cairn-steam-visit --update CHILD` through `SystemJob`, as the
  system unit `cairn-steam-update-UID`, inside the inhibitor above, with
  `RuntimeMaxSec=`, `TimeoutStopSec=` and an `ExecStopPost=` release
  (below).
  It passes on the helper's lines with the child's login put in from its
  own argument, never from the helper, as ADR-0041 does.
- `cairn-family stop-update LOGIN` has its own action,
  `com.cairnlinux.family.stop-update`, `auth_admin`, with argv1
  `stop-update`.
  It stops that child's update unit and nothing else.

### The helper's update mode

`cairn-steam-visit --update CHILD` is a fourth mode beside the visit, the
copy and the logout.

- **Root only, never through pkexec,** as the copy and the logout.
  It refuses `not-for-pkexec` when `PKEXEC_UID` is set, so the visit's
  own polkit action, which names no first argument, can never start it.
- **The lock.**
  It clears what a killed update left (below), writes its marker, then
  lists the child, checks the list's SELinux label (`not-set-up`) and
  waits for the child's last login to end (`waiting`, `logged-in`,
  `still-running`), with the copy's own code (ADR-0033, ADR-0041).
- **Room.**
  It holds `low-space` when the filesystem that holds the child's home
  has less than 5 GB free (inference: a guess, tried in E16(c)).
  During the run, Steam's `Disk write failure` ends it as out of space
  (a line others have reported, not yet seen here; E16(c)).
- **A Steam that has never run, or never signed in,** is held
  `no-steam` before Steam starts.
  How the helper tells, as the child, is for E16(b) to settle.
- **The session.**
  It starts Ada's Steam as a visit starts it, with no Guardian's screen:
  `systemd-run --uid=CHILD --property=PAMName=cairn-steam-visit`, with
  `WLR_BACKENDS=headless` and `WLR_RENDERER=pixman`, running the image's
  run-only cage (`/usr/libexec/cairn/cage-visit`), which runs
  `/bin/bash -c 'exec /usr/bin/steam -silent'`.
  - The PAM service stays the visit's, so neither rule that decides
    whether a child is logged in changes: the helper's and the Guardian
    tool's `LoginSessions` already pass over that service.
  - `-silent` is the kiosk's own start (ADR-0027), so Steam does the
    work it would do at the child's next login, on the same update
    branch.
  - bash sits in the middle because SELinux refuses a PAM-started
    process that enters Steam's script directly
    (`docs/research/steam-visit.md`).
  - Pixman lets cage draw without the GPU, and cage ends when Steam
    ends.
  - The unit gets no hardening that takes away the user namespaces
    Steam's runtime needs.
- **The watcher**, below, runs as the child.
  The helper passes on only its lines that match a fixed pattern
  (`is_update_line`), with tracing off, as it does the copy program's
  (ADR-0041).
- **The close** is the visit's: `steam -shutdown` as the child, with its
  25 s, then stopping Steam's session and ending everything of the
  child's (`end_child`).
  E16(a) showed the second half is needed: Steam offline did not shut
  down when asked.
  The helper says `closing`, then `finished`, or `stuck` when a program
  of the child's would not end.
  A stuck close lets Ada log in, as a copy's and a logout's do
  (ADR-0041), and the card says "One of Ada's programs would not close.
  Restart the computer before Ada logs in."
- **However it ends,** the helper's own trap removes the list and the
  marker, as a copy's does.
- **The release.**
  systemd runs the job's `ExecStopPost=` after every end of the job, not
  only when the helper is killed: after a run that ended well, a refused
  press, Stop and the time limit too.
  So the marker says which run wrote it, and whether that run took the
  list.
  It holds the child's name and the job's invocation id from the start,
  the `$INVOCATION_ID` that systemd gives every process of one run of a
  unit, `ExecStopPost=` included.
  The helper adds a line to it once it has listed the child.
  The release does nothing at all, not even end Steam, unless the marker
  holds this run's invocation id.
  Then, if this run took the list, the release runs the close for what
  is left of Ada's hidden Steam and removes the list; and it removes the
  marker.
  So a press refused during a visit never ends the visit's Steam, and
  never frees the list the visit holds.
- **Time to close.**
  The job's `TimeoutStopSec=` is 2 minutes.
  The close's own limits add up to about 85 s at most: up to 25 s for
  `steam -shutdown`, 25 s more for Steam to end, up to 15 s for Steam's
  own unit to stop, and up to 20 s in `end_child`.
  The image's default is 15 s, shorter than E16(a)'s close of 26 s.
  Without a limit of its own, systemd would kill the helper halfway
  through the close after Stop or at the time limit, and the release
  halfway through its own (inference; checks 2 and 3 under "Once the
  pieces are built").
- **Known limit (E17(b)).**
  When the whole unit is killed, by hand or perhaps by systemd-oomd,
  which kills whole groups of processes, systemd kills the
  `ExecStopPost=` command too.
  The list and the marker stay, and so does Ada's hidden Steam, which
  runs in a unit and a session of its own.
  Ada then cannot log in until a restart empties `/run`, as ADR-0041's
  known limit says for a copy.
- **Narrowing it.**
  Before it takes the list, every mode of the helper looks for an update
  marker whose invocation id belongs to no update unit that is running:
  what a killed update left.
  If that update had taken the list, the helper runs the close for its
  child, then removes the list; it removes the marker either way, and
  goes on.
  If a program of that child's would not end, both stay until a
  restart.
  A new update for the same child runs under a new invocation id, so it
  clears its own child's leftovers too (inference; check 4 under "Once
  the pieces are built").

### The watcher

- A small C++ program, `cairn-steam-watch`, run by the helper as the
  child, as the copy program is: `setpriv`, `env -i`, standard input
  from `/dev/null`, and its error output going nowhere.
  It runs as the child because the child's home is closed to everyone
  else, and code that reads a child's home runs as the child (ADR-0033).
  A link planted there reaches nothing the child could not reach
  already.
- It shares the launcher's `LogTail`, `SteamStartup` and `SteamSignIn`
  through a static library that both build from, as `accounts/` is
  shared.
  It adds a reader for every app's update and a pass over the manifests.
- It reads, in Ada's Steam folder:
  - `logs/bootstrap_log.txt` and `logs/updateui_child.txt`: Steam
    updating itself, with its sizes (`Downloading update (x of y KB)`,
    `Set percent complete`), its restarts, and its check after each start
    (`Background update loop checking for update`, then `Nothing to do`);
  - `logs/console_log.txt` and `logs/steamui_login.txt`: Steam ready
    (its own start command, as `SteamStartup` reads it), signed in
    (`Waiting for compat in post-logon took`, `SetLoginState`), or its
    sign-in turned down (`LogonFailure`);
  - `logs/connection_log.txt`: no internet (`result=NoLAN`, E16(a), and
    perhaps `result=NoInternet`, inference);
  - `logs/content_log.txt`: each app's update (`Update Required`,
    `update prefetch finished : N bytes to download`, `Update delayed`,
    `update started`, `finished update`, `scheduler finished`), and
    `Disk write failure`;
  - at the end, each `appmanifest_*.acf` in her libraries, for the Update
    Required bit (2) of `StateFlags` and `BytesDownloaded`, which Valve
    does not document;
  - and the names of the windows on Ada's hidden display, as E9 listed
    them, for a window that waits for a person: Steam's Install dialog,
    the box it shows when the account plays elsewhere, or the box that
    says steamwebhelper is not responding.
- Which window names mean a question, and which are Steam's own quiet
  windows, is recorded in E12, E13, E15 and E16.
  How the watcher lists them, with no new dependency, is for the change
  that builds it.
- It prints only fixed words, appids and numbers, only when something
  changed, and at most one line every 5 seconds.
  So 90 minutes stay under `SystemJob`'s 4000 lines, and every line under
  its 256 bytes.
  It never prints a Steam name, a game's name, a path or a word from a
  log.
- If Valve changes its lines, the watcher sees nothing it knows, and the
  run ends at its limit with the last result above: never a false "up to
  date".

### When it ends

The end rule is inference until E12 and E15 check it.
The run is finished when all of these hold:

- Steam is ready and signed in;
- its check after its last start said `Nothing to do`;
- no app has been queued for an update, or updating, for 3 minutes;
- no update Steam put off (`Update delayed for N secs`) comes due before
  the 90 minutes are up.

If Steam downloaded an update of itself meanwhile, the helper closes it
cleanly and starts it once more, so that the update installs, and waits
until it is ready and signed in again.
Then the watcher reads the manifests, and the helper closes Steam.

A run also ends:

- at once, when the connection log says there is no internet (E16(a));
- 3 minutes after `LogonFailure No Connection`, if Steam has not signed
  in again by then;
- when Steam's servers turn its sign-in down, as `SteamSignIn` reads a
  refusal (ADR-0036, #182);
- at `Disk write failure`;
- when a window waits for a person, unless the connection log or the
  sign-in has already given the reason;
- at **Stop**, or at the 90 minutes.

Steam's own restarts while it updates itself should not end the session:
`/usr/bin/steam` starts its script again in place (inference; E15).

### Updates Steam puts off

Valve may leave updates for the games and tools Ada has not played
lately, and a child has never played their copies (inference; E12).
So the result says what Steam left, and E12 to E14 test ways to fetch
it anyway:

- E12: whether a hidden Steam fetches them by itself;
- E13: `steam://validate/<appid>`, and `steam -applaunch <appid>`, for a
  tool;
- E14: Steam's "Immediately download updates" value, written into a
  copied manifest while Ada's Steam is stopped.

Answer 3 decides what Cairn may do about them:

- for Steam's tools only, never for games, which may keep their saves
  inside their own folders (ADR-0033);
- by validating a tool, and only if E13 shows that validating fetches
  Valve's build with no window left waiting;
- Steam's "Immediately" value only with a line of its own in an ADR
  first, since it changes ADR-0046's byte-for-byte copies and would
  start downloads at Ada's own logins too (inference).

So if E12 shows Steam waiting and E13 shows that validating works, the
helper validates each tool Steam left, once the end rule holds.
It runs `/usr/bin/steam steam://validate/<appid>` as Ada, on her hidden
display, for each tool by its appid and for the tools it runs in, scout
with soldier, as Mason's addition of 2026-10-30 asked.
Then the run ends by the end rule again, and the result names what is
still left.
If E12 shows Steam fetching them by itself, or E13 fails, Cairn asks
Steam for nothing, and the result says what Steam left.
Which of these holds is written in here, from E12 and E13, before this
ADR is accepted.
`steam -applaunch` for a tool, E13's other half, is not in answer 3, so
it is not used.

### What cairn-family says

`update-steam` answers in lines of fixed words, checked logins, appids
and numbers, never a Steam name or a path, as ADR-0041 requires:

| Line | Meaning |
|---|---|
| `starting LOGIN` | The first line: the password was accepted and the job starts |
| `waiting LOGIN` | The child's last login is still ending |
| `steam-starting LOGIN` | The child's Steam is being started out of sight |
| `steam-updating LOGIN DONE TOTAL` | Steam is updating itself; DONE and TOTAL in KB, both 0 while its updater gives no sizes |
| `signing-in LOGIN` | Steam is ready and signing in |
| `checking LOGIN` | Steam is signed in, and is updating no app |
| `app LOGIN APPID DONE TOTAL` | Steam is getting the app's update, in bytes; TOTAL from Steam's prefetch line, and DONE 0 while Steam says nothing of how far it has got (inference; E12) |
| `app-done LOGIN APPID` | The app's update has finished |
| `restarting LOGIN` | Steam is started once more, to install an update of itself |
| `closing LOGIN` | Steam is asked to save and quit |
| `left LOGIN APPID` | At the end, the app's manifest still says it needs an update |
| `updated LOGIN` | The run ended by the end rule; the `left` lines say what Steam left |
| `held LOGIN WORD` | The run ended for WORD, or never started |
| `done` | The last line |
| `refused WORD` | Nothing was tried |

`held` is one of:

- before Steam starts: `on-battery`, `low-space`, `no-steam`,
  `logged-in`, `still-running`, `busy` (a visit), `copying`,
  `logging-out`, `updating` (another update), `not-set-up`, or
  `not-started` (the job, or its inhibitor lock, could not start);
- after: `sign-in-refused`, `no-connection`, `out-of-space`, `question`,
  `time-limit`, `stopped` (Stop, or a signal), `unsure` (the run ended
  and the watcher could not tell, or the manifests could not be read),
  `stuck`, or `failed` (the helper ended without its last word).

`refused` is one of ADR-0039's `not-root`, `not-a-grown-up`, `usage`,
`bad-name` and `not-a-child`.
`stop-update` says `stopped LOGIN` once the unit has stopped, or `held
LOGIN not-updating` when no update of that child runs, then `done`, with
the same refusals.
`journalctl -t cairn-family` and `journalctl -t cairn-steam-visit` get the
same words.

### Where it applies

The image only, as ADR-0041 and ADR-0046.
The kids' laptop has no `cairn-family`, and `docs/TRY.md`, which
describes provisioning, keeps step 7's visit.

### Nothing phones home

The only network traffic is Steam's own: the client, its tools and the
games, software the family installed, under the Steam account the family
signed in.
Cairn adds no network call of its own, and the watcher reads local files
only.

## Consequences

- A child whose parent pressed the button logs in to a Steam that has
  updated itself and fetched what it was ready to fetch.
  So a first tap waits less often, and less long.
  How much less depends on E12: if Steam leaves the tools and games Ada
  has not played for later, the button covers Steam's own update, and
  her tools too if validating works (E13), and the result says what is
  left.
- **The parent waits instead of the child.**
  A run can keep Ada from logging in for up to 90 minutes.
  A Save, a visit and a logout, for any child, wait too, since the list
  names one child for the whole computer.
  The warning says before the password that Ada cannot log in meanwhile,
  and **Stop** lets her in at once.
- **Memory.**
  In E10, `ada`'s processes held 2.16 GB two minutes after the sign-in
  and 2.26 GB at the peak, as systemd counted them, most of it in
  Steam's two web helpers.
  Mason judged that too much for a 4 GB computer, the Minimum tier
  (ADR-0003), beside the parent's Plasma and their own Steam, about
  1.4 GB signed in (ROADMAP P0-10), and asked for a measurement on one
  (#181): the 4 GB check.
  If it shows the computer swapping, an update closes the parent's Steam
  first, and E11's lighter sessions are tried.
- **The lid under Plasma** (inference).
  A parent presses the button from their own Plasma session, and often
  stays logged in.
  If logind refuses Plasma's suspend while the update's lock is held, a
  closed lid does not suspend the computer until the update ends,
  against answer 4.
  The sleep check settles it before this ADR is accepted ("Power, sleep
  and memory" says what changes).
- **What a stale list costs.**
  A whole unit killed leaves Ada refused at login, and her hidden Steam
  running, until a restart, or until the next job of the helper clears
  them (the known limit above).
- **Disk.**
  Each update a child's Steam downloads breaks the reflink sharing for
  the files it changes, as it would at her login (ADR-0033).
- **Steam Cloud.**
  Starting Ada's Steam can bring her Steam account's cloud saves into her
  copies of the games, as her login does.
- **Steam's own words.**
  The watcher depends on Steam's logs and manifests, which Valve does
  not document.
  A change in their wording is a lost feature, not a wrong one
  (ADR-0036): the run ends at its limit with the sentence that says
  Cairn could not tell.
- **Given up:** a promise that everything is current, since Valve
  decides when a game's update is due and Cairn never asks for one;
  live progress in a window opened again later; starting on battery.
- **Left for ADRs of their own** (answer 6): updating every child at
  once, and updates at night.
- **New pieces:** a fourth mode of the visit helper, which reads the
  child's Steam logs and manifests as the child; a watcher program; the
  launcher's Steam readers in a static library shared with it; two
  subcommands with their polkit actions, `update-steam` and
  `stop-update`; `SystemJob` reading its input while it waits, and
  giving a job `TimeoutStopSec=` and `ExecStopPost=`; every mode of the
  helper clearing what a killed update left; the busy word `updating` in
  nine places; and the button, the warning and the card on the Games
  page.
  `GamesPage` is past CLAUDE.md's 200 lines already, so the update gets
  classes of its own.
- Once accepted, it amends:
  - **ADR-0027:** Steam also starts outside a login, while the child is
    logged out, in a session of its own, never as `cairn-steam`.
    The launcher's restart stops only `cairn-steam`, so it never touches
    an update's Steam, and the child cannot log in meanwhile.
  - **ADR-0032:** the visit helper gains `--update`, in which Cairn
    reads the child's Steam logs and manifests, as the child, and writes
    none of Steam's files.
    cage also runs headless.
    A visit is refused `updating` while an update runs, and every mode
    first clears what a killed update left.
  - **ADR-0033:** check 15, whether Steam fetches an update in the
    background before a tap, gets its answer for a Steam left running
    out of sight, from E12.
    "Updates are each Steam's own" stays: the update is the child's own
    Steam doing its own updates.
  - **ADR-0041:** two subcommands and polkit actions with their own
    lines, the held word `updating` in the Save's table and the
    logout's, and a system job of up to 90 minutes with an
    `ExecStopPost=` release.
  - **ADR-0046:** its section "Later (#181)" is this ADR.
- DESIGN §3.2's Games row and §8.3 name the button, marked proposed.
- No new dependency: cage, `systemd-inhibit`, `systemd-ac-power` and
  `setpriv` are in the image, the watcher is C++ with Qt, and the shared
  library is a CMake change.

## Checks

### Before anything is built

E11 to E16 and the sleep check run in cairn-iso, with the test child
`ada`, and the 4 GB check on a computer with 4 GB.
Each run starts from a snapshot, with `ada` logged out, her Steam signed
in, and a note of where each of her Steam logs ended.
For a **stale tool T**, a reflink copy of each of `ada`'s four tools,
with its manifest, goes aside while they are current, as in E1.
Once Valve ships a newer build of a tool T, the old copy goes back over
hers, owned by her and labelled again.
"Start hidden" is E10's start, and "close" is the visit's close.

1. **E11. Fallbacks,** run only if the 4 GB check finds memory tight,
   since E10 passed.
   - (a) Headless labwc in place of cage, with an empty configuration
     folder.
   - (b) E10's start with a light session
     (`XDG_SESSION_CLASS=background-light`) and `dbus-run-session`
     before cage, so no user manager runs for `ada`.

   Passes as E10 did: signed in within 3 minutes, closed within 25 s,
   `bootstrap_log.txt` ending `Shutdown`, and nothing of `ada`'s left.
   Fails on a web helper crash loop, or no sign-in.
2. **E12. What a hidden Steam fetches by itself.**
   - (a) A stale T that `ada` last played more than 3 days ago: start
     hidden, leave it 45 minutes, then close.
   - (b) `ada` plays a game that uses T for a minute (Pajama Sam 2 for
     scout and soldier, Arthur's Birthday for Proton and Steam Linux
     Runtime 4.0), T is made stale again, and (a) is repeated.

   Watch T in `content_log.txt` (`Update Required`, `not played for N
   seconds`, `Update delayed`, `finished update`) and in its manifest
   (`buildid`, `StateFlags`, `ScheduledAutoUpdate`).
   During T's download, record whatever says how far it has got: the
   `update started` line, the rate and `stats:` lines, and
   `BytesDownloaded` in its manifest over time.
   Record the window names too.
   Passes if (a) updates T: no lever is needed.
   If (a) waits while (b) updates, Steam is waiting, and E13 decides
   whether validating fetches T ("Updates Steam puts off").
   Fails if neither updates T: the button covers only Steam's own
   update, and the warning and the results are amended to say so.
3. **E13. Validate or launch a tool.**
   T is made stale and Steam started hidden.
   After the sign-in, as `ada`, with her hidden display, run
   `/usr/bin/steam steam://validate/T`.
   Then T is made stale again, and `/usr/bin/steam -applaunch T` is run.
   Watch for 10 minutes: `GameAction [AppID T, ...]` in
   `console_log.txt`, `content_log.txt`, and the window names.
   Passes when T reaches Valve's build with no window left waiting.
   Fails when a dialog waits, as in E9, or nothing happens.
4. **E14. The "Immediately" value.**
   In a visit, set a game's Properties, Updates, to each of its four
   choices; after each, run `steam -shutdown` and read the manifest's
   `AutoUpdateBehavior`.
   With `ada`'s Steam stopped, put that value into a stale T's manifest,
   then repeat E12(a) for 15 minutes.
   Passes when T's update starts within 5 minutes and the value stays.
   Fails when T waits, or the value does not stay.
5. **E15. Steam updates itself out of sight.**
   It needs an update of the client: Valve's next, or
   `-forcesteamupdate -forcepackagedownload` added to the start once,
   undocumented options that repair tools use.
   Start hidden, follow `updateui_child.txt` and the window names, and
   note the process ids of cage and its child around each `Update
   complete, launching Steam...`.
   Also, with `ada`'s Steam stopped, run `/usr/bin/steam
   -forcesteamupdate -exitsteam` as her: does the update install with no
   `Startup - Steam Client launched` line?
   Passes when the session survives every restart and Steam comes back
   signed in.
   Fails when cage ends at a restart; then the helper starts it again,
   and this ADR says so.
6. **E16. Sign-in, network and disk.**
   - (a) `ada`'s network cut alone, with nftables matching her uid; start
     hidden for 10 minutes. Run on 2026-10-31 (below).
   - (b) A child whose Steam ran but never signed in: start hidden for 5
     minutes.
   - (c) Low disk, after a snapshot: with a stale T, fill `/var` until
     less than T's download plus 200 MB is free, repeat E12(a) for 10
     minutes, then free the space.

   Record `LogonFailure`, `SetLoginState`, `Download failed`, the
   content log's result, and the window names.
   Passes when each case leaves a clear line within 3 minutes.
   Fails when only the window names tell.
   No account is ever deauthorised for this: the kids' laptop already
   recorded `Access Denied`.
7. **The 4 GB check** (answer 5).
   On a computer with 4 GB, with the parent logged in to Plasma and
   their own Steam open and signed in, a child's update runs to its end.
   Record free memory and swapping with `vmstat`, the peak of the
   update's session, and whether the parent's desktop stays usable.
   The image swaps to compressed memory (zram), as Fedora does, so a
   little swap in use is normal.
   Passes when the computer does not swap: pages swapped in and out stay
   few, and the desktop stays usable.
   Fails when it does: then this ADR is amended so that an update closes
   the parent's Steam first, and E11 runs.
8. **The sleep check** (answer 4).
   In cairn-iso, with the Guardian logged in to Plasma, root holds the
   update's lock: `systemd-inhibit --what=sleep:idle --mode=block
   --who=Cairn --why=test sleep 900`.
   Then, as the Guardian: `systemctl suspend`, as ADR-0041's step 6 runs
   it; Plasma's own Sleep, a stand-in for the lid, which a VM lacks
   (inference: both ask logind the same way); and Plasma's sleep timer at
   1 minute, with nobody at the keyboard.
   All again with `--what=idle` alone.
   On a laptop, if the project has one, the lid too, with each lock.
   Passes when, with the update's lock, the sleep timer does not suspend
   the computer, and Plasma's Sleep and the lid do.
   Fails when they do not: then this ADR is amended as "Power, sleep and
   memory" says, with what `--what=idle` alone did.

### Run so far, 2026-10-31

- **E10 passed:** signed in after 10 s, closed in 4 s, `Shutdown` at the
  end of `bootstrap_log.txt`, nothing of `ada`'s left (Context).
- **E16(a) ran.** The one clear line was `result=NoLAN` in
  `connection_log.txt`, so the watcher reads that log, and an offline run
  ends at once instead of waiting out its limit.
  `steam -shutdown` did not close the offline Steam, so the close keeps
  its second half.
- **E17 ran.** (a) passed. (b) passed with the main process killed and
  failed with the whole unit killed, which is the known limit above.
- **Not run:** E16(b), since the two test accounts whose Steam never
  signed in are kept for ADR-0046's check 1; E12 to E14 and E16(c),
  which need a newer build of a tool from Valve; E15, which needs an
  update of Steam itself, Valve's next or a forced one; the 4 GB check;
  and the sleep check.

### Once the pieces are built

In cairn-iso, with an image built from the pieces, the Guardian logged
in to Plasma, `ada` and a second child.

1. **A run to its end:** the card's stage lines and a result, Cairn in
   `systemd-inhibit --list` meanwhile, and Ada able to log in at once
   afterwards.
2. **Stop**, during a download, and again with `ada`'s network cut, as
   in E16(a), whose close took 26 s.
   Passes when the card gives Stop's sentence, nothing of `ada`'s is
   left, and she can log in at once.
3. **The release:** the helper's main process killed (`kill -KILL`)
   mid-run.
   Passes when the release ends `ada`'s hidden Steam and frees the list,
   the card gives the sentence for `failed`, and she can log in.
4. **A killed unit** (`systemctl kill -s KILL`) mid-run: `ada` stays
   refused, and her hidden Steam runs on.
   Then a Save for the second child.
   Passes when its copy closes `ada`'s Steam and frees the list first,
   and she can log in afterwards.
5. **A refused press:** with the second child's visit open, an update
   for `ada` is held `busy`.
   Passes when the visit's Steam carries on, and the second child is
   still refused at login until the visit closes.
6. **`updating`:** during `ada`'s update, a Save holds the second
   child's copy `updating`, and **Log out** and **Open Steam** for the
   second child each say that a child's Steam is being updated.
7. **The window:** closed and opened again mid-run, it shows "Ada's
   Steam is being updated." with **Stop**, which asks for the password
   and lets Ada in.
   The update also runs on after the Guardian logs out.
8. **The results** the VM can bring about: no internet; a Steam never
   signed in, with E16(b)'s child; too little space before the start;
   and, on a laptop, on battery.
   `journalctl -t cairn-family -t cairn-steam-visit` shows the same
   words, and no Steam name or path.

## Alternatives considered

- **Headless labwc,** the kiosk's compositor, which ran Steam on the
  kids' laptop: a second configuration to keep. It stays E11's
  fallback.
- **`kwin_wayland --virtual`, Xvfb, headless gamescope:** heavier; Xvfb
  is a new package in the image; gamescope needs Vulkan, and crashed on
  exit on the dev PC.
- **`systemd-run --user -M`:** it killed Steam after 10 s and left
  sessions stuck until a restart (`docs/research/steam-visit.md`).
  **Linger** was refused in ADR-0032. **A unit without PAM** gives Steam
  no `XDG_RUNTIME_DIR`.
- **SteamCMD:** its own sign-in and Steam Guard, one login per Steam
  account at a time, and a new dependency.
- **Steam's web debugging port:** any account on the computer, a child at
  L3 or L4 with a terminal among them, could drive the child's Steam.
- **A PAM service of its own, `cairn-steam-update`:** clearer journal
  lines, but both rules that decide whether a child is logged in would
  have to learn it, or the Games page would call Ada logged in during
  her update.
- **A run like a visit,** one root process through pkexec, closed by a
  word on its input: Stop would need no second password, but closing the
  window or logging out would end the update too.
- **No Stop:** Mason chose one (answer 1).
- **A polkit rule that lets Guardians stop the unit with no password:**
  answer 1 asks for the password again in a window opened later, and a
  new rule would sit at a trust boundary.
- **Closing the parent's own Steam during an update:** it saves about
  1.4 GB, but closes the parent's games and downloads. Answer 5 keeps it
  open unless the 4 GB check says otherwise.
- **Every child at once, or at night:** answer 6 leaves both to an ADR of
  their own.
- **Copying newer builds from the parent's Steam over a child's own:**
  ADR-0046 never touches a tool the child's Steam has, and the client's
  own files are far past ADR-0033's "writes none of Steam's files of its
  own". Sharing one download is for a later ADR.
