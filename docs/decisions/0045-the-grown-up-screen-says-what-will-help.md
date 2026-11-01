# ADR-0045: The grown-up screen says what will help, when the launcher knows

**Status:** accepted (2026-10-30). The line was seen on the grown-up
screen in a child's session on the test VM, after a launch that starting
Steam again could not mend, and logging the child out and back in then
started the game (#206; "Checking it", below).
**Date:** 2026-10-30
**Closes:** (none) — part of issue #206; amends ADR-0030; part of ROADMAP
P1-13
**Amended by:** the section "The cloud question, once Steam has signed in
again" below (2026-10-31, #211)

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
logging out is not what helps then. The one exception is a later tap on a
game Steam holds behind its question about the game's cloud saves, once
Steam has signed in again ("The cloud question, once Steam has signed in
again", below, #211). It never shows for a window that opened on its own,
a tile that is coming soon, or a launch that went on to start. The
journal's own line says the same, with the reason first (ADR-0036).

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
  grown-up the wrong way, which is why there is one line, wired only for
  cases where logging out was seen to mend the launch, and why it gives
  way to any other reason Steam's logs give. Since 2026-10-31 there are
  two such cases: the stall, and the cloud question once Steam has signed
  in again ("The cloud question, once Steam has signed in again", below,
  #211).
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

## The cloud question, once Steam has signed in again (2026-10-31, #211)

Offline, Steam asks about a game's Steam Cloud saves on a window the kiosk
hides, and keeps the question after it signs in again (ADR-0036, "A
question Steam keeps"). Each later tap on that game ends on the grown-up
screen, and the journal names the question. On the test VM on 2026-10-31,
logging the child out and back in freed the game, and the next tap
started it a second later, in #211's report and again in #230's check. So
Steam's logs identify the case, and the remedy was seen to mend it, as the
Decision above asks of a new case. Mason chose on 2026-10-31 to show the
line for it.

**Once Steam has signed in again, a later tap on a game Steam holds
behind its cloud question shows "Log out and back in, then try it
again."** The cloud question is the one Steam's log writes as `waiting
for user response to SynchronizingCloud "syncfailed"`. A later tap is one
Steam takes on the game without writing a step for it, after an earlier
tap met the question (`SteamQuestions`, ADR-0036). Steam has signed in
again when its log shows no failed sign-in since it last signed in, the
reading #182 uses (`SteamSignIn`). Steam writes `Waiting for compat in
post-logon took:` each time it signs in, also when it reconnects without
a new client, as on the development laptop on 2026-10-28 after a run of
`LogonFailure No Connection` lines. The journal's line says the same as
the screen, with the reason first: "Steam is waiting for an answer about
Pajama Sam 2 on a window the kiosk hides. Log the child out and back in,
then try it again."

There is no line, and the journal names the question alone:

- at the tap that met the question, whatever the sign-in says: Steam had
  just failed to check the game's saves, so a sign-in that reads fine is
  least sure then, and the next tap shows the line if logging out still
  helps;
- while Steam cannot reach its servers: logged back in, Steam would only
  ask the same question again;
- while Steam's sign-in is turned down: logged back in, the next tap would
  meet the sign-in, which needs a grown-up (#182);
- for any other question Steam keeps: what frees its game is not known.

The sign-in decides only the line. A tap on a held game still ends at its
first look and names the question, not the sign-in (ADR-0036).

Consequences:

- A grown-up who reads only the screen is told what frees the game, and a
  child who can read may do it alone, as for #206.
- The line rests on Steam's wording. If Valve changes the words of the
  question, or of the line Steam writes when it signs in, the line stops
  coming: a lost feature, not a wrong one. If Valve changes how Steam
  writes a failed sign-in (`LogonFailure`), `SteamSignIn` reads fine while
  Steam is still offline, and the line shows when logging out would not
  help. #182's short offline wait would stop working at the same time.
- `SteamSignIn` counts Steam as signed in whenever nothing in its log says
  otherwise. A Steam that has lost the internet and not noticed yet, or
  whose cloud check failed while it stayed signed in, could get the line
  when logging out would not help. On the test VM on 2026-10-31, Steam
  wrote `LogonFailure No Connection` about six seconds after the VM's
  network link went down. Its log said the network device was lost in
  that same second, so the six seconds passed before Steam noticed. On the
  development laptop on 2026-10-28, the first one came two seconds after
  Steam's log said the device was lost; when the device itself went is not
  known there. How long any Steam takes when the Wi-Fi stays up and the
  line beyond it goes down has not been measured.
- No new string and no new value of `AppLauncher.GrownUpStep`, so the
  screen and its QML are unchanged.

Tested in C++: Steam's launch log tells the cloud question from Steam's
other questions, another question about the cloud saves among them, also
when lines about something else follow it, and a new tap or a new log
forgets it. `SteamQuestions` sees a later tap on a game the cloud question
holds, at every look, and does not see the tap that met it, a tap Steam
has not taken yet, a tap in which a new client asks again, another game,
also right after a held tap, or a game another question holds. The line
comes once Steam has signed in again, also when Steam could not be
started again and still holds the game. It does not come while Steam is
still offline, while its sign-in is turned down, at the tap that met the
question (with Steam signed in before that tap or during it), for another
question, for another game, for a tile that is not a Steam game, or once
Steam has written a step for the game or a new client has started. An
`AppLauncher` test runs the VM's case: the tap that met the question and
a later tap while Steam is offline end on the grown-up screen without the
line, two later taps once Steam has signed in again end at their first
look with it, and a tile that is not a Steam game after them gets no
line.

To check it on the test VM:

1. Log a child in, and wait until their Steam's `console_log.txt` has a
   `Waiting for compat in post-logon took:` line and a minute has passed
   since the login.
2. Take the VM's network down, and tap Pajama Sam 2, a game with Steam
   Cloud saves. About 15 seconds after the tap the grown-up screen comes,
   with no line under "… did not start.", and the journal names the
   question.
3. A minute later, with the network still down, tap it again: the same,
   with no line. Afterwards, check that `console_log.txt` has a
   `LogonFailure No Connection` line after Steam's last sign-in and before
   this tap. Until Steam writes one, `SteamSignIn` counts it as signed in,
   and the screen adds the line (the consequence about `SteamSignIn`
   above).
4. Bring the network back, and wait until `console_log.txt` has a new
   `Waiting for compat in post-logon took:` line after the `LogonFailure`
   lines.
5. Tap Pajama Sam 2. The grown-up screen comes about 15 seconds after the
   tap and adds "Log out and back in, then try it again." The journal
   says "Steam is waiting for an answer about … on a window the kiosk
   hides. Log the child out and back in, then try it again."
6. Tap another game, such as Arthur's Birthday: it starts.
7. Log the child out and back in, and tap Pajama Sam 2: it starts.

Run on the test VM on 2026-10-31 by these steps, with main b0b1efc and
this change's launcher laid over `/usr`. With ada's Steam signed in since
22:10:10, the VM's network link went down at 22:11:35. Steam saw its
network device go and wrote `LogonFailure No Connection` at 22:11:41,
about 6 seconds later. The tap at 22:12:02 met the cloud question: Steam
wrote `waiting for user response to SynchronizingCloud "syncfailed"` in
the same second. That launch ended on the grown-up screen 15 seconds after
the tap, with no line under "… did not start.", and the journal named the
question alone. The tap at 22:14:27, with the network still down, ended
the same way, and its two journal lines were the same word for word.
Steam's last sign-in line before that tap was `LogonFailure No
Connection`, at 22:13:31. The network came back at 22:15:08, and Steam
wrote `Waiting for compat in post-logon took:` at 22:15:10, with no new
client. The tap at 22:15:52 ended on the grown-up screen 15 seconds after
the tap, with "Log out and back in, then try it again." under "… did not
start.", at the same size. The journal said "Steam is waiting for an
answer about Pajama Sam 2: Thunder And Lightning Aren't So Frightening on
a window the kiosk hides. Log the child out and back in, then try it
again." A tap on Test Game Three, whose app id Steam will not launch,
then ended with no line, and the journal said only "Nothing of Test Game
Three came on the screen in time." Arthur's Birthday started: Steam had
it running 5 seconds after the tap. After ada logged out and back in,
Pajama Sam 2 started a second after the tap. There was no AVC denial.
