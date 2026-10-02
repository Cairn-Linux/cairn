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

## If something goes wrong

- **Ctrl-Alt-Home** ends a stuck app and brings the tiles back.
- Holding the **power button** for five seconds powers off in order. A tap
  does nothing.
- From the grown-up's account, this shows what went wrong:

  ```sh
  journalctl -t cairn-launcher -t cairn-kiosk --since today
  ```

- Please report what you find as a
  [GitHub issue](https://github.com/Cairn-Linux/cairn/issues). Leave out
  children's names, photos and anything else personal.

## Known gaps

- Level 2 looks the same as level 1 for now.
- In a child's session the brightness keys do nothing, nothing warns of a
  low battery, and a second screen extends the desktop instead of
  mirroring it. Keep a laptop plugged in.
- Adding games, such as Steam's ScummVM classics, is not covered here yet.
- Running the script again puts the default tiles back.
- After a system update that brings a newer Qt, rebuild in the toolbox and
  run the script again.

## Undo

Reinstall Bazzite, or go back to a snapshot of your virtual machine.

---

These steps were checked on 2026-10-01 in a Bazzite 44 virtual machine,
from the toolbox build to a child's session.
