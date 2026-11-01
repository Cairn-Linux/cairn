# Try the Cairn prototype

Cairn is a pre-alpha prototype. There is no installer or ISO yet: a script
turns a fresh Bazzite install into a Cairn machine instead. This page takes
an adult who is comfortable installing Linux through it, in about an hour.

> **Use a spare computer or a virtual machine.** The script replaces the
> login screen, adds packages to the system and creates accounts, and the
> only undo is reinstalling. Cairn is not ready to be a family's everyday
> computer.

## What you need

- A 64-bit Intel or AMD computer with UEFI firmware.
- 4 GB of memory (8 GB is better) and 64 GB of storage.
- Intel or AMD graphics. NVIDIA graphics are not supported yet.
- An internet connection while you set it up.

A virtual machine works too: 4 GB of memory, a 64 GB disk and UEFI
firmware.

## 1. Install and update Bazzite

1. Download Bazzite from <https://bazzite.gg>. Choose the desktop edition
   with KDE Plasma, and the image for AMD or Intel graphics, not NVIDIA.
2. Install it. The account you create during installation is the
   computer's administrator. It stays, and it shows on Cairn's login screen
   too.
3. Open Konsole, update, and reboot:

   ```sh
   ujust update
   ```

   Update before building: Cairn is built against the same Qt as the
   updated system.

## 2. Build Cairn

Bazzite's system files are read-only, so Cairn is built inside a toolbox: a
Fedora 44 container that shares your home folder. In Konsole:

```sh
toolbox create --distro fedora --release 44 cairn-build -y
toolbox run -c cairn-build sudo dnf install -y git cmake ninja-build \
    gcc-c++ pkgconf-pkg-config python3 qt6-qtbase-devel \
    qt6-qtdeclarative-devel qt6-qtsvg-devel qt6-qttools-devel \
    qt6-qtwayland-devel qt6-linguist wlr-protocols-devel
toolbox run -c cairn-build git clone --recurse-submodules \
    https://github.com/Cairn-Linux/cairn ~/cairn
toolbox run -c cairn-build bash -c \
    'cd ~/cairn && cmake --preset release && cmake --build --preset release'
```

The build takes a few minutes and ends with a line like
`[369/369] Linking ...`.

## 3. Set up the accounts

Back in an ordinary Konsole window, not in the toolbox:

```sh
cd ~/cairn
sudo ./provision/cairn-provision.sh --guardian parent --child sam --child alex:2
```

- `--guardian` names the grown-up's account. The script creates it and
  asks for its password twice, without showing it.
- Each `--child` names a child's account, at level 1. Add `:2` for level
  2. Children log in without a password.
- Names use lowercase letters, digits, `-` and `_`.

The first run takes a while: it adds packages to the system and downloads
Tux Paint and GCompris. Every step it takes prints a line starting
`changed:`. Then:

1. Reboot, and run the same command again. This switches the login screen
   to Cairn's.
2. Reboot again, and run it once more. It should end with
   `done: 0 change(s)`.

## 4. Try it

- The login screen asks "Who's playing today?" and shows a round tile for
  each child and each grown-up. A child's tile logs in with one press; a
  grown-up's asks for the password.
- A child sees full-screen tiles: Draw (Tux Paint), Practice (GCompris) and
  the Terminal. Music, Build and Story say they are coming soon.
- **Super+Q** leaves an app. **Log out** is at the top right.
- The grown-up's account logs in to an ordinary KDE Plasma desktop.
- To sign a child in to Steam, or to change their Steam's settings, open
  **Steam for** and the child's name from the grown-up's application
  menu. Only a grown-up's menu has it, and the child must be logged out.
  It asks for your password and closes your own Steam while it is open.
  The child's own Steam opens in a window of its own, and the child cannot
  log in until you press **Done**. If they try, the login screen tells
  them a grown-up is setting something up and to try again soon.
- Press Done before you log out, restart or shut down. Those close the
  child's Steam before it can save, which can lose a game's last cloud
  save, and so does the close button on the child's Steam window. Locking
  the screen closes it properly, but stops any download in it.
