# guardian

The Guardian's side of Cairn (ROADMAP P1-5). So far it has three pieces:

- **The Guardian tool**, "Family" in a Guardian's application menu
  (ADR-0040). Its Children page lists the family's children with their
  levels, adds a child and moves a child to another level. Its Games
  page chooses the games on each child's own list (ADR-0041).
- **`cairn-family`**, the root command under it that makes a child's
  account, sets a child's level (ADR-0039) and gives children their
  games (ADR-0041). The family's setup on the image runs it too, and a
  systemd unit runs it at a child's logout to copy games that waited.
- **Steam for a child**, a small window that opens a child's own Steam
  from the Guardian's own Plasma login, to sign the child in, switch the
  account or change Steam's settings (ADR-0032, proposed).

## Family

`/usr/bin/cairn-guardian`, from the "Family" entry in the menu. Like the
Steam window, only the `cairn-guardian` group may run it, and the entry
names it as its `TryExec`, so no one else's menu shows it. It is on the
image only: hand provisioning installs neither it nor `cairn-family`.

Two buttons above the page, **Children** and **Games**, move between its
pages. The window opens on Children.

The Children page:

1. Lists every child Cairn made, in the order their accounts were made,
   with their name and level. A child at level 3 or 4 shows that level.
2. **Move to level N** beside each child, for each level the page can
   give, 1 or 2. Up or down, the button and the sentence after it read
   the same (DESIGN §3). The new level starts the next time the child
   logs in, because PAM and the session read the groups at login.
3. **Add a child:** the name for the login tile, and level 1 or 2. The
   window makes the account's own short name from the name, such as `zoe`
   from "Zoë", and asks for one only when the name has no letters a to z.
   If an account or a group already has it, it uses `zoe2`, `zoe3` and so
   on, so it never names an existing child.
4. Each change runs `cairn-family` through `pkexec`, which asks for the
   Guardian's password every time. Every control waits until the answer
   is in, and one sentence says what happened, or why nothing changed and
   what to do next. A refusal leaves the form as it was.

The Games page (ADR-0041):

1. Pick a child. The first child is picked when the page opens.
2. A grid shows every game in the shared folder, from
   `/var/lib/cairn/games.json`, with its picture from
   `/var/lib/cairn/game-pictures` and its title as plain text. The games
   on that child's own list, `/var/lib/cairn/games/<login>.json`, are
   highlighted and marked "On the list". That file is what the child's
   launcher reads, so the highlighted games are the child's game tiles.
   A game on the child's list that the shared folder no longer has
   still shows, last, marked "On the list, no longer shared". A child
   with no file has no games, as a new child does.
3. A tap on a game, or Space or Enter, puts it on the list or takes it
   off. Nothing is saved yet: the card says "Added, not saved yet" or
   "Taken off, not saved yet", the child's name above says "not saved",
   and a sentence names every child with changes. The parent can change
   several children and save once. **Put back as saved** undoes the
   changes for the child picked.
4. **Save** runs `cairn-family save-games` through `pkexec` once, with
   each changed child's whole new list, so one password covers the whole
   Save. It is ready only when there is something to save, and **Put
   back as saved** only when the child picked has a change. While it runs,
   every control on the page waits, and the page says what it is doing,
   such as "Copying games for Ada.", read from the command's lines as
   they come. Then one sentence says it was saved, or why nothing was,
   and one for each child says what their list holds now and what held
   their new games back, such as the child being logged in, or which game
   was not given and why. A child's games that could not be given stay
   chosen, so pressing Save again tries them again. A closed password box
   keeps every change. The page reads everything again after each Save,
   and what the Save did stops showing once the parent turns a game,
   puts a list back or picks another child.
5. **Look for new games** runs `cairn-family save-games` with no child
   named, which changes no list but makes the full games list and the
   pictures again from the Guardian's own Steam, so a game just
   installed shows. Save does that too.
6. **Open Steam for <child>** opens the same window as the "Steam for
   <child>" menu entry, as a program of its own. If it cannot be
   started, the reason is under `-t cairn-guardian`; if it started and
   then stopped, under `-t cairn-steam-for`.
