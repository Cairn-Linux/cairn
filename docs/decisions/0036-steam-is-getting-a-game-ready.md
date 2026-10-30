# ADR-0036: Wait while Steam's log says it is getting a game ready

**Status:** proposed. It becomes accepted when a tap on a game that needs
an update has shown the sentence on the kids' laptop and the game has
taken over from it.
**Date:** 2026-10-20
**Closes:** (none) — amends ADR-0029 and ADR-0030; part of ROADMAP P1-13
**Amended by:** ADR-0037 (Steam updating itself is work too; a tap lost to
its restart is sent again); the section "A Steam that cannot sign in"
below (2026-10-29, #182); the section "Steam stops part-way on a new
child's first Linux game" below (2026-10-30, #206)

## Context

ADR-0029 waits for a Steam game past the usual 15 s grace on two facts:
the Steam client started at login a short while ago, or Steam's `reaper`
for the game is running. It named the gap this ADR closes: with Steam
up, a game Steam must first download or update has no reaper until that
is done.

On 2026-10-20 the tester account on the kids' laptop pressed Arthur's
Birthday. Steam had held back two updates for days, the Steam Linux
Runtime and Proton Experimental, and the tap made them due. Steam
downloaded them for 40 s with no reaper, then ran the game's install
script under one. At 29 s the grown-up screen said the game did not start;
the game's window came at 60 s and took over (ADR-0030). A child would have
pressed Back, and the game would then have opened on its own over the
tiles.

Steam writes each step of a launch to its own log,
`~/.local/share/Steam/logs/console_log.txt`, as it goes:

```
GameAction [AppID 2471140, ActionID 1] : LaunchApp changed task to DownloadingDepots with ""
GameAction [AppID 2471140, ActionID 1] : LaunchApp changed task to ProcessingInstallScript with ""
GameAction [AppID 2471140, ActionID 1] : LaunchApp changed task to Completed with ""
```

The laptop's logs from 2026-10-07 to 2026-10-20 show three kinds of last
line for a launch that has not finished. A **step**, changed task to
something, ends by itself. A **question**, waiting for user response to
`SynchronizingCloud` "syncfailed", is the offline cloud-sync dialog behind
ADR-0030; 13 launches stopped there. A **refusal**, failed with
`AppError_N` and then Failed, ends the launch. Questions about
`ShowInterstitials`, `CreatingProcess` and `ProcessingShaderCache` are
answered in the same second.

The child should not stare at the stones for minutes either. ADR-0028
says motion says something true. If the screen moves for a minute or more,
it should also say why.

## Decision

**While Steam's log says it is working on the game, the launch waits, and
the starting screen says the game is getting ready.** `SteamLaunchLog`
reads the lines Steam has added to its log since the tap, about that app id
only. The last of them decides. A step, or a question already answered,
is work. A question still open, a refusal, or the game's own process
starting (`WaitingGameWindow`, `Completed`) is not work.

When a grace ends with no window of the app up, the launch waits another
grace if Steam's log says it is working, for up to 10 minutes from the
tap. ADR-0029's two facts still count, but their 2-minute limit is now
counted from the last time Steam's log showed work, not from the tap. So a
game whose download ends after 3 minutes, or after 9, still gets 2 minutes
for a Proton game's first window, and the longest wait is about 12
minutes. If no work is ever seen, nothing changes from ADR-0029.

While the wait is held up by work, the starting screen says a second
sentence under "Arthur's Birthday is starting.":

> Getting it ready. This can take a few minutes.

It comes only after the first grace, so a game Steam starts at once never
shows it. It goes when the work does, or when the state leaves Starting.

**A question or a refusal changes nothing yet.** Neither keeps the launch
waiting, so the grown-up screen comes at the end of the grace, as it does
today. The journal says which it was, for the Guardian: "Steam is waiting
for an answer about Arthur's Birthday on a window the kiosk hides." or
"Steam refused to start Arthur's Birthday." Ending the launch sooner once
the log shows a question or a refusal is a separate decision. A second tap
on a game already stuck on a question adds nothing to the log. Telling
that apart needs lines from before the tap, which may belong to an earlier
Steam session.

## Consequences

- A game that needs an update first keeps the starting screen up, with the
  sentence, until its window comes. It no longer passes through the
  grown-up screen, and pressing Back no longer makes the game open on its
  own over the tiles.
- The log is read only when a grace ends, every 15 s, and only the part
  added since the last read, at most 4 MB of it.
- The launcher depends on the wording of Steam's log, which Valve does not
  promise to keep. If Valve changes it, the lines stop matching and the
  launcher waits as ADR-0029 does: a lost feature, not a wrong one. A Steam
  in a Flatpak keeps its log under `~/.var/app`, which the launcher does
  not read; the client today is the system one (ADR-0027).
- A game whose download takes more than 10 minutes, on a slow line or a
  big update, still ends on the grown-up screen, and its window still takes
  over if it comes (ADR-0030).
- Steam updating its own client at login is not a game step. The launcher
  still waits for that only on ADR-0029's 60 s.
- The sentence does not name Steam or what it is downloading. A child has
  no use for either. The Guardian finds them in the same log.
- Tested in C++ with real lines from the laptop. Each kind of line is
  sorted correctly. Only lines after the tap count, and the last line about
  the game decides. A half-written line is read once it is whole. A new log
  is read from its start. Work keeps a launch waiting past the 2-minute
  limit but not past the work limit, and the reaper's limit restarts after
  the last work, also when the work ends just before the work limit. A question or refusal does not keep a launch waiting. An
  `AppLauncher` test runs a download to the game's window. Another has the
  sentence go when the download ends and the game is still starting.
  Others run a download, then a cloud question or a refusal, to the
  grown-up screen, each with its journal line. In QML, the
  sentence shows only while the game is getting ready.

## A Steam that cannot sign in (2026-10-29, #182)

On the kids' laptop on 2026-10-27 a tap came while the tester account's
Steam was starting. Steam's servers turned its saved sign-in down, and the
client waited for a person to sign in on a window the kiosk hides. It never
took the tap. Because it never finished starting, ADR-0037 counted it as
work, and the starting screen said "Getting it ready" for 10 minutes.

Steam writes each try to sign in to `console_log.txt`, and the line it
writes once it has signed in:

```
[2026-10-27 23:12:03] LogonFailure Access Denied
[2026-10-27 23:13:07] LogonFailure No Connection
[2026-10-28 13:14:59] Waiting for compat in post-logon took: 0.096359s
```

Its connection log says which failures it tries again by itself: `No
Connection`, also `Service Unavailable`, `Try another CM`, `Timeout` and a
bare `Failure`. Its first five tries come 8 to 90 s apart, and later ones 2
to 14 minutes apart (development laptop, 2026-10-08 to 2026-10-28). After `Access Denied` it says `Do not
reconnect`, and its later tries, made without an account, all fail with `No
Connection`. Other refusals in Steam's list, such as `Invalid Password` or
`Account Logon Denied`, also need a person.

`SteamSignIn` reads the lines of the client that runs now: those since the
updater last started it and since the client last wrote `Client version:`.
A refusal counts until Steam next signs in. A run of failures Steam tries
again counts as no connection.

**While Steam's log says nothing about the game since the tap:**

- **A refused sign-in ends the wait** at the end of the grace it is seen
  in. The grown-up screen comes, as for any launch that shows nothing.
  The journal says "Steam could not sign in, so Putt-Putt Travels Through
  Time did not start. Steam needs to be signed in again."
- **No connection gets a short wait**, one minute from the tap. This is
  a chosen wait, not a count of Steam's own tries: a Steam offline for a
  while next tries minutes later, and a child should not watch a frozen
  screen that long. Then the grown-up screen comes. The journal says
  "Steam could not reach the internet, so … did not start."
- Neither counts as work, so neither shows "Getting it ready". Neither
  sends the tap again.

Once Steam's log names the game, its sign-in no longer decides. Steam
starts games while it cannot reach its servers, as on 2026-10-28 on the
development laptop, and ADR-0030's offline cloud question still comes as
before.

The grown-up screen still does not say why (ADR-0030). Whether it gets a
short line for the grown-up, such as "Steam needs to be signed in again.",
is left open: the child reads that screen too, and the line could live in
the Guardian tool instead. Signing a child's Steam in again is #131.

Tested in C++ with lines from the kids' laptop and the development laptop.
A refusal counts through the `No Connection` lines after it and ends when
Steam signs in. No connection lasts until Steam signs in. Lines from an
earlier client do not count, nor do a client's old lines while the updater
installs a new one. Each reason is sorted. A refusal ends the wait with
no sentence and no second tap. No connection gets the short wait, ends
sooner if Steam signs in, and never stops a game Steam has in hand. Two
`AppLauncher` tests run to the grown-up screen with the journal line: the
evening of #182, and a Steam with no connection.

## Steam stops part-way on a new child's first Linux game (2026-10-30, #206)

A native Linux game runs in Steam Linux Runtime 1.0, scout (1070560).
Scout itself runs inside Steam Linux Runtime 2.0, soldier (1391110): its
`toolmanifest.vdf` says `"require_tool_appid" "1391110"`. Each account's
Steam fetches its own tools (ADR-0033), so a new child's Steam has neither.

On the test VM on 2026-10-30 a new child tapped Pajama Sam 2 (292780). It
went the same way again after both tools were taken out of the child's
Steam (2 of 2). Steam fetched scout, took the game off its download
schedule, and never asked for soldier. Its launch stayed at
`DownloadingDepots`, and nothing more happened:

```
17:10:08 console  GameAction [AppID 292780, ActionID 1] : LaunchApp changed task to DownloadingDepots with ""
17:10:08 content  AppID 1070560 update started : download 0/75258128, store 0/0, reuse 0/0, delta 0/0, stage 0/223012461
17:10:28 content  AppID 1070560 finished update, 1 mounted depots (BuildID 26115262) : 1070561 (8475493586714137016),
17:10:28 content  AppID 292780 scheduler finished : removed from schedule (result Suspended, state 0xc)
```

`DownloadingDepots` counts as work, so the starting screen said "Getting it
ready" for this ADR's 10 minutes, and then the grown-up screen came. A
second tap was taken by the stuck launch. A launch that works writes the
same "removed from schedule (result Suspended)" line for the game, and its
next step, `RunningInstallScript`, in the same second (2 of 2 on the VM).
A Windows game did not stop: for Arthur's Birthday, Steam scheduled Proton
Experimental (1493710) and the runtime it needs (4183110) together.

Starting only the child's Steam again mends it. By hand on the VM, `steam
-shutdown` ended the `cairn-steam` service after 4 s. Steam was started
again with the launcher's own command from the login, and the tap was sent
again a few seconds after the new client's start line. Steam fetched
soldier from 17:13:00 to 17:13:41, and the game started at 17:13:41, 58 s
after the restart. The launcher, still inside its work limit, handed over
to the game's window by itself.

**When Steam stops this way, the launcher starts the child's Steam again,
once in each tap, and sends the tap again. The child sees "Getting it
ready" throughout.**

`SteamContentLog` reads Steam's `content_log.txt` from the tap, as
`SteamLaunchLog` reads `console_log.txt`, and `SteamStall` decides. At the
end of a grace, the launch has stopped when all three hold:

- The last launch step about the game since the tap is `DownloadingDepots`.
- The last thing Steam did for the launch, in its content log, was taking
  the game off its schedule (`scheduler finished : removed from schedule`),
  whatever the result, and none of the launch's downloads still runs.
- That line is at least a minute old, by its own time.

Only the game and the apps Steam put at the front of its schedule since the
tap (`scheduler update : Priority First`) count: those are the tools the
launch needs. Steam resumes other games' downloads the moment a launch's
apps leave the schedule (dev PC, 2026-10-30), so those do not count. Of
the lines about the launch's apps, only those about the schedule and the
downloads count: `scheduler update`, `scheduler finished`, `update
started`, `update changed`, `finished update` and `update canceled`. A
download ends at `finished update`, at `update canceled`, or at the app's
own `scheduler finished` line, because a shader cache's download never
writes `finished update` (dev PC, 2026-10-30).

A minute is far past the second in which a launch that goes on writes its
next step, even on a slow laptop, and leaves about 8 of the 10 minutes for
the restart, which took about a minute by hand. The rule does not fire on
a slow download, because the game stays on the schedule until every
download for it ends. It does not fire when Steam updated itself and lost
the tap (ADR-0037), because the launch log then has no step about the
game. "Result Suspended" alone does not make a stall.

Steam is started again only when it has started (`SteamStartup` says it
is ready, ADR-0037), has signed in (#182), and runs no new reaper for the
game. `SteamRestart` then:

1. Asks the child's systemd whether `cairn-steam` is active. If it is not,
   it stops nothing. Asked to shut down when it is not running, Steam
   starts instead, and only the client the launcher started at login is
   its to stop. That leaves alone a Steam that a tile started, and the
   dev PC's own Steam.
2. Runs `steam -shutdown`, so Steam saves its state as it quits.
3. Asks again every second, for up to 25 s as `cairn-steam-visit` waits
   for a child's Steam, until the service is `inactive` or `failed`. A
   service started with `--collect` has no process left then, and its
   name is free.
4. Starts Steam with `SteamAtLogin`'s command, as at login.

It never kills Steam. Logging the child out ends it for certain, and that
is what the journal asks for when the restart does not help.

Once Steam runs again, only what the new client writes counts, in both
logs. The tap is sent again by ADR-0037's rule: at the first look at least
3 s after the new client's own start command, once. Until the new client
takes the tap, the launch waits as work, so "Getting it ready" stays, even
while the old client writes that its launch failed as it stopped. A
restarted client that cannot reach its servers gets #182's minute from the
restart, not from the tap.

**If the restart does not help, the wait ends soon.** The launch ends on
the grown-up screen at the next look after any of these:

- Steam could not be started again.
- Steam, started again and ready, has not taken the tap 2 minutes after its
  own start command, or 2 minutes after it was started if it never runs
  that command. Starting or updating itself before that is work, under the
  10-minute limit.
- The stall comes back after the tap sent again. There is no second
  restart in the same tap.

A game that still never comes after a good second tap ends by the 2-minute
rule after the last work, as before. Steam takes a later tap on a game it
keeps stuck without writing a word, so that tap ends at its first look.

The journal says:

1. "Steam stopped getting Pajama Sam 2 ready; starting Steam again."
2. "Steam is not running as the launcher started it, so it was not started
   again."
3. "Steam did not stop when asked, so it was not started again."
4. "Steam could not be started again."
5. "Steam stopped and was started again."
6. "Steam started again without taking Pajama Sam 2; asking again."
   (ADR-0037)
7. "Steam stopped getting Pajama Sam 2 ready, so Pajama Sam 2 did not
   start. Log the child out and back in, then try it again." This comes
   when the wait ends after a stall, or on a later tap of a game Steam
   keeps stuck, and Steam gave no other reason. A refused sign-in, no
   connection, a question or a refusal is named instead, as before.
8. "Nothing of Pajama Sam 2 came on the screen in time." (ADR-0030)

`systemctl --user restart cairn-steam` would end Steam with a signal, not
with its own shutdown that saves its state. A longer wait would still
never bring soldier.

Consequences:

- On the VM, a first tap that stops this way should bring the game about 3
  minutes after the tap, with "Getting it ready" throughout.
- The rule depends on the wording of two Steam logs. If Valve changes it,
  the stall is not seen and the launcher waits as before: a lost feature,
  not a wrong one.
- Only `cairn-steam` is ever stopped. A Steam that will not stop is left
  for logging out to end.
- Both logs are read only when a grace ends.
- A stall inside a "Steam for" visit is not handled.
- Fetching each child's tools ahead of time (#181) would make the stall
  rare.

Tested in C++ with the VM's log lines. `SteamContentLog` finds the stuck
run's removal, and loses it to every later line that means Steam went on;
other games' downloads, Steam's own ticket, state, space and rate lines
change nothing; a tool still downloading, or a shader cache's download
before its scheduler line, is not the end. `SteamStall` sees the stuck run
after a minute and not a second before, never the working run, nor a
question, a refusal, a started game or no word; asks for one restart in
each tap; waits for a Steam that is starting or updating itself again, and
gives one that has started 2 minutes from its own start command; and is
stuck after a failed restart or a second stall. `SteamRestart` runs
against stand-ins: a Steam the launcher started stops and starts again with
the login's command, one it did not start is never told to shut down, one
that will not stop is not started again, a service still stopping is
waited for, and a failed start says so. `SteamGameWait` restarts once,
keeps "Getting it ready" while Steam stops and starts again, sends the tap
again once, and ends the wait after a second stall or a failed restart; it
never restarts a Steam that is updating itself, offline or running a new
reaper; a restarted Steam gets its own offline wait. Three `AppLauncher`
tests run the stuck run to the game's window with one restart and one tap
again, a stall that comes back, and a Steam the launcher did not start;
another has a later tap of the stuck game end at its first look, and
another a refused sign-in after the restart.

This section is checked when, on the test VM with scout and soldier taken
out of a child's Steam, the first tap on Pajama Sam 2 starts the game with
no grown-up (#206).