- If you switch a child's Steam to the child's own account, remove your
  own account from Steam's list of accounts in the same window. Cairn
  suggests a Steam account for each child, as a child member of your
  Steam Family.

## 5. Add games for the children

You install each game once, from your own Steam, into a folder only
grown-ups can change. The set-up command then gives each child their own
copy, in their own account, so each child's saves are their own. A copy
takes no extra disk space until it changes.

Log in as the grown-up's account you named with `--guardian`. The account
made while installing Bazzite cannot change the shared folder.

1. In your own Steam, open **Settings › Storage**, add a drive, and choose
   the folder `/var/lib/cairn/steam-library`. Do this before any game is in
   it. If Steam refuses the folder, write down its message, stop here, and
   report it as a GitHub issue.
2. To install a game for the children, choose that folder in Steam's
   install dialog.
3. To give them a game you have already installed, select it in
   **Settings › Storage** and use **Move**. A Windows game is the
   exception: Steam's Move can undo itself for one. Uninstall it, then
   install it again into the shared folder, which downloads it once more.
   Uninstalling can delete the game's saves, and Steam Cloud keeps a copy
   only for games that use it. Copy the game's folder somewhere safe
   first.
4. Only put games there that every child may play. Every child gets a copy
   of each one.
5. A child's Steam must have run once before it can hold copies. Open
   **Steam for** the child once and sign their Steam in.
6. With the children logged out, run the set-up command from section 3
   again. It prints a `changed:` line for each game it copies. A `note:`
   line says when a child was logged in, when their Steam has not run yet,
   or why a game was left out. Those children get their copies at a later
   run.

   The same run makes a tile for each game in the shared folder, with the
   game's picture on it. The picture is the one your own Steam keeps for
   the game on this computer; nothing is downloaded. A `note:` line names
   a game that has no picture yet. Open Steam in your own login, so its
   library shows the game, then run the set-up command again. Until then
   that game's tile shows its name.

   Every child sees the new tiles at their next login, even a child who
   did not get their copies in this run. Until a later run copies the
   game for that child, its tile ends on "Something needs a grown-up".
7. Open **Steam for** the child and start one game, so Steam fetches the
   tools it needs out of the child's sight. With a Linux game this may not
   be enough: the child's Steam can stop part-way here too (#206; not yet
   checked in a visit). If the game has not started after a few minutes,
   close **Steam for**. The launcher deals with it at the child's first
   tap (see "If something goes wrong").

A new game's tile reaches every child at their next login, and the game
itself the next time you run the set-up command with that child logged
out. Each child's Steam keeps its own copy up to date, so
an update downloads once for you and once for each child.

Try games in **Steam for** the child, not in your own Steam. A game played
from the shared folder keeps its saves there, and every copy made after
that starts with them. A Windows game played there also leaves its Wine
prefix there, with anything its launcher keeps in it, and every account on
this computer can read that. Your own Steam can also bring a game's saves
from Steam Cloud into the shared folder when it starts, even if you never
played the game here, and copies made after that start with them too.

Children who share one Steam account share saves. Before a game starts,
Steam Cloud brings any new or changed save into that child's copy, without
asking. A save one child deletes stays in the other children's copies.
Turning Steam Cloud off for those games, in each game's **Properties**, or
for the whole account, in Steam's **Settings › Cloud**, should stop that.
The saves are then kept only on this computer. The switch belongs to the
Steam account, so on your own account it also goes off on your other
computers, and some people say it turned itself back on. One Steam account
for each child avoids shared saves.

Steam lets one account at a time play a game your family owns once. While
you, or a child on another Steam account, play it, it will not start for
anyone else, and Steam says the shared library is locked. A game already
running is not stopped. A second copy of the game avoids that.

If a child's Steam uninstalls a game but leaves its folder behind, the
folder may hold their saves, so the set-up command leaves that game out for
that child and says so. To give them a fresh copy, move the folder aside,
with the child logged out, and run the set-up command again. For a child
`sam` and a game folder `Some Game`:

```sh
sudo -u sam mv -T -- "/home/sam/.local/share/Steam/steamapps/common/Some Game" \
    "/home/sam/Some Game (old saves)"
```

This runs as the child, so it can only move what the child could move
themselves.

