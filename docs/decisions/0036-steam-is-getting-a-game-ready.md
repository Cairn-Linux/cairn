# ADR-0036: Wait while Steam's log says it is getting a game ready

**Status:** proposed. It becomes accepted when a tap on a game that needs
an update has shown the sentence on the kids' laptop and the game has
taken over from it.
**Date:** 2026-10-20
**Closes:** (none) — amends ADR-0029 and ADR-0030; part of ROADMAP P1-13
**Amended by:** ADR-0037 (Steam updating itself is work too; a tap lost to
its restart is sent again)

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
