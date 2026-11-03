# ADR-0049: A grown-up steps in from their own login, not inside the child's session

**Status:** proposed. On 2026-11-02 Mason decided that child test 2
includes Steam games. After the research below, he decided the same day
that for that test a grown-up steps in by logging the child out and
fixing Steam from their own login, with one new line on the grown-up
screen ("What Mason decided then", below). The in-session step-in stays
open in #88. This ADR becomes accepted when checks 1 to 6 and 8 under
"Checks in the VM" have run without overturning a bullet of the
Decision.
**Date:** 2026-11-02
**Closes:** (none) — the narrow version of issue #88, for child test 2
(ROADMAP P1-17); the in-session step-in stays open in #88. It answers
the question ADR-0045 left open for #182, so #182 can close with this
change. Part of ROADMAP P1-13. Once accepted, amends ADR-0032, ADR-0036
and ADR-0045.

## Context

### What #88 found

On 2026-09-30, in the VM, Steam opened its "Sign in to Steam" window in
`ada`'s session after Putt-Putt quit.
The kiosk hid it, and the launcher said "Something needs a grown-up.
Sign in to Steam opened on its own."
The grown-up it asked for had no way to act.
The window could be dealt with only inside the child's session, and
nothing there let a Guardian in.
#88 asks for a way for a Guardian, with their own password, to step in
inside a child's session and fix what the grown-up screen names.

### What changed since

- **A hidden window never counts** (ADR-0026, 2026-10-01).
  Steam's hidden windows no longer bring up the grown-up screen by
  themselves, so #88's report cannot happen again in that form.
- **A launch that shows nothing ends on the grown-up screen**
  (ADR-0030).
  A Steam game held behind a window the kiosk hides ends there, with
  Back, and its window still takes over if it comes late.
- **The launcher reads the reason from Steam's logs** (ADR-0036,
  ADR-0045; #182, #206, #211).
  It tells four cases apart: a refused sign-in (Steam's servers turned
  the saved sign-in down, `LogonFailure Access Denied` and the like), no
  connection, a question Steam keeps on a hidden window, and a refusal.
  The reason goes to the journal.
  The screen adds one line only where a remedy was seen to mend the
  case.
- **A grown-up can open a child's Steam from their own login**
  (ADR-0032, proposed). One such opening is a *visit*.
  The child's own Steam runs as the child, in a run-only cage on the
  Guardian's desktop, while the child is logged out and refused at the
  login screen.
  It is built: "Steam for Ada" in the Guardian's menu, and "Open Steam
  for Ada" on the Family app's Games page.
  ADR-0032 said that until #88, fixing what Steam waits for means
  logging the child out, and that what the grown-up screen says about
  it would get an ADR of its own.
  Its check that a sign-in made in a visit reaches the kiosk has not
  run.

### The kiosk and the grown-up screen before this ADR

- labwc runs as the child: `cairn-session` execs it, and it runs
  Xwayland and the launcher.
  Two window rules iconify Steam's own windows (`steamwebhelper`,
  `steam`) when they first appear.
  A hidden window keeps its foreign-toplevel handle, so the launcher
  still hears of it.
- In Failed, the grown-up screen says "Something needs a grown-up." and
  "<game> did not start.", with Back.
  Under them went at most one line from `AppLauncher.grownUpStep`: none,
  or "Log out and back in, then try it again."
  Nothing on the screen refers to a hidden window.
- For a refused sign-in the screen added nothing.
  The journal said "Steam could not sign in, so <game> did not start.
  Steam needs to be signed in again."
  `docs/TRY.md` told the grown-up to log the child out and open Steam
  for the child from their own account.

### What Mason decided first

Child test 2 includes Steam games.
ROADMAP P1-17 asked for "Steam games with the overlay off (#74) and a
Guardian's way to step in (#88), or no Steam in that run".
Before the test, build #88's narrow version.
On the grown-up screen (ADR-0030, ADR-0045), a button asks for a
Guardian's password.
Then the hidden window it names, such as Steam's sign-in or a question
Steam asks, shows, so the Guardian can deal with it in the child's
session.
When that window closes, or after a time limit, containment is back.

#88's safety criteria are requirements:

1. Authentication happens in a trusted agent, root-owned or the
   Guardian's.
2. The Guardian's credentials never enter, and never become readable
   by, a process the child owns.
