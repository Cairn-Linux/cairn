# ADR-0045: The grown-up screen says what will help, when the launcher knows

**Status:** accepted (2026-10-30). The line was seen on the grown-up
screen in a child's session on the test VM, after a launch that starting
Steam again could not mend, and logging the child out and back in then
started the game (#206; "Checking it", below).
**Date:** 2026-10-30
**Closes:** (none) — part of issue #206; amends ADR-0030; part of ROADMAP
P1-13

## Context

ADR-0030 put "Something needs a grown-up." and "Pajama Sam 2 did not
start." on the screen for a launch that shows nothing, with Back and no
reason. The launcher could not read Steam's dialogs, and a guess would be
worse than nothing. The reason goes to the journal, and `docs/TRY.md`
tells the grown-up what each journal line means.
[DESIGN §8.3](../DESIGN.md#83-steam-and-proton-available-but-never-visible),
mitigation 2, asks for a plain "something needs a grown-up" screen; this
ADR adds one line to it, and §8.3 now says so.

Since then the launcher has learned to tell some cases apart from Steam's
logs. In one of them it also knows the remedy. A new child's first Linux
game can stop part-way: Steam takes the game off its download schedule and
never asks for the second tool the game runs in (#206). The launcher now
starts Steam again once to mend it (ADR-0036). When that does not help,
logging the child out and back in is what is left. On the test VM on
2026-10-30, logging out and back in mended the first stall, and the game
started within a minute; starting only Steam again mended the second.
Logging out ends every process of the child's Steam, so it also covers a
restart that could not be done or did not help. The grown-up screen did
not say so, and a grown-up who does not read the journal would not know.
PR #196 asked the same question for a Steam that cannot sign in (#182).

The child reads that screen too. Whatever it says must be calm, short and
true, and must not frighten or blame anyone.

## Decision

**When the launcher knows what will help, the grown-up screen adds one
short line saying it, under "… did not start.".** Never a reason, a code
or a program's own words, and never more than one line. It is written as
the other sentences are: calm, sentence case, at the sentence's size
(`Tokens.headingSize`, 28 px, above the 18 px floor for a child), in
`qsTr()` so it can be translated, and read out with the screen
(`Accessible.description`). C++ decides which line it is
(`AppLauncher.grownUpStep`, an enum); QML only draws it.

Today there is one line:

> Log out and back in, then try it again.

It shows on the grown-up screen after a failed launch in which Steam
stopped part-way through getting the game ready, however the launch then
ended, and after a later tap on a game Steam still keeps stuck that way.
It never shows when Steam's logs gave another reason (a refused sign-in,
no connection, a question on a hidden window, or a refusal), because
logging out is not what helps then, and never for a window that opened on
its own, a tile that is coming soon, or a launch that went on to start.
The journal's own line says the same, with the reason first (ADR-0036).

It is an instruction on purpose. A grown-up reads it as "log the child
out". A child can follow it alone: Log out is theirs to press (ADR-0021),
and a child at L1 or L2 is logged back in with one press (ADR-0020).
"Try" fits a keyboard, a mouse and a touch screen alike, as "Try
something else." does on the coming-soon screen.

Each later case decides its own line, if any. A line is added only for a
case that Steam's logs identify, and only with a remedy that was seen to
mend it. #182's question stays open: whether "Steam needs to be signed in
again." belongs on this screen, or in the Guardian tool.

## Consequences

- A grown-up who does not read the journal is told what to do for the
  case in #206. A child who can read may do it alone.
- ADR-0030's screen is no longer free of advice. A wrong line would send a
  grown-up the wrong way, which is why there is one line, wired for one
  case, and why it gives way to any other reason Steam's logs give.
- A later tap on a game Steam keeps stuck shows the line only while
  Steam writes no step for that game. If Steam writes one, such as the
  launch failing, the line goes and the journal gives Steam's reason.
- The enum leaves room for more lines without changing the screen's
  layout. Each new one needs its own C++ case, its own string and its own
  tests.
- Tested in C++: the line comes after a stall the launcher could not mend,
  after a stall that came back after the restart, after a restart that got
  the game ready but brought no window, and on a later tap of the stuck
  game, also one whose stall ended its tap before Steam was started again;
  it does not come after a mended stall, after a stall followed by a
  refused sign-in, after a stall seen while Steam could not reach its
  servers or sign in, after a launch that failed without a stall, nor for a
  window that opened on its own. In QML, the line is hidden without a step,
  and shown, named for a screen reader and at least 18 px with one.

## Checking it

On the test VM a new child's Steam stopped part-way at the first tap on
Pajama Sam 2, and did so again once scout and soldier were taken out of
it: 2 times of 2 (ADR-0036). The launcher stops only the Steam it started
at login, `cairn-steam`, so a child's Steam started under another name is
one it will not stop, and the stall stays unmended. To see the line:

1. Take scout (1070560) and soldier (1391110) out of a child's Steam.
2. In that child's session, stop Steam with `steam -shutdown`, and start
   it again under another unit name:

   ```
   systemd-run --user --unit=other-steam --collect --quiet --setenv=DISPLAY --setenv=WAYLAND_DISPLAY -- steam -silent
   ```

3. Tap Pajama Sam 2. "Getting it ready" shows for a minute or two, and
   then the launch ends on the grown-up screen with "Log out and back in,
   then try it again." The journal's lines include, in this order: "Steam
   stopped getting Pajama Sam 2 ready; starting Steam again.", "Steam is
   not running as the launcher started it, so it was not started again.",
   "Steam stopped getting Pajama Sam 2 ready, so Pajama Sam 2 did not
   start. Log the child out and back in, then try it again." and "Nothing
   of Pajama Sam 2 came on the screen in time."
4. Log the child out and back in, and tap Pajama Sam 2 again. The game
   starts.

Done on the test VM on 2026-10-30 with this change's build. The tap at
21:08:41 fetched scout, and Steam took the game off its schedule at
21:09:07. At 21:10:12 the launcher found that Steam was not running as it
had started it, and left it alone. At 21:10:27 the launch ended on the
grown-up screen with the line, 1 minute 46 seconds after the tap, where it
used to wait 10 minutes. The journal had the four lines above, in that
order. After the child logged out and back in, the next tap fetched
soldier in 37 seconds, and the game started 41 seconds after the tap.
