# guardian

The Guardian's side of Cairn (ROADMAP P1-5). So far it has two pieces:

- **Steam for a child**, a small window that opens a child's own Steam
  from the Guardian's own Plasma login, to sign the child in, switch the
  account or change Steam's settings (ADR-0032, proposed).
- **`cairn-family`**, the root command that makes a child's account and
  sets a child's level (ADR-0039, proposed). The Guardian tool's window
  will run it; the family's setup on the image already does.

The rest of the Guardian tool is still to come.

## What a Guardian sees

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
| `qml/Main.qml` | The window. |
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

CTest runs seven: `steamvisit`, `ownsteam`, `screenlockwatch`,
`steamforchild`, `steamwindow` (the real window in Qt Quick Test),
`familycommand` and `steamentry`. They use stand-ins in `tests/fixtures/`
for `pkexec` with the helper behind it, which takes a scenario where the
helper's path goes, and for the Guardian's Steam client, over a stand-in
process list. `familycommand` runs `cairn-family`'s steps with
`system-tool`, one stand-in linked under each account tool's name that
writes down what it was asked, and a table of accounts in place of the
system's, so no test needs root or touches a real account. It covers
every refusal, the order of the group changes, a second run, the tools'
environment and a tool that fails. `steamentry` checks the entry's text
against provisioning's own `steam_entry`. The screen-lock tests and the
ones that build the whole window run on a session bus of their own under
`dbus-run-session`, which the build needs.

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
- No window runs `cairn-family` yet; that is the next slice of #167.
  Nor has the command run in the VM.