3. Only narrowly allowlisted windows or actions can be shown.
4. The child's input and apps are kept apart while the Guardian works.
5. Containment comes back by itself when the window closes or time runs
   out.
6. Audit tests cover a failed password, an abandoned prompt, and a
   child's window imitating the prompt.

### What the research found

On 2026-11-02 the research read the image built from main e005b7a
(`localhost/cairn:dev`), labwc 0.9.6, wlroots 0.19.3, polkit 127,
systemd 259 and SDDM 0.21.
Its raw notes are in the maintainer's work area.
What decides this ADR:

- **Every key typed in the kiosk passes through programs the child
  owns.**
  labwc, its Xwayland and the launcher are the child's processes.
  The image sets `kernel.yama.ptrace_scope = 0`, so any program of the
  child can read their memory and copy their open files.
  labwc offers every client without a Flatpak security context the
  protocols for an input method, a virtual keyboard, screen capture,
  overlays (layer shell) and the session lock.
  A program on the kiosk's Xwayland can read keys typed into another
  X11 window.
  So the child's programs can read a password typed into anything drawn
  in the kiosk: a polkit agent in the launcher, `pkttyagent` or
  `pkexec` run as the child, even a root agent that draws as a client
  of labwc.
  Criterion 2 can be met only if the password is typed while a VT the
  child does not own is the active one.
- **Anything drawn in the kiosk can be copied.**
  Steam, its games and the native apps can draw a full-screen copy of
  the grown-up screen and of any prompt.
  The copy can be a full-screen window, an overlay above it, or a
  session-lock surface, which also takes every key and turns off Super+Q
  and Ctrl-Alt-Home.
  A labwc key binding is no better: a program can press it through the
  virtual-keyboard protocol.
  The one key path on this image that goes around the child's
  compositor is logind's Secure Attention Key, Ctrl+Alt+Shift+Esc.
  Nothing on the image listens for it: SDDM 0.21 has no handler.
  And while `ptrace_scope` is 0, a program of the child can copy
  labwc's keyboard and grab it, so that logind never sees the keys
  (inference; not tried).
- **The screen can be moved from the child's session.**
  logind's `org.freedesktop.login1.chvt` is yes, with no password, for
  active and inactive local sessions, and the levels rule did not list
  it.
  SDDM also makes the kiosk's VT the controlling terminal of labwc, the
  launcher and the native apps they start, and logind gives that VT to
  the child.
  A process may switch VTs through its controlling terminal with an
  ioctl that polkit never sees (from SDDM's source; not yet seen in the
  VM).
- **Showing a hidden window is not a boundary.**
  labwc honours foreign-toplevel requests from any client, so any
  program of the child can already show and focus Steam's hidden
  windows.
  A window's app id and title are whatever the program says.
  labwc shows a dialog together with its parent, so a Steam question
  could bring Steam's main window, the store, with it.
  And the child's X11 programs can read a Steam password typed into a
  shown Steam window, as ADR-0032 says of a visit.
- **Containment does not come back by itself.**
  labwc's window rules act only when a window first appears, and
  reloading labwc hides nothing again.
  Something must hide a shown window again, and in the kiosk that is
  the launcher, which the child owns.
- **Other doors.**
  `/dev/uinput` is open to the active user, so a program of the child
  can make a keyboard of its own and type into a prompt on another VT,
  though not read it.
  Bazzite gives the active user the raw HID nodes (hidraw) of every
  Logitech device and of many keyboards (the Solaar, OpenRGB, Wooting
  and VIA rules), and a program that opened one keeps it.
  Three wrong passwords in 15 minutes keep every Guardian out for 10
  minutes (ADR-0020), so a child who can reach a prompt can lock the
  Guardians out.
  The image has no fingerprint in its PAM stack (no `with-fingerprint`,
  no `pam_fprintd`), although ADR-0032 says it has.

Three designs were drawn up from these findings.
Each was reviewed three ways: for safety against a determined child,
for what the child and the parent see, and for what Cairn would own.

- polkit's own text agent, run as root on a spare VT, after the child's
  processes are frozen and the screen is switched away from the kiosk.
- Cairn's own root password prompt on a spare VT, with the same freeze
  and switch, reached by the button or by Ctrl+Alt+Shift+Esc.
- The child logs out, and the grown-up fixes Steam from their own
  login, in a visit.

