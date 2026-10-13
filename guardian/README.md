# guardian

The Guardian's side of Cairn (ROADMAP P1-5). So far it is one small window:
**Steam for a child**, which opens a child's own Steam from the Guardian's
own Plasma login, to sign the child in, switch the account or change
Steam's settings (ADR-0032, proposed). It is the first piece of the
Guardian tool; the rest of that tool is still to come.

## What a Guardian sees

Provisioning puts a "Steam for <child>" entry in the application menu for
each child. Only a Guardian's menu shows them: the entry names
`/usr/local/bin/cairn-steam-for` as its `TryExec`, which only the
`cairn-guardian` group may run, and Plasma's menu leaves out an entry whose
program the account cannot run. The window:

1. Says what opening the child's Steam means, that the child cannot log in
   until Done, and, when the Guardian's own Steam is running, that it
   closes first, with any game or download in it.
2. On **Open**, closes the Guardian's own Steam (`steam -shutdown`), then
   starts the root helper through `pkexec`, and Plasma asks for the
   Guardian's password.
3. While the child's Steam is open in its own window, says to press
   **Done** when finished and before logging out.
4. On Done, on the screen lock, or when its own close button is pressed,
   closes the child's Steam and waits until it has. Then it starts the
   Guardian's own Steam again (`bazzite-steam -silent`, as Bazzite's
   autostart does) and says so.

If the visit does not open, the window says why in one sentence and offers
**Try again**. If a program of the child's would not end, it says to
restart the computer before the child logs in again, and stays open until
the Guardian has read it.

## How it is built

QML draws, C++ decides (ADR-0002):

| File | What |
|---|---|
| `src/SteamVisit` | Runs `pkexec` with the root helper, `session/bin/cairn-steam-visit`, as a direct child, so polkit can find the Guardian's session through it. Reads the helper's lines (`waiting`, `open`, `closing`, `closed`, `stuck`, `refused REASON`) and writes `done`. |
| `src/OwnSteam` | The Guardian's own Steam: whether it runs (from `/proc`), closing it before a visit, starting it after. |
| `src/ScreenLockWatch` | KWin's `org.freedesktop.ScreenSaver` `ActiveChanged` signal on the session bus, through Qt D-Bus. |
| `src/SteamForChild` | The steps the window shows, and what Done, the screen lock and the close button do at each one. |
| `src/AccountName` | The child's full name, or the login name when there is none, as on the login screen. |
| `qml/Main.qml` | The window. |

The window's notes and the helper's trace go to the journal as
`cairn-steam-for`, and the helper writes one line when a visit opens and
one when it closes as `cairn-steam-visit`:

```sh
journalctl -t cairn-steam-for -t cairn-steam-visit --since today
```

## Tests

CTest runs five: `steamvisit`, `ownsteam`, `screenlockwatch`,
`steamforchild` and `steamwindow`, the real window in Qt Quick Test. They
use stand-ins in `tests/fixtures/` for `pkexec` with the helper behind it,
which takes a scenario where the helper's path goes, and for the Guardian's
Steam client, over a stand-in process list. The screen-lock tests and the
ones that build the whole window run on a session bus of their own under
`dbus-run-session`, which the build needs.

What the stand-ins cannot show was run in the VM (`docs/research/steam-visit.md`).

## Not yet

- Logging out during a visit still stops the child's Steam without its
  shutdown: Plasma asks every window to close at once, and cage closes
  with the rest. The window holds the logout until the visit has closed,
  but Steam has lost its window by then, so the window says to press Done
  first.
- The Guardian's own Steam, started again by the window, runs in the
  window's own systemd unit, which then stays until Steam ends.
