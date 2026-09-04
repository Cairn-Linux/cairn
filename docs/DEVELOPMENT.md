# Development setup

How to set up a machine to work on Cairn. Written for the Phase 0 dev PC.
Package names and versions below were checked against the Fedora 44 repos and
against an actual Fedora 44 KDE laptop on 2026-09-04.

## Two machines, two jobs

| Machine | Job | What it needs |
|---|---|---|
| **Dev PC** | Write and build the C++/QML code, run VMs, run `kidscan` against a real Steam library | x86_64, KVM, a mutable Fedora with the toolchain below, Steam with a few Humongous titles, ScummVM |
| **Test laptop** | The thing a child actually uses in P0-9 | A **Minimum**-tier machine (ADR-0003): about 2013 or newer, 4 GB RAM, Intel HD 4000-class graphics, UEFI, 64 GB. Test at the floor, not the comfortable case. **Stock Bazzite KDE** (ADR-0006), then `provision/` |

Nothing in this project runs on ARM (DESIGN §7), so an ARM laptop is for
editing documents only.

## Dev PC operating system

**Fedora KDE Plasma Desktop** (the mutable spin, not Kinoite). Reasons: `dnf
install` of development packages works directly, and the L3/L4 target desktop
is Plasma, so daily use is daily exposure to the thing being locked down. If
you would rather run an immutable base, develop inside a `toolbox` container;
everything below applies inside it.

Fedora 44 KDE logs in through **Plasma Login Manager**
(`plasmalogin.service`), not SDDM, and the `sddm` package is not installed by
default. Leave that alone. Installing `sddm` to read its example themes and
run `sddm-greeter-qt6 --test-mode` is fine, but never enable it on the dev
PC. Prototype the greeter (P0-3) in the VM, and make the first step there
recording which display manager stock Bazzite ships.

## Toolchain

```sh
sudo dnf install \
  gcc-c++ clang clang-tools-extra git-clang-format cmake ninja-build \
  libasan libubsan gdb \
  qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtsvg-devel qt6-qttools-devel \
  qt6-qtwayland-devel wayland-protocols-devel qt6-linguist qt6-doc \
  qt-creator ShellCheck just
```

| Package | Version seen | Why |
|---|---|---|
| `gcc-c++` | 16.2 | The compiler. C++20 is fully supported. |
| `libasan`, `libubsan` | 16.2 | The AddressSanitizer and UndefinedBehaviorSanitizer runtimes for gcc. Without them the Debug preset fails to link. clang carries its own runtime in `compiler-rt`, which the `clang` package pulls in. |
| `clang`, `clang-tools-extra`, `git-clang-format` | 22.1 | `clang-format` and `clang-tidy`, which CLAUDE.md requires, and `git clang-format`, which formats only the staged hunks. Also a second compiler that gives different, often clearer, error messages. |
| `cmake`, `ninja-build` | 4.3, 1.13 | The build system (ADR-0002). CMake 4 refuses projects that ask for compatibility older than 3.5; start at 3.28. |
| `gdb` | 17.2 | The debugger behind Qt Creator's debug view. |
| `qt6-qtbase-devel` | 6.11 | Qt core, GUI, `QProcess`, Qt Test. |
| `qt6-qtdeclarative-devel` | 6.11 | QML and Qt Quick, including Quick Controls, plus `qmllint`, `qmlformat`, `qmlls`, `qmltestrunner` and the `qml` runner. They live in `/usr/lib64/qt6/bin/` and as `/usr/bin/*-qt6`. |
| `qt6-qtsvg-devel` | 6.11 | Renders the mark and tile icons from SVG. |
| `qt6-qttools-devel` | 6.11 | Designer, `qdbus`, `pixeltool` and friends. Not the QML tools and not the translation tools. |
| `qt6-linguist` | 6.11 | `lupdate` and `lrelease`. Needed from the first commit because every string is translatable (ADR-0007). |
| `qt6-qtwayland-devel`, `wayland-protocols-devel` | 6.11, 1.49 | Generate the Wayland client binding the launcher needs to hear about foreign windows (P0-10). The scanner is `/usr/lib64/qt6/libexec/qtwaylandscanner`; CMake finds it. |
| `qt6-doc` | 6.9 | Offline Qt documentation, browsable in Qt Creator's Help mode. It trails the libraries by two minor versions. |
| `qt-creator` | 20.0 | Optional but recommended for a learner: jump-to-definition, a debugger with a GUI, a live QML preview. The binary is `qtcreator`. |
| `ShellCheck`, `just` | — | CLAUDE.md's shell-script rule; the upstream image `Justfile` in Phase 1. |