The safety review and the ownership review chose the third.
The review for the child and the parent chose Cairn's own prompt, and
found no blocking flaw in the third.
Both in-session designs rest on a round trip, with the child's
processes frozen, that nobody has tried.
Both meet criterion 6 only after system-wide hardening ("Alternatives
considered").

### What Mason decided then

The research put one question to Mason.
For child test 2, is this the grown-up's way to step in: log the child
out, then open Steam for the child from your own login, with one new
line on the grown-up screen?
On 2026-11-02 he answered yes.
The in-session password button stays open in #88, for after the test.

The other answer would have built the in-session step-in now, starting
from Cairn's own prompt on a spare VT ("Alternatives considered").
That was about two to three weeks and a dependency ADR (libpam, or
polkit-qt6-1's agent classes).
Before any friend got the ISO it would also have needed `ptrace_scope`
1 and launched apps in a session of their own.

## Decision

**For child test 2, and until the in-session step-in in #88 has what it
needs, a grown-up steps in from their own login. When Steam's sign-in
was turned down, the grown-up screen adds one line: "Log out, then a
grown-up can fix it from their own login." The child logs out. The
grown-up logs in at the login screen, opens Steam for the child
(ADR-0032), signs it in, presses Done and logs out. Cairn does not ask
for a Guardian's password inside a child's session, and does not show a
hidden window there.**

### What the child sees

1. They tap a Steam game whose Steam sign-in was turned down.
   About 15 seconds after the tap, at the first grace (#182), the
   grown-up screen says "Something needs a grown-up." and "<game> did
   not start.".
   Under them is the new line, "Log out, then a grown-up can fix it
   from their own login.", with Back.
2. Back, then Log out above the tiles, which asks once (ADR-0021).
   The login screen comes.
   Nothing in progress is lost: the launcher runs one app at a time,
   and this one never started.
3. If they tap their own tile while the visit is open, the login screen
   says "A grown-up is setting something up for you. Try again soon."
   (ADR-0032).
4. Afterwards one tap logs them in (ADR-0020).
   They tap the game, and it starts.

No new button, nothing that looks like a Guardian's door, no Steam
window and no text console appear in their session.

### The line

- **A new value of `AppLauncher.GrownUpStep`, `LogOutForAGrownUp`,**
  drawn as "Log out, then a grown-up can fix it from their own login."
  It follows ADR-0045: one line, calm, sentence case, at the sentence's
  size (`Tokens.headingSize`, 28 px), in `qsTr()`, and read with the
  screen (`Accessible.description`).
  It is never a reason, a code or a program's own words.
- **It shows only in Failed, for a Steam game, when Steam's log says its
  sign-in was turned down** (`SteamSignIn` reads `Refused`), whatever
  else the log says.
  It wins over ADR-0045's line: nothing else helps until Steam is
  signed in again.
  It never shows while Steam cannot reach its servers, for a window
  that opened on its own, for a coming-soon tile, or for a tile that is
  not a Steam game.
- **Steam's questions do not get it yet.**
  For a question Steam keeps, such as a cloud conflict, the grown-up
  would press Play inside the visit so that Steam asks again, answer,
  and quit the game.
  That runs the game in cage without the GPU, which nobody has tried.
  ADR-0045 adds a line only for a remedy seen to mend the case.
  So questions get this line only after check 7, by an amendment here.
  Until then they keep the journal's line, and ADR-0045's "Log out and
  back in" for the cloud question once Steam has signed in again.
- **C++ decides, QML draws.**
  The choice is a small class of its own, `GrownUpLine`, not part of
  `AppLauncher.cpp`, which is already 485 lines against CLAUDE.md's
  200.
  It reads what `SteamGameWait` read in Steam's logs at the launch's
  last look.
  `GrownUpScreen.qml` draws one more string.
- **The journal says the same, with the reason first:** "Steam could not
  sign in, so <game> did not start. Log the child out, then open Steam
  for them from your own login and sign in again."
  A refused sign-in comes first in the journal whatever else Steam's
  logs say.
  That includes a later tap on a game Steam holds behind a question,
  where ADR-0045 named the question alone.

### What the grown-up does

Every step is built already:

1. At the login screen, tap their own tile and type their password.
   That is SDDM's greeter, run as the system account `sddm`, with PAM's
   `password-auth` and faillock (ADR-0016, ADR-0020).
2. In Plasma, open "Steam for Ada" from the menu, or the Family app's
   Games page and "Open Steam for Ada".
   The window closes the grown-up's own Steam first, and says so.
   Plasma's own polkit box asks for the password again: "A grown-up's
   password is needed to open a child's Steam." (`auth_admin`, never
   kept).
3. Ada's own Steam opens in a window beside it, in the run-only cage.
   Sign in with Ada's Steam account and "Remember me", and Steam Guard's
   code if Steam asks.
4. Press Done, then log out of Plasma rather than Switch User, so that
   no grown-up session stays behind the child's (#213).
   P1-17 asks for no path from a child's session to Plasma.

### What stays the same

The kiosk does not change: `rc.xml`, its keys, the window rules and the
launcher's window decisions stay as they are.
Nothing new runs as root.
No PAM service, polkit action, package or library is added.

### The levels rule

`session/polkit/rules.d/10-cairn-levels.rules` adds two actions to the
kiosk's list (`notInTheKiosk`).
L1 and L2 get no for them before any prompt.
Administrators, L3 and L4 fall through as before (ADR-0023).

- **`org.freedesktop.login1.chvt`,** so that a program of the child
  cannot move the screen to another session through logind, such as a
  grown-up's lock screen after Switch User.
  It is a second layer only: a process whose controlling terminal is
  the kiosk's VT can still switch VTs (above).
  "Before any friend gets the ISO" narrows that.
- **`org.freedesktop.login1.inhibit-handle-power-key`,** so that a
  program of the child cannot take the lock that stops the power
  button's five-second hold (ADR-0018; inference, check 3).
  The parent docs give that hold as the way out of a screen that looks
  wrong (below).
  If check 3 shows that the lock does not stop the hold, this entry is
  dropped.

labwc's seat library, libseat 0.9.3, never calls `Session.Activate`.
It calls `Seat.SwitchTo` only for a VT key, which the kiosk removes.
SDDM switches the screen as root.
So labwc's start, Log out, and the login screen's return to a running
kiosk should need neither action (inference; check 3).

### What the parent docs say

`docs/TRY.md`, under "If something goes wrong", gets the steps above,
keyed to the line.
They end with Done and a logout rather than Switch User.
It also gets two habits:

- Cairn asks for your computer password only at the login screen and in
  your own login, never on a child's screen.
  If anything on a child's screen asks for it, do not type it.
  Hold the power button until the computer turns off: five seconds turns
  it off in order, and if that does not, keep holding.
  Then start it again, and log in at the login screen.
- Steam's window wants the child's Steam account, never your computer
  password.
  A password typed into a child's Steam is only as private as the
  child's account (ADR-0032).

It also says plainly what this costs: two passwords, about five
minutes, and the grown-up's own Steam closed for the visit.
"A way to answer it without logging the child out is planned" becomes
"Cairn has no way yet to do this without logging the child out (#88)."

### How the six criteria are met

| Criterion | Met | How |
|---|---|---|
| 1. A trusted agent | Yes | Only two upstream prompts take the Guardian's password: SDDM's greeter, and Plasma's polkit agent in the Guardian's own session, through polkitd, the setuid `polkit-agent-helper-1` and PAM's `system-auth` with faillock. Cairn writes none of it. |
| 2. Never readable by the child's processes | Partly | The password is typed after Log out has ended the child's session, and in the Guardian's own session. A visit opens only when no process of the child is left (ADR-0032). What remains is not new. After Log out, a program of the child that ignores the request to end lives up to 15 s, or 60 s if it stalls the child's user manager. Meanwhile it could type into the login screen through `/dev/uinput`, and on a listed keyboard it might read keys through hidraw. "Before any friend gets the ISO" closes most of that. |
| 3. Only allowlisted windows or actions | Yes | Nothing is shown in the child's session. The one recovery action is the visit, on the Guardian's own desktop. |
| 4. The child kept apart | Yes | The child has no session while the Guardian acts, and PAM refuses them at the login screen and the consoles during a visit. The visit's processes run in a session of their own, with no seat, on cage's own display. |
| 5. Containment back by itself | Yes | The kiosk's containment never changes, so there is nothing to restore. Every end of a visit runs one close (ADR-0032), and the child's next login is a fresh kiosk. |
| 6. Audit tests | Partly | Failed and abandoned prompts: yes (checks 5 and 6). Imitation: once the child has logged out, no program of theirs runs outside the run-only cage, so none can copy the Guardian's prompts. Before that, a program in a child's kiosk can still draw a copy of the login screen, as ADR-0020 already records. The defence is the habit above, and the audit row says so. It becomes a real defence when SDDM acts on Ctrl+Alt+Shift+Esc and `ptrace_scope` is 1 ("What stays open in #88"). |

### What stays open in #88

The in-session step-in stays open in #88, with its safety criteria.
It is not ruled out.
Whoever takes it up starts from what this research found it needs:

- The password typed only on a VT that root owns, with every process of
  the child frozen and the kiosk's input taken away by logind.
  Nobody has tried that round trip; check 9 tries it with no product
  code.
- A key the child's programs cannot fake.
  On this image that is logind's Secure Attention Key, which can be
  trusted only with `ptrace_scope` 1 and launched apps in a session of
  their own.
- A prompt in Cairn's voice.
  polkit's own text agent prints red, English-only banners and the
  action id.
  With two Guardians it reads the choice of Guardian with the echo on,
  and prints back what was typed.
  So the prompt is Cairn's, through either its own PAM conversation (a
  libpam dependency ADR) or polkit-qt6-1's agent classes, which are in
  the image (a dependency ADR, as ADR-0032 foresaw).
  Which one is decided then.
- Plain words in its ADR and in #86.
  Showing a Steam window stops the child's hands, not a program of the
  child.
  labwc shows a dialog with its parent.
  A Steam password typed there is only as private as the child's
  account.

SDDM's own handler for the Secure Attention Key is merged upstream
(SDDM pull request #1922, 2024) but not yet in a release.
When Fedora ships it, the kiosk can bind Ctrl+Alt+Shift+Esc to an action
that does nothing, as it does Ctrl+F5, so that the launcher and games
never take it as Escape.
The parent docs then teach "press Ctrl+Alt+Shift+Esc before you type
your password".
With `ptrace_scope` 1, criterion 6 becomes a yes for this route too.

### Slices

1. **The line, the journal's sentence, the levels rule's two entries,
   the parent docs, and the edits below**, in one change.
   Checks 1 to 4 need none of it, and run first; checks 5, 6 and 8 run
   on it.
2. **A time limit on a visit,** 60 minutes, in the visit helper's own
   close (ADR-0032), in a change of its own.
   Plasma's idle lock is the only limit today, and a family can turn it
   off.

### Before any friend gets the ISO

The research found these on the way.
They come after child test 2 and before any friend gets the ISO, each
in a change of its own, as #85 and #86 work; none is part of slice 1 or
2.
Each narrows this route's residue, and an in-session step-in would need
all of them.
Whatever is not built before the first friend's ISO is stated in the
parent docs as a known gap, as Mason chose for #86 on 2026-11-02.

- **Launched apps in a session of their own** (`QProcess`'s
  new-session flag), so that none has the kiosk's VT as its controlling
  terminal: about 10 lines and a Qt Test.
  DESIGN §4.5 changes with it.
- **`kernel.yama.ptrace_scope = 1`,** after the family's games, Proton,
  an anti-cheat title and Steam's crash reporter have run with it on
  the kids' laptop.
  The image ships 0, and this needs an ADR line of its own.
- **Ctrl+Alt+Del does nothing on a text console:**
  `ctrl-alt-del.target` masked and `CtrlAltDelBurstAction=none`.
  Today it reboots, and a program of the child can reach a text console
  through its controlling terminal.
- **A shorter stop for a child's user manager:** a `user@.service`
  drop-in with a `TimeoutStopSec=` near the image's 15 s, so that
  nothing of a child lingers 60 s behind the login screen.
- **Keyboards' raw HID nodes kept from children:** `getfacl
  /dev/hidraw*` on the kids' laptop during a child's session, then a
  udev override that drops `uaccess` from the keyboard nodes the
  Solaar, OpenRGB, Wooting and VIA rules cover.
- **Two side findings.**
  `uupd.rules` lets any account, a child's included, start the system
  updater (`manage-units` on `uupd.service`, with no check of who asks).
  It goes to #85's audit.
  And the fingerprint is missing from the image, against ADR-0032 and
  the "or your fingerprint" in the Guardian tool's words.
  It needs an issue of its own; nothing in this ADR relies on a
  fingerprint.

### Where it applies

The image, and a computer set up by provisioning, such as the kids'
laptop today (`docs/TRY.md` step 7).
The visit, its menu entry or the Family app, and the login screen's
sentence are on both.

## Consequences

- Child test 2 can include Steam.
  A refused sign-in, which the kids' laptop met on 2026-10-27 (#182),
  now tells the grown-up on the screen what to do, without the journal.
- #88's narrow version is smaller than the one Mason first described:
  the child logs out instead of a grown-up stepping in beside them.
  The cost is the child's session, about five minutes, two passwords,
  and the grown-up's own Steam closed for the visit.
  The child loses no play in progress, because the game never started.
- A grown-up who reads only the screen still needs `docs/TRY.md` for the
  steps.
  Nothing in the grown-up's own login points to "Steam for Ada" yet.
  A later change could have the Family app say whose Steam needs
  signing in, from ADR-0048's watcher, which reads a child's Steam logs
  as the child.
- Steam's questions still mean logging out and reading the journal,
  until check 7.
- Nothing new sits at a trust boundary.
  The new code is a few dozen lines of C++ and QML and two entries in
  the levels rule, with their tests.
  - Tested in C++: the line comes after a refused sign-in, also through
    the `No Connection` tries after it, with a stall seen at the same
    look, after Steam was started again for a stall, with a question
    held, and in place of ADR-0045's line when both apply.
    It does not come while Steam cannot reach its servers, at a plain
    tap, at the tap that meets the cloud question or at a later tap on
    the game it holds.
    Nor for a window that opened on its own, for a coming-soon tile,
    while a launch is starting or open, for a tile that is not a Steam
    game, once Steam has signed in again, or before a new tap's first
    look.
    ADR-0045's line still comes in its own cases.
    An `AppLauncher` test runs a refused sign-in at login to the
    grown-up screen with the line, which Back takes away, and no
    connection to the grown-up screen with no line.
    The journal's new sentence is tested too, also for a question held.
  - In QML: the line is hidden without a step, and with this one it is
    shown, named for a screen reader, at the sentence's size and at
    least 18 px.
  - In Python: the levels rule refuses both new actions at L1 and L2,
    and leaves administrators, L3, L4 and an account Cairn did not make
    to polkit's defaults.
- ADR-0032's own remaining checks, above all a sign-in made in a visit
  reaching the kiosk, run here, so ADR-0032 can be accepted with this
  ADR.
- The grown-up screen now tells a child that grown-ups have logins of
  their own.
  The login screen already shows grown-ups' tiles, and DESIGN §3.2's
  "nothing to probe" is about controls, not words.
  Mason confirms the words with this change.
- With the two new entries, a child's program can no longer switch the
  screen through logind, or stop the power button's hold with a lock.
  Until `ptrace_scope` is 1, a program that takes over labwc can still
  switch VTs through the controlling terminal, and grab the power
  button's device.
  The computer's own firmware hold still turns it off.
- P1-7's PIN overlay is unaffected.
  DESIGN §3.2 already assumes the PIN is known, and on this image a PIN
  typed in the kiosk is readable by the child's programs too.
- If Valve changes how Steam writes a refused sign-in, the line stops
  coming: a lost feature, not a wrong one (ADR-0045).
- If a sign-in made in a visit does not reach the kiosk (check 1), this
  route fails for Steam, and P1-17's other branch applies: no Steam in
  that run.
- Once accepted, it amends:
  - **ADR-0045:** a second line, for a refused sign-in.
    It answers #182's question: the grown-up screen says what will help,
    never the reason.
    While the sign-in is turned down, the journal names it before a
    question Steam holds.
  - **ADR-0036:** the journal's sentence for a refused sign-in, and the
    question its section on #182 left open.
  - **ADR-0032:** its "What comes later" item for the grown-up screen's
    words is this ADR.
    "Until a Guardian can step in inside the child's session (#88),
    fixing what Steam is waiting for means logging the child out" stays
    true.

## Edits in the same change

- **DESIGN §8.3, the mechanism:** the bullet that ends with ADR-0045's
  lines gains: "When Steam's sign-in was turned down, the grown-up
  screen says 'Log out, then a grown-up can fix it from their own
  login.', and a Guardian signs the child's Steam in again from their
  own login (ADR-0032; ADR-0049, proposed)."
- **DESIGN §8.3, mitigation 3,** said the foreign-toplevel protocol tells
  the launcher a Steam window appeared "so it can show the grown-up
  screen instead".
  It becomes three statements.
  The launcher hears of Steam's windows that way, but never counts a
  hidden one (ADR-0026).
  A launch held behind one ends on the grown-up screen (mitigation 2,
  ADR-0030).
  No Steam window is shown in a child's session, even for a grown-up
  (ADR-0049, proposed).
- **DESIGN §3.2, "Two access paths":** "Full administration" gains a
  sentence: fixing what a child's Steam waits for is done there, once
  the child has logged out (ADR-0032, ADR-0049).
  It also says that Cairn does not ask for a Guardian's password inside
  a child's session, and that a way to step in there is not designed
  yet (#88).
  The Games row of "What Guardians own" gains "open a child's own Steam
  to sign it in or answer it (ADR-0032)".
  ADR-0032 planned both edits for its acceptance; they are marked
  proposed until then.
- **DESIGN §4.5 and §14:** no edit.
  No grown-up's button is added, so §4.5's "never appears to the child
  as a button to find" still holds.
  §4.5 changes later, with launched apps' own sessions.
- **ADR-0032, ADR-0036 and ADR-0045** get "Amended by" lines.
- **`docs/TRY.md`:** the steps, the habits and the sentence above.
- **ROADMAP:** P1-13 gains the line.
  P1-17's "a Guardian's way to step in (#88)" becomes "a Guardian's way
  to fix Steam from their own login (ADR-0049; the in-session step-in
  stays #88)".
  The row for the Guardian boundary audits, #85 and #86, gains the
  audit rows from checks 4 to 6, and the list under "Before any friend
  gets the ISO".

## Checks in the VM

In cairn-iso, from a snapshot, with the test child `ada` (L1) and her
Steam signed in, unless a check says otherwise.
Checks 1 to 4 need nothing of this change, and run first.
If check 1 or 2 fails, this ADR is amended before anything more is
built.

### Before the line is built

1. **A sign-in made in a visit reaches the kiosk** (ADR-0032's open
   check).
   In a visit, sign `ada`'s Steam out in Steam's own account menu, sign
   in again with "Remember me", and press Done.
   Log `ada` in.
   Passes when her Steam stays silent, its `console_log.txt` has
   `Waiting for compat in post-logon took:` after the login and no
   `LogonFailure`, and a tile starts a game.
   Also record what Steam writes at a kiosk login with no saved sign-in
   at all, for a later line, and try Exit in Steam's menu in a visit.
2. **A real refused sign-in, mended from the grown-up's login.**
   Make `ada`'s saved sign-in fail.
   First spoil it in a snapshot: with her Steam stopped, change the
   saved token (Steam keeps it under `ConnectCache` in
   `config/config.vdf`; inference).
   Only if that gives no `LogonFailure` other than `No Connection`,
   revoke the test account's saved sign-ins once, if Mason agrees.
   Tap a game: the grown-up screen comes within about 15 s, and the
   journal says "Steam could not sign in".
   Then Back, Log out, the Guardian's login, "Steam for ada", sign in,
   Done, Plasma's logout, `ada`'s tile, and the game.
   Passes when the game starts.
   Record how long each step took.
3. **The levels rule's two entries.**
   First without them: does a `systemd-inhibit --what=handle-power-key
   sleep 60` run by `ada`'s program stop the power button's hold?
   Then with them installed:
   - `pkcheck --action-id org.freedesktop.login1.chvt --process
     <launcher pid>` says no for `ada`, with her session active and
     inactive, and so does a `busctl` call of `Seat.SwitchTo` from her
     program.
   - The same `systemd-inhibit` is refused.
   - Passes when labwc starts, Steam starts and signs in, Log out reaches
     the login screen, and the login screen returns `ada` to her running
     kiosk after a grown-up's Switch User.
     The power button's hold still turns the computer off.
4. **The handover.**
   As `ada`, start a test program that ignores SIGTERM and holds
   `/dev/uinput` open, then press Log out.
   Record when her last process ends (15 s expected; 60 s if her user
   manager is stopped), and whether the program can type into the login
   screen meanwhile.
   Passes when it ends within 60 s; what it could do goes into #85's
   row.

### Once the line is built

5. **Failed passwords.**
   A wrong password at the login screen, once, then three times:
   faillock keeps the Guardian out of every path for 10 minutes, and
   the words shown are recorded.
   A wrong password, and Cancel, in the visit's polkit box: the window's
   sentence, nothing opens, and `ada` is not listed and can log in.
6. **Abandoned prompts and imitation.**
   - The visit's polkit box left open until Plasma's idle lock: the
     request closes, nothing opens later, and `ada` can log in.
   - The login screen left half-typed: nothing changes.
   - `pidfd_getfd` and `ptrace` on `cage-visit` from `ada`'s programs,
     inside and outside cage's tree: "Operation not permitted".
   - A test client as `ada` draws a full-screen copy of the login
     screen in her kiosk, then a session-lock copy.
     Record what a grown-up sees, and that holding the power button
     brings back the real login screen with nothing of `ada`'s left.
7. **A question, before the line covers questions.**
   Make a cloud conflict for a game with Steam Cloud saves, and an
   interstitial if a test game has one.
   In a visit, press Play.
   Does the question come in cage, does the game run without the GPU
   and quit cleanly, and is the game free at `ada`'s next login?
   Only if so does an amendment here extend the line to questions.
8. **The screen.**
   With `ada`'s sign-in refused, tap a game: the line shows under "…
   did not start.", at the sentence's size, and is read with the
   screen.
   Back and Log out work as before.
   With the network down and her sign-in fine, tap a game: no line.

### Research only, after child test 2

9. **The frozen round trip and the key,** for #88's in-session half,
   with no product code.
   As root, freeze `user-<ada uid>.slice` and switch to a spare VT
   showing a test prompt; then thaw and switch back.
   Record whether labwc, Xwayland, the launcher and Steam all resume,
   whether sound comes back, how long the screen is dark each way, and
   whether labwc's debug log shows any key typed on the other VT.
   Then press Ctrl+Alt+Shift+Esc in `ada`'s kiosk, from the VM's
   keyboard and later from the kids' laptop's.
   Does the journal say "Secure Attention Key sequence pressed on seat
   seat0"?

### On the kids' laptop

10. Once it runs the image: check 2's route, timed; and `getfacl
    /dev/hidraw*` during a child's session, to size the hidraw residue
    on that computer.

## Alternatives considered

- **polkit's own text agent, as root on a spare VT.**
  The button or Ctrl+Alt+Shift+Esc would freeze the child's processes,
  switch the screen, and run `pkcheck --enable-internal-agent` for the
  launcher's process.
  The agent and PAM would stay upstream.
  But a child reaches polkit's own text with one tap: red, English-only
  banners and the action id.
  They break non-negotiable 5 and cannot be translated.
  With two Guardians it reads the choice of Guardian with the echo on,
  and prints back an answer it does not take.
  So a password typed there, as the sentence before it asks, shows on
  the screen.
  It also needs four things:
  - a polkit rule that lets a child's process be the subject of a Cairn
    action, against ADR-0032's "no before any prompt";
  - a root broker of about 400 lines of bash, behind a socket any
    process of a child can reach;
  - `ptrace_scope` 1 and new-session apps before its key can be
    trusted;
  - the round trip nobody has tried.

  If systemd killed the broker's whole unit, the child could be left
  frozen on the spare VT until the power button's hold.
- **Cairn's own root password prompt on a spare VT.**
  The same freeze and switch, with Cairn's own words, a root D-Bus
  service that recovers after a crash, and Ctrl+Alt+Shift+Esc.
  It gives the best in-session experience, and is the shape to start
  from if #88's in-session half is built.
  But Cairn would own a root password prompt and its PAM conversation:
  a libpam dependency ADR, about 1,100 lines of root C++ and 500 in the
  launcher.
  That is the most sensitive boundary in the system, and the layer
  non-negotiable 2 says to inherit.
  It shares the untried round trip.
  It shares the imitation gap too, until `ptrace_scope` is 1 and apps
  start in their own session.
  Child test 2 would wait for all four of its slices: about 6 to 9
  agent-days, and several evenings in the VM and on the laptop.
- **A polkit agent in the launcher, or `pkttyagent` or `pkexec` run as
  the child:** the password would go into a process the child owns,
  readable at `ptrace_scope` 0 (criterion 2).
- **Showing Steam's window with no password, or behind the PIN:** #88
  asks for the Guardian's password, the PIN is assumed known (DESIGN
  §3.2), and a child's hands would reach Steam's store.
- **Switch User, keeping the child's session:** the visit needs the
  child logged out, since Steam runs once per account.
  And while the Guardian types, a live child session can move the
  screen, and may read some keyboards through hidraw.
- **SDDM as a password check:** SDDM has no call that only checks a
  password; a Guardian's login always starts a session or returns to
  one.
- **A fingerprint,** the one factor a copied prompt cannot capture: it
  is not in the image's PAM stack today.
