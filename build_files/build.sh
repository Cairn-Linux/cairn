#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Cairn Linux image build script. Runs in the Containerfile's final stage,
# after Cairn's own files are in place, with the repo's build_files/ mounted
# at /ctx and system_files/ at /ctx/system_files.

set -ouex pipefail

# Lay system_files/ over /, all but its own README. Empty for now: every
# file Cairn ships comes from `cmake --install` in the build stage
# (ADR-0038).
find /ctx/system_files -mindepth 1 -maxdepth 1 ! -name README.md \
    -exec cp -avf {} / \;

### Slice 3 of #164 (ADR-0038, "What the image does itself"):
### - layered packages: labwc, ScummVM, SDDM and the brand's fonts, with
###   sddm.service enabled and plasmalogin.service disabled;
### - the visit's cage: install -m 0711 /usr/bin/cage /usr/libexec/cairn/cage-visit;
### - groups from sysusers.d and folders from tmpfiles.d;
### - the Guardian's window owned by root:cairn-guardian;
### - authselect with-faillock and the semanage fcontext labels.

### Everything else arrives as Flatpak at first boot or via the Guardian tool:
### GCompris (org.kde.gcompris), Tux Paint (org.tuxpaint.Tuxpaint), etc.