## Phase 0 targets and apps

```sh
sudo dnf install cage labwc sddm scummvm tuxpaint gcompris-qt \
  malcontent malcontent-control malcontent-tools malcontent-pam \
  xdotool xterm xeyes foot wlr-randr podman
```

| Package | Version seen | Role |
|---|---|---|
| `cage`, `labwc` | 0.3, 0.9 | Kiosk compositor candidates (D3). labwc is the primary candidate, cage the measurement baseline. Both run nested inside the Plasma session for behaviour checks; VT switching and logind rows need the VM console or the test laptop. |
| `sddm` | 0.21 | Display manager candidate (D6). Install for its themes and test mode only; never enable it on the dev PC (see above). |
| `scummvm` | 2.9 | Native engine for the Humongous titles; `kidscan` runs `scummvm --detect` and needs it on `PATH`. |
| `tuxpaint`, `gcompris-qt` | 0.9, 26.1 | Phase 0 app set. |
| `malcontent`, `malcontent-control`, `malcontent-tools`, `malcontent-pam` | 0.14 | Parental controls. Fedora splits malcontent into daemons, the control app, the CLI and the PAM module; P0-7 needs all four. |
| `xdotool`, `xterm`, `xeyes`, `foot`, `wlr-randr` | — | X11 and Wayland test clients for P0-4's focus-stealing and placeholder rows. |
| `podman` | — | Builds the image locally in Phase 1; runs `bootc-image-builder`. |