7. With no children, or no games in the shared folder yet, it says what
   to do instead. It reads everything again each time it is shown.

A child who is logged in gets the games taken off at once. Their new
games are written down at the Save, and copied by themselves about ten
seconds after the child next logs out (ADR-0041, step 5a). The child's
sentence after the Save says so, and that there is nothing more to do.
When a child was held or a game was left out, the Save's first sentence
says not every new game could be given, never that each child sees their
games. Coming back to the page, a child whose games still wait starts
from the list as saved, waiting games and all: those are marked "Waits
for Ada to log out", a note says they are copied at the next logout, and
Save stays off until something changes. Once the copy at the logout has
run, a note says it worked, with its date, or what stopped it, until the
next Save for that child. An open page follows both as they happen.

Before the password box opens, the page asks systemd-logind, without
root, who is logged in (ADR-0041, step 5c). For each child with a new
game to copy who is logged in, one at a time, it offers **Log Ada out
now** or **Later**, and every other control waits. **Log Ada out now**
first warns that anything Ada has not saved in a game may be lost, with
**Log Ada out** and **Not now**, which goes back to the choice. Focus
goes to **Not now**, so a second Enter never logs a child out. Once
every child is answered, Save runs once, with one password, and names
the children to log out with `--log-out=LOGIN`. While it runs the page
says "Logging Ada out. Steam gets a moment to save.", then that it is
copying. If one of the child's programs will not close, the page says
to restart the computer, and the games are copied at the child's next
logout. A child whose change only takes games off is never asked about,
and when logind cannot be asked, nobody counts as logged in, so the
games wait as for **Later**. The question comes only before a Save: the
waiting and finished notes have no **Log Ada out now** button. A child
whose games already wait gets them at their next logout, or after the
parent changes their list again and answers **Log Ada out now**. A child
is only still logged in if they walked away and the computer slept, so
this is rare; a button can come later if families ask for it.

The window's notes and the error output of `cairn-family`'s tools go to
the journal:

```sh
journalctl -t cairn-guardian -t cairn-family --since today
```

## Steam for a child

Provisioning puts a "Steam for <child>" entry in the application menu for
each child. Only a Guardian's menu shows them: the entry names
`/usr/local/bin/cairn-steam-for` (`/usr/bin/cairn-steam-for` in the
image, ADR-0038) as its `TryExec`, which only the
`cairn-guardian` group may run, and Plasma's menu leaves out an entry whose
program the account cannot run. The window:

1. Says what opening the child's Steam means, that the child cannot log in
   until Done, and, when the Guardian's own Steam is running, that it
   closes first, with any game or download in it.
2. On **Open**, closes the Guardian's own Steam (`steam -shutdown`), then
   starts the root helper through `pkexec`, and Plasma asks for the
   Guardian's password.
3. While the child's Steam is open in its own window, says to press
   **Done** when finished, rather than the close button on Steam's window,
   and before logging out, restarting or shutting down.
4. On Done, on the screen lock, or when its own close button is pressed,
   closes the child's Steam and waits until it has. Then it starts the
   Guardian's own Steam again (`bazzite-steam -silent`, as Bazzite's
   autostart does) and says so.

If the visit does not open, the window says why in one sentence and offers
**Try again**. If a program of the child's would not end, it says to
restart the computer before the child logs in again, and stays open until
the Guardian has read it.

## cairn-family

```sh
cairn-family create-child LOGIN LEVEL [--full-name NAME]
cairn-family set-level LOGIN LEVEL
cairn-family save-games [--log-out=LOGIN]... LOGIN=APPID,APPID... [LOGIN=...]
cairn-family finish-games UID
```

It is installed as `/usr/libexec/cairn/cairn-family` and runs as root:
through `pkexec`, behind a polkit action for each subcommand
(`com.cairnlinux.family.create-child`, `.set-level`, `.save-games`),
each of which asks for an administrator's password for every change, or
with `sudo` from `../provision/cairn-family-setup.sh`. Started by
`pkexec`, it checks again that the account that started it is an
administrator.

