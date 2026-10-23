# guardian

The Guardian's side of Cairn (ROADMAP P1-5). So far it has three pieces:

- **The Guardian tool**, "Family" in a Guardian's application menu
  (ADR-0040, proposed). Its one page so far, Children, lists the family's
  children with their levels, adds a child and moves a child to another
  level.
- **`cairn-family`**, the root command under it that makes a child's
  account and sets a child's level (ADR-0039, proposed). The family's
  setup on the image runs it too.
- **Steam for a child**, a small window that opens a child's own Steam
  from the Guardian's own Plasma login, to sign the child in, switch the
  account or change Steam's settings (ADR-0032, proposed).

## Family

`/usr/bin/cairn-guardian`, from the "Family" entry in the menu. Like the
Steam window, only the `cairn-guardian` group may run it, and the entry
names it as its `TryExec`, so no one else's menu shows it. It is on the
image only: hand provisioning installs neither it nor `cairn-family`.

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
```

It is installed as `/usr/libexec/cairn/cairn-family` and runs as root:
through `pkexec`, behind the polkit action `com.cairnlinux.family`, which
asks for an administrator's password for every change, or with `sudo` from
`../provision/cairn-family-setup.sh`. Started by `pkexec`, it checks
again that the account that started it is an administrator.

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
`taken`, `not-a-child` or `failed`. It writes one line to the journal:

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
| `qml/FamilyWindow.qml`, `qml/ChildrenView.qml` | The Guardian tool's window and its Children page, with the sentence for every answer. |
| `qml/ChildRow.qml`, `qml/AddChildForm.qml`, `qml/LabelledField.qml`, `qml/LevelChoice.qml` | A child on the list, the Add form, a text field with its label, and one level to pick. |
| `qml/TextButton.qml` | The button both windows use. |
| `src/FamilyCommand` | `cairn-family`: who may run it, which accounts it may change, and the steps for each subcommand. Everything it reads from or does to the machine is in its `Setup`, so a test can replace it. |
| `src/FamilyRequest` | The subcommand and its values, read from the arguments and checked before anything looks at an account. |
| `src/Refusal` | Why the command made no change, and the word for each. |
| `src/SystemTool` | Runs `useradd`, `passwd`, `gpasswd`, `usermod` or `restorecon` by absolute path, without a shell, with a small environment of its own. |
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

CTest runs twelve: `steamvisit`, `ownsteam`, `screenlockwatch`,
`steamforchild`, `steamwindow` (the real window in Qt Quick Test),
`familycommand`, `steamentry`, and for the Guardian tool `loginname`,
`childlist`, `familychange`, `childrenpage` and `familywindow` (the real
window in Qt Quick Test). They use stand-ins in `tests/fixtures/`
for `pkexec` with the helper behind it, which takes a scenario where the
helper's path goes, and for the Guardian's Steam client, over a stand-in
process list. `pkexec-family` stands in for `pkexec` as the Guardian
tool uses it: like pkexec, it refuses a command that is not there with
127, and otherwise runs it. Behind it, the stand-in `cairn-family`
answers as `CAIRN_TEST_SCENARIO` says: each of the command's refusal
words, pkexec's own 126 and 127, answers that cannot be read, or waiting
like an open password box, and writes down the arguments it was given.
The Guardian tool's tests give the page a table of accounts in place of
the system's, so they cover the order, who is left out, the login names,
every check before a change, one change at a time, trying an add again
after it failed, a missing command, and the sentence and the keyboard
focus after each answer, with no root and no real account.

`familycommand` runs `cairn-family`'s steps with `system-tool`, one
stand-in linked under each account tool's name that writes down what it
was asked, and a table of accounts in place of the system's, so no test
needs root or touches a real account. It covers every refusal, the
order of the group changes, a second run, the tools' environment and a
tool that fails. `steamentry` checks the entry's text against
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
- The Guardian tool cannot copy Steam games to a new child; the family
  set-up script still does (ADR-0033). Nor can it reset a child, set a
  PIN, or make an existing account a child (#113).