Steam: install the RPM (`sudo dnf install steam`, from `rpmfusion-nonfree`).
Bazzite ships the client as a native RPM and `tools/kidscan` emits
`steam -applaunch`, so the RPM on the dev PC matches the target; the Flatpak
client needs a different launch command (issue #24). Then, by hand: sign in
and install two or three Humongous Entertainment titles so `kidscan` can be
run against a real library. P0-10 also needs a Steam Family, which means a
second account held by someone 13 or older (issue #34).

Fonts: `atkinson-hyperlegible-next-fonts` and `atkinson-hyperlegible-mono-fonts`
(see the note at the end of this file).

## Flathub

Fedora ships only its own Flatpak remote. `provision/` and `build.sh` name
Flathub IDs, so add Flathub and install the Phase 0 set once:

```sh
flatpak remote-add --if-not-exists flathub \
  https://dl.flathub.org/repo/flathub.flatpakrepo
flatpak install flathub org.kde.gcompris org.tuxpaint.Tuxpaint \
  org.scummvm.ScummVM
```

## Virtual machines

P0-2 is done when the provisioning script runs clean twice on a fresh VM,
P0-3 changes the display manager, and P0-7 restricts a user under Plasma.
All three happen in a VM of stock Bazzite KDE, not on the dev PC.

```sh
sudo dnf group install virtualization
sudo dnf install edk2-ovmf virtiofsd guestfs-tools
sudo systemctl enable --now libvirtd
sudo usermod -aG libvirt "$USER"      # then log out and back in
```

Download the Bazzite KDE desktop ISO for Intel/AMD graphics (not the Nvidia
build; ADR-0006) into `/var/lib/libvirt/images/`. Do not build a disk image
with `bootc-image-builder` for this; Phase 0 has no image plumbing. Create
the VM at the Minimum tier (ADR-0003), UEFI only:

```sh
sudo virt-install --name cairn-min --memory 4096 --vcpus 2 \
  --cpu host-passthrough --disk pool=default,size=64,format=qcow2 \
  --boot uefi --graphics spice --video virtio \
  --osinfo detect=on,require=off \
  --cdrom /var/lib/libvirt/images/<bazzite-kde>.iso
```

Snapshot after the first login (`virsh snapshot-create-as cairn-min fresh`)
and revert before each provisioning run. The VM can exercise provisioning,
groups, PAM, the display manager, VT switching through virt-viewer's
send-key menu, and hold the 4 GB floor. It cannot stand in for Intel HD 4000
graphics: the guest renders with llvmpipe, so RSS and latency there are an
upper bound. Published numbers come from the test laptop.

## Building

```sh
cmake --preset debug          # -Wall -Wextra -Werror, ASan + UBSan
cmake --build --preset debug
ctest --preset debug
```

Presets live in `CMakePresets.json` at the repo root. A Debug build is the
default for development: a crash prints a stack trace with file and line
instead of "Segmentation fault". Release builds are for the image only. The
presets do not pin a compiler; `CC=clang CXX=clang++ cmake --preset debug`
picks clang for the clearer messages.

## Quality gates

Run these before every commit that touches code. Each one is a single
command with no options to remember, and each exits non-zero on failure.

```sh
cmake --preset debug && cmake --build --preset debug   # warnings are errors
ctest --preset debug                                    # Qt Test, sanitizers on
git clang-format --diff --staged                        # empty output = clean
clang-tidy -p build/debug $(git ls-files '*.cpp')       # .clang-tidy, errors on any warning
cmake --build --preset debug --target all_qmllint     # qmllint over every QML module
for f in $(git ls-files '*.qml'); do
  /usr/lib64/qt6/bin/qmlformat "$f" | diff -q - "$f" || exit 1
done
python3 brand/build.py --check                          # tokens.json -> outputs, byte-identical
python3 tools/kidscan/tests/test_kidscan.py
```

Translations: `lupdate` collects every `tr()` and `qsTr()` string into the
`.ts` file that lives beside the component (ADR-0007). The first CMake
project (P0-5) wires it in as a target and creates `launcher/i18n/`; the
command it runs is

```sh
/usr/lib64/qt6/bin/lupdate launcher -ts launcher/i18n/launcher_en.ts
```

Formatting in place, when a gate fails: `git clang-format` for the staged
C++ hunks, `/usr/lib64/qt6/bin/qmlformat -i file.qml` for QML,
`python3 brand/build.py` to regenerate the brand outputs.

## Git identity and sign-off

Set your own name and email once (`git config --global user.name` and
`user.email`) and use the same identity on every commit; a consistent
pseudonym and a GitHub noreply address are fine. `git commit -s` adds the
DCO trailer (ADR-0005). Git has no setting that adds it on every commit
(`format.signOff` only affects `format-patch`), so keep `-s` a habit or
enable the sign-off lines in the sample `prepare-commit-msg` hook git ships.
Pushes go over HTTPS through `gh auth git-credential`, so no SSH key is
needed, and GPG or SSH signatures are not required.

## Reading the code as a learner

- Every C++ or Qt concept is explained once, in the commit or PR that
  introduced it (CLAUDE.md, "Explain the new thing once"). `git log -p` on a
  file is the textbook for that file.
- Qt Creator's "Follow Symbol Under Cursor" (F2) answers "where is this
  defined" faster than any search.
- The Qt 6 documentation is installed by `qt6-doc` and browsable in Qt
  Creator's Help mode, offline.

## What the dev PC cannot measure

Nothing measured on a laptop with 30 GB of RAM and a discrete GPU is a floor
number. Nested compositor runs are for behaviour only; wlroots reads
`WLR_RENDER_DRM_DEVICE` if you want them on the integrated GPU. The 4 GB
floor is the VM; the graphics floor is the test laptop.

## Fonts

Both are in the plain Fedora repos, so the image build can `dnf5 install` them
without any third-party repo:

| Package | Version | Repo | Licence |
|---|---|---|---|
| `atkinson-hyperlegible-next-fonts` | 2.100 | Fedora 44 | OFL-1.1 |
| `atkinson-hyperlegible-mono-fonts` | 2.100 | Fedora 44 | OFL-1.1 |

```sh
sudo dnf install atkinson-hyperlegible-next-fonts atkinson-hyperlegible-mono-fonts
```

Note the family names. The package ships **Atkinson Hyperlegible Next**, the
Braille Institute's second-generation family, not the original "Atkinson
Hyperlegible" the brand guide names and Google Fonts serves. fontconfig has no
alias from the old name, so `fc-match "Atkinson Hyperlegible"` silently
returns Noto Sans. The QML and CSS tokens use the family name the installed
font actually reports (`fc-list | grep -i atkinson`), and the brand guide
should say "Next" (issue #20). Both faces are published under the SIL Open
Font License, which permits redistribution in the image (ROADMAP §7).