- LEVEL is 1 or 2. L3 and L4 have no child session yet.
- `create-child` makes a new account with its level group and a locked
  password, with NAME as its full name. Run again for a child Cairn made,
  it locks the password if needed, sets the full name and the level, and
  otherwise changes nothing.
- `set-level` changes an existing child's level: the new group first,
  then the old one goes (#100).
  It also locks the child's password if it is not locked.
- An account Cairn did not make is refused, never made a child: an
  administrator, a Guardian, a system account, or one in no level group.
- Both write the child's "Steam for <child>" entry in
  `/usr/local/share/applications`, as provisioning does, and relabel it.

It answers in one line: `done`, or `refused` and one of `not-root`,
`not-a-grown-up`, `usage`, `bad-name`, `bad-full-name`, `bad-level`,
`taken`, `not-a-child` or `failed`.

`save-games` gives children their games (ADR-0041); the Games page's
Save runs it. Each argument is one
child's whole new list, so `ben=` means no games, and a child not named
is left alone. With no child named it only makes the pictures and the
full games list again: the page's Look for new games. `--log-out=ada`
says the parent chose to log Ada out for her copy; every `--log-out`
comes before the lists, once, for a child whose list the Save gives.

- It checks the caller, every value and that every name is a child
  Cairn made before anything changes, and holds a lock for the whole
  Save, so a second Save at once is refused with `busy`.
- It remakes the pictures from the Guardian's own Steam, reading the home
  of the account pkexec names from the account database. Under `sudo`
  there is no Guardian, and the pictures stay.
- It remakes `/var/lib/cairn/games.json` with kidscan, as provisioning
  does, and keeps the old one when kidscan fails.
- Before any copy starts, it writes every child's
  `/var/lib/cairn/games/<login>.json` with the games taken off gone, so
  a Save cut off mid-copy leaves no child a game the parent took off.
- For each child it copies the games newly ticked with
  `cairn-steam-visit --copy-games CHILD APPID...`, run as a system job
  with `systemd-run`, outside the Guardian's session, and waited for with
  no time limit. A child named with `--log-out` gets
  `--copy-games --log-out CHILD APPID...`, which logs them out first
  when they are logged in; any other logged-in child is held. A Guardian who logs out mid-copy ends this command and
  not the copy.
- It then writes that child's file again, the launcher's games file,
  with the games still ticked that the child had and the ones the copy
  gave them. A game taken off goes even when the copy is held, for
  instance because the child is logged in.
- Before anything changes for a child with games to copy, it writes
  their whole new list, such as `ada=570,620`, to
  `/var/lib/cairn/games-waiting/<login>`, and removes
  `/var/lib/cairn/games-finished/<login>`. When the copy is held as
  `logged-in`, `still-running`, `stopped`, `not-started` or `stuck`,
  which a logout or a restart settles, the file stays and it says `later LOGIN`; any other
  ending removes it.

It answers line by line as it goes, with fixed words, checked logins and
appids: `looking` (the password was accepted and the work starts),
`pictures made|kept`, `catalogue made|kept`, `copying LOGIN`,
`waiting LOGIN`, `logging-out LOGIN`, `logged-out LOGIN`,
`copied|have LOGIN APPID`, `skipped LOGIN APPID REASON`,
then `child LOGIN added N removed N`, with `held WORD` when the child's
new games could not all be given, and `later LOGIN` when they wait for
the child's next logout. It ends with `done`, or `refused` and a
word, which means nothing changed: the words above, `bad-appid` or `busy`.

`finish-games` copies the games that waited for a child, once they have
logged out. Only `cairn-finish-games@UID.service` starts it, from a
drop-in on systemd's `user-runtime-dir@.service`
(`../session/systemd/`); it has no polkit action and refuses
`not-for-pkexec` when `PKEXEC_UID` is set. For an account that is not a
child with games waiting it does nothing and says nothing. Otherwise it
waits for a Save that is running, reads the waiting list again with
every check, and runs a Save's copy for that one child from the games
file as it is, with no pictures made. A copy held for a reason a logout
settles waits for the next one; any other ending writes the child's
lines to `/var/lib/cairn/games-finished/<login>` for the Games page and
ends the wait. `create-child` removes a new child's old games files
first.
ADR-0041 lists every word but the copy step's skip reasons, which
`provision/cairn-copy-games.py` lists and `cairn-family` passes on.

It writes to the journal:

```sh
journalctl -t cairn-family --since today
```

## How it is built

QML draws, C++ decides (ADR-0002):

| File | What |
|---|---|
| `src/SteamVisit` | Runs `pkexec` with the root helper, `session/bin/cairn-steam-visit`, as a direct child, so polkit can find the Guardian's session through it. Reads the helper's lines (`waiting`, `open`, `closing`, `closed`, `stuck`, `refused REASON`) and writes `done`. |
| `src/OwnSteam` | The Guardian's own Steam: whether it runs (from `/proc`), closing it before a visit, starting it after. |
| `src/ScreenLockWatch` | KWin's `org.freedesktop.ScreenSaver` `ActiveChanged` signal on the session bus, through Qt D-Bus. |
| `src/SteamForChild` | The steps the window shows, and what Done, the screen lock and the close button do at each one. |
| `qml/Main.qml` | The Steam window. |
| `src/LoginName` | The account's login name, made from the name a parent types, and the first free one when it is taken. |
| `src/ChildList` | The family's children among the accounts, in the order they were made, each with their name and level. |
| `src/FamilyChange` | Runs `pkexec` with `cairn-family`, one change at a time, and reads its one-line answer, or pkexec's own exit code. |
| `src/ChildrenPage` | What the Children page shows and what its buttons do: the list, the checks before a change, and why a change was refused. Its account database is in its `Setup`, so a test can replace it. |
| `src/GamesPage` | What the Games page shows and does: the children, the one picked, the games with each marked when it is on that child's list as saved and as wanted, turning a game, Save, and opening a child's Steam window. Its files, accounts, commands and program start are in its `Setup`, so a test can replace them. |
| `src/LoginSessions` | Who is logged in, from systemd-logind's `ListSessions` on the system bus through Qt D-Bus, without root, counted by the visit helper's own rule. |
| `src/LogOutQuestion` | The question before a Save: which children to ask about, and the steps of the choice and the warning for each, then whom the Save logs out. |
| `src/WaitingGames` | Each child's list as saved, their waiting list, which is the page's starting point for them, and the copy at their last logout, read without root and watched for changes. |
| `src/SteamWindowStart` | Starts a child's Steam window as a program of its own, and notes in the journal when it cannot. |
| `src/SaveResults` | One child's result, from a Save or from the copy at their logout, as the map the page's QML reads, and whether every child got every new game. |
| `src/GameShelf` | Every shared game from the full games list, each marked when it is on one child's own list, and whether that list could be read. |
| `src/GameChoices` | The changes to each child's list not saved yet, and `save-games`'s arguments for them. |
| `src/GamesSaving` | One Save as the page shows it: the password box, how far it has got, each child's result, or why nothing was saved, each as a value the page's QML puts into a sentence. |
| `src/SaveRun` | Runs `pkexec` with `cairn-family save-games`, and passes on each line as it arrives, then the answer, pkexec's own exit code, or `not-started` when nothing could be started. |
| `src/SaveReport` | Reads `save-games`'s lines into how far it has got and each child's counts, held word and games left out, taking only its fixed words and the children the Save named. |
| `src/PkexecAnswer` | pkexec's own two exit codes, for both runners of `cairn-family`. |
| `src/GamePicture` | A game's picture from its games file entry, with the launcher's rule: only the full path of a file here. |
| `qml/FamilyWindow.qml`, `qml/ChildrenView.qml`, `qml/GamesView.qml` | The Guardian tool's window with its switch between pages, its Children page with the sentence for every answer, and its Games page. |
| `qml/ChildChoice.qml`, `qml/GameCard.qml`, `qml/SaveOutcome.qml` | A child to pick on the Games page, one game in its grid, and the sentences for a Save. |
| `qml/LaterNote.qml`, `qml/WaitingNote.qml` | After a Save, why a child's games wait and that they come at their next logout; on the page, that they still wait, or how the copy at the logout went. |
| `qml/LogOutChoice.qml` | The question before a Save about a child who is logged in: **Log Ada out now** or **Later**, then the warning. |
| `qml/ChildRow.qml`, `qml/AddChildForm.qml`, `qml/LabelledField.qml`, `qml/LevelChoice.qml` | A child on the list, the Add form, a text field with its label, and one level to pick. |
| `qml/TextButton.qml` | The button both windows use. |
| `src/FamilyCommand` | `cairn-family create-child` and `set-level`: which accounts it may change, and the steps for each. Everything it reads from or does to the machine is in its `Setup`, so a test can replace it. |
| `src/Caller` | Who started `cairn-family`: root, and through pkexec an administrator, with their home from the account database. |
| `src/GamesSave` | `cairn-family save-games`: the checks, the lock, the pictures and full list, then each child in turn. Its `Setup` holds everything it reads from or does to the machine. |
| `src/GamesRequest` | Each child's new list, and the children to log out, read from the arguments and checked before anything looks at an account. |
| `src/CatalogueRefresh` | Runs the picture step and kidscan, and writes kidscan's list into `/var/lib/cairn/games.json`. |
| `src/GameCatalogue` | A games file: reading it, each game by its appid with the launcher's rule, and writing one. `cairn-family` and the Games page both use it. |
| `src/ChildGameList` | The rule for a child's new list, with nothing read or written. |
| `src/ChildGamesFile` | A child's own games file: reading it, and writing it into place in a folder only root may change. |
| `src/ReadableFile` | Writes a file every account may read, renamed into place. |
| `src/CopyJob` | Runs a child's copy as a system job with `systemd-run`, or, for `finish-games`, which is a system unit already, in finish-games' own unit, with `--log-out` for a child the parent chose to log out, waits for it, and passes on only the helper's own lines. |
| `src/ChildSave` | One child's part of a Save: their file without the games taken off, then the copy, their file again and their line. |
| `src/WaitingSave` | A child's waiting list: writing it, reading it back with every check, removing it, and which held words keep it. |
| `src/FinishedSave` | How the copy at a child's logout ended: the lines, dated by the file. |
| `src/FinishGames` | `cairn-family finish-games`: the guards, the lock, the waiting list read again, and the copy for one child. |
| `src/RootFolder` | A folder only root may change and every account may read, and its small files, checked without following a link. |
| `src/FamilyRequest` | The subcommand and its values, read from the arguments and checked before anything looks at an account. |
| `src/Refusal` | Why the command made no change, and the word for each. |
| `src/SystemTool` | Runs `useradd`, `passwd`, `gpasswd`, `usermod`, `restorecon`, the picture step or kidscan by absolute path, without a shell, with a small environment of its own and a two-minute limit. |
| `src/PasswordLock` | Whether a child's password is locked, from the shadow database. |
| `src/SteamEntry` | A child's "Steam for <child>" menu entry: its text, the same as provisioning's, and writing it into place. |

The child's name the window shows, and the rule for whose account is a
child's, come from `../accounts/`.

The window's notes and the helper's trace go to the journal as
`cairn-steam-for`, and the helper writes one line when a visit opens and
one when it closes as `cairn-steam-visit`:

```sh
journalctl -t cairn-steam-for -t cairn-steam-visit --since today
```

## Tests

CTest runs twenty-seven, among them `steamvisit`, `ownsteam`, `screenlockwatch`,
`steamforchild`, `steamwindow` (the real window in Qt Quick Test),
`familycommand`, `steamentry`, `waitingsave` and `finishgames`, and for
the Guardian tool `loginname`,
`childlist`, `familychange`, `saverun`, `savereport`, `gamechoices`,
`childrenpage`, `gamepicture`, `gameshelf`, `gamespage` and
`familywindow` (the real window in Qt Quick Test, both pages and the
switch). They use stand-ins in `tests/fixtures/`
for `pkexec` with the helper behind it, which takes a scenario where the
helper's path goes, and for the Guardian's Steam client, over a stand-in
process list. `pkexec-family` stands in for `pkexec` as the Guardian
tool uses it: like pkexec, it refuses a command that is not there with
127, and otherwise runs it. Behind it, the stand-in `cairn-family`
answers as `CAIRN_TEST_SCENARIO` says: each of the command's refusal
words, pkexec's own 126 and 127, answers that cannot be read, or waiting
like an open password box, and writes down the arguments it was given.
Its `save-*` scenarios answer `save-games` in many lines, as the real
command does: every game given, a child held by a word, games left out,
a copy still running, or a run cut off before `done`; `save-done` also
writes each child's list into the folder `CAIRN_TEST_GAMES` names, and
`save-held-later` holds each child as logged in, says `later` and writes
their waiting list into the folder `CAIRN_TEST_WAITING` names. A child
named with `--log-out` is logged out first in `save-done`, held as
`stuck` and left waiting in `save-stuck`, and `save-logging-out` stays
logging the first child out.
The Guardian tool's tests give the page a table of accounts in place of
the system's, so they cover the order, who is left out, the login names,
every check before a change, one change at a time, trying an add again
after it failed, a missing command, and the sentence and the keyboard
focus after each answer, with no root and no real account.
The Games page's tests write their own full games list, children's
lists and stand-in pictures into a temporary folder, and replace the
start of the Steam window with one that writes down what it was asked.
They cover turning games for two children and saving once, the exact
arguments pkexec is given, a child held or a game left out, every
refusal keeping the changes, every control waiting while a Save runs, a
waiting list as the child's starting point, an open page following the
files root writes, the dated note after a copy at a logout, and the
question about a logged-in child: who is asked about, the choice, the
warning and its focus, and exactly one Save naming the children to log
out. `loginsessions` checks the rule for a login, and asks the test
machine's own logind when it has one; `logoutquestion` covers whom the
page asks about and every step of the question.
Setting `CAIRN_SCREENSHOT=/path/family.png` when running
`tst_familywindow` saves pictures of both pages beside that path.

`familycommand` runs `cairn-family`'s steps with `system-tool`, one
stand-in linked under each account tool's name that writes down what it
was asked, and a table of accounts in place of the system's, so no test
needs root or touches a real account. It covers every refusal, the
order of the group changes, a second run, the tools' environment, a
tool that fails, and a new child's old games files removed first.
`gamessave` and `finishgames` run `save-games` and `finish-games` with
stand-ins for `systemd-run` and the visit helper's copy, which copies
the waiting folder as it finds it, so they see the waiting list written
before anything changes, and which endings keep it. `finishgames` holds
the Save lock from a second thread, to show it waits and reads the list
again. `waitingsave` covers every check on a waiting list. `steamentry` checks the entry's text against
provisioning's own `steam_entry`. The screen-lock tests and the Steam
window's tests run on a session bus of their own under
`dbus-run-session`, which the build needs; the Guardian tool's tests
need no bus.

What the stand-ins cannot show was run in the VM (`docs/research/steam-visit.md`).

## Not yet

- Logging out, restarting or shutting down during a visit still stops the
  child's Steam without its shutdown: Plasma asks every window to close
  at once, and cage closes with the rest. The window holds them until the
  visit has closed, but Steam has lost its window by then, so the window
  says to press Done first. The close button on Steam's own window ends
  cage at once in the same way.
- The Guardian's own Steam, started again by the window, runs in the
  window's own systemd unit, which then stays until Steam ends.
- Neither `cairn-family` nor the Guardian tool has run in the VM yet.
- Logging a child out from the Games page has not run in the VM yet
  (ADR-0041, step 6).
- The copy at a child's logout has not run in the VM yet (ADR-0041,
  step 6).
- The Guardian tool cannot reset a child, set a PIN, or make an existing
  account a child (#113).
