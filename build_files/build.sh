#!/bin/bash
# Cairn Linux image build script. Runs inside the Containerfile with the repo's
# build_files/ mounted at /ctx and system_files/ at /ctx/system_files.
# Phase 1 fills this in (ROADMAP P1-1..P1-4). Kept runnable so the image
# builds even now.

set -ouex pipefail

# Lay system_files/ over / — session entries, greetd config, PAM rules,
# launcher and shell packaging outputs, brand fonts.
cp -avf /ctx/system_files/. /

### Packages that must live in the image, not in Flatpak (DESIGN §4.2):
### the launcher, the restricted shell, session plumbing, the kiosk
### compositor, ScummVM. Uncomment as each lands.
# dnf5 install -y cage scummvm

### Everything else arrives as Flatpak at first boot or via the Guardian tool:
### GCompris (org.kde.gcompris), Tux Paint (org.tuxpaint.Tuxpaint), etc.

### Services
# systemctl enable greetd.service