## If something goes wrong

- **Ctrl-Alt-Home** ends a stuck app and brings the tiles back.
- Holding the **power button** for five seconds powers off in order. A tap
  does nothing.
- From the grown-up's account, this shows what went wrong:

  ```sh
  journalctl -t cairn-launcher -t cairn-kiosk -t cairn-steam-for -t cairn-steam-visit --since today
  ```

- If a Steam game says "did not start" and it is new, the child may not
  have their copy yet: run the set-up command again with that child logged
  out.
- If a Steam game says "did not start", Steam is often waiting for an
  answer in a window the child's session hides. Its own log says which:
  look for "waiting for user response" in
  `~/.local/share/Steam/logs/console_log.txt` in the child's home. Without
  internet that is usually Steam Cloud, which checks a game's saves before
  starting it; Steam's settings can turn Steam Cloud off, and saves then
  stay on this computer only. To answer Steam, log the child out and open
  Steam for the child from your own account. A way to answer it without
  logging the child out is planned.
  The journal command above also says it for each such launch: "Steam is
  waiting for an answer about …" or "Steam refused to start …".
  Steam keeps the question for that game. The game stays held after the
  internet comes back, until the child logs out and back in. Each tap on
  it says "did not start" again, and the journal names the question again.
  For Steam Cloud's question, once Steam has signed in again, the screen
  also says "Log out and back in, then try it again.", and the journal
  line ends "Log the child out and back in, then try it again." Other
  games still start.
- If the journal says "Steam could not sign in", Steam no longer accepts the
  child's saved sign-in. Log the child out, open Steam for the child from
  your own account, and sign in again. If it says "Steam could not reach
  the internet", check the Wi-Fi; games Steam has already set up may still
  start without it.
- The first time a child taps a Linux game, the child's Steam sometimes
  stops part-way while it fetches the tools the game runs in (#206). The
  launcher starts the child's Steam again by itself and taps the game
  again; the child keeps seeing "Getting it ready", and the game comes a
  minute or two later. The journal says "Steam stopped getting … ready;
  starting Steam again." If the game still does not start, the screen adds
  "Log out and back in, then try it again." and the journal says "Steam
  stopped getting … ready, so … did not start." or "Starting Steam again
  did not bring …". Log the child out, log them back in, and tap the game
  again. If the journal also says "Steam did not stop when asked", "Steam
  could not be started again" or "Steam is not running as the launcher
  started it", please report it.
- Right after a child logs in, Steam sometimes updates itself, which can
  take ten minutes or more on slow Wi-Fi. The child sees the tiles, and
  Steam's update window is hidden; a game tapped then says "Getting it
  ready". The journal says "Steam is updating itself" for each window it
  hid.
- If the set-up command says some of a game's files can be read only by
  your own account, your own Steam made them private. This, in your own
  login, lets every account read them, and the children get their copies
  at the next run. Please report it too.

  ```sh
  chmod -R o+rX "/var/lib/cairn/steam-library/steamapps/common/Some Game"
  ```

- Please report what you find as a
  [GitHub issue](https://github.com/Cairn-Linux/cairn/issues). Leave out
  children's names, photos and anything else personal.

## Known gaps

- Level 2 looks the same as level 1 for now.
- In a child's session the brightness keys do nothing, and a second
  screen extends the desktop instead of mirroring it.
- The battery shows above the tiles and says when it is low, but not
  while a game is open, so keep a laptop plugged in.
- Running the script again puts the repository's tiles back, so a tile
  you added by hand goes. Game tiles come from the shared folder and
  stay.
- After a system update that brings a newer Qt, rebuild in the toolbox and
  run the script again.

## Undo

Reinstall Bazzite, or go back to a snapshot of your virtual machine.

---

These steps were checked on 2026-10-01 in a Bazzite 44 virtual machine,
from the toolbox build to a child's session. Steam for a child was checked
there on 2026-10-12, with no Steam account signed in. The games section
has not been tried as written: adding the folder in Steam, installing into
it, Move, and the set-up command's copy step, game tiles and pictures have
not been run on a real computer yet. The kids' laptop got its copies by
hand on 2026-10-13.
