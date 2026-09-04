# Development setup

How to set up a machine to work on Cairn. Written for the Phase 0 dev PC.
Package names and versions below were checked against the Fedora 44 repos on
2026-09-03.

## Two machines, two jobs

| Machine | Job | What it needs |
|---|---|---|
| **Dev PC** | Write and build the C++/QML code, run VMs, run `kidscan` against a real Steam library | x86_64, KVM, a mutable Fedora with the toolchain below, Steam with a few Humongous titles, ScummVM |
| **Test laptop** | The thing a child actually uses in P0-9 | A 2013–2018 x86_64 laptop at whatever hardware floor ROADMAP §1 settles on. Stock Fedora Kinoite or Bazzite, then `provision/` |

Nothing in this project runs on ARM (DESIGN §7), so an ARM laptop is for
editing documents only.

## Dev PC operating system

**Fedora KDE Plasma Desktop** (the mutable spin, not Kinoite). Reasons: `dnf
install` of development packages works directly, and the L3/L4 target desktop
is Plasma, so daily use is daily exposure to the thing being locked down. If
you would rather run an immutable base, develop inside a `toolbox` container;
everything below applies inside it.

## Toolchain

```sh
sudo dnf install \
  gcc-c++ clang clang-tools-extra cmake ninja-build \
  qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtsvg-devel qt6-qttools-devel \
  qt-creator
```

| Package | Version seen | Why |
|---|---|---|
| `gcc-c++` | 16.2 | The compiler. C++20 is fully supported. |
| `clang`, `clang-tools-extra` | 22.1 | `clang-format` and `clang-tidy`, which CLAUDE.md requires. Also a second compiler that gives different, often clearer, error messages. |
| `cmake`, `ninja-build` | 4.3, 1.13 | The build system (ADR-0002). |
| `qt6-qtbase-devel` | 6.11 | Qt core, GUI, `QProcess`, Qt Test. |
| `qt6-qtdeclarative-devel` | 6.11 | QML and Qt Quick, including Quick Controls (folded into this package in Qt 6). |
| `qt6-qtsvg-devel` | 6.11 | Renders the mark and tile icons from SVG. |
| `qt6-qttools-devel` | 6.11 | `qmllint`, `qmlformat`, translations tooling. |
| `qt-creator` | 20.0 | Optional but recommended for a learner: jump-to-definition, a debugger with a GUI, a live QML preview. |

## Phase 0 targets and apps

```sh
sudo dnf install cage labwc sddm scummvm tuxpaint gcompris-qt malcontent podman
```

| Package | Version seen | Role |
|---|---|---|
| `cage`, `labwc` | 0.3, 0.9 | Kiosk compositor candidates (D3). |
| `sddm` | 0.21 | Display manager; the Cairn login screen is an SDDM theme (D6). |
| `scummvm` | 2.9 | Native engine for the Humongous titles; `kidscan --detect` needs it. |
| `tuxpaint`, `gcompris-qt` | 0.9, 26.1 | Phase 0 app set. |
| `malcontent` | 0.14 | Parental controls; P0-7 tests it under Plasma in a VM. |
| `podman` | — | Builds the image locally in Phase 1; runs `bootc-image-builder`. |

Steam: install from RPM Fusion or as the Flatpak, sign in, and install two or
three Humongous Entertainment titles so `tools/kidscan` can be run against a
real library (issue #24 lists what to check).

Fonts: `atkinson-hyperlegible-next-fonts` and `atkinson-hyperlegible-mono-fonts`
(see the note at the end of this file).

## Building (once the first CMake project lands in P0-5)

```sh
cmake --preset debug      # -Wall -Wextra -Werror, ASan + UBSan
cmake --build --preset debug
ctest --preset debug
```

Presets live in `CMakePresets.json` at the repo root. A Debug build is the
default for development: a crash prints a stack trace with file and line
instead of "Segmentation fault". Release builds are for the image only.

Before committing C++:

```sh
clang-format -i $(git diff --name-only --cached -- '*.cpp' '*.h')
```

## Reading the code as a learner

- Every C++ or Qt concept is explained once, in the commit or PR that
  introduced it (CLAUDE.md, "Explain the new thing once"). `git log -p` on a
  file is the textbook for that file.
- Qt Creator's "Follow Symbol Under Cursor" (F2) answers "where is this
  defined" faster than any search.
- The Qt 6 documentation is installed with `qt6-qttools-devel` and browsable
  in Qt Creator's Help mode, offline.

## Fonts

Both are in the plain Fedora repos, so the image build can `dnf5 install` them
without any third-party repo:

| Package | Version | Repo |
|---|---|---|
| `atkinson-hyperlegible-next-fonts` | 2.100 | Fedora 44 |
| `atkinson-hyperlegible-mono-fonts` | 2.100 | Fedora 44 |

```sh
sudo dnf install atkinson-hyperlegible-next-fonts atkinson-hyperlegible-mono-fonts
```

Note the family names. The package ships **Atkinson Hyperlegible Next**, the
Braille Institute's second-generation family, not the original "Atkinson
Hyperlegible" the brand guide names and Google Fonts serves. The QML and CSS
tokens must use the family name the installed font actually reports
(`fc-list | grep -i atkinson`), and the brand guide should say "Next" (issue
#20). Both families are published under the SIL Open Font License; verify the
**Mono** face's licence text in the package before it ships in the image
(ROADMAP §7).
