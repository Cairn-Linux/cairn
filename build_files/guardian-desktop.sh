#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# The Guardian's Plasma desktop in Cairn's frame (#166). build.sh runs it
# after name-and-logos.sh, with the generated brand files at /ctx/brand.
# build_files/check-guardian-desktop.sh checks the result.
#
# First step: the start-menu button shows the Cairn mark. Valve's look,
# the one a new Guardian gets, points it at `distributor-logo-steamdeck`,
# which name-and-logos.sh already leads to the mark. A Guardian whose
# button names `start-here` would get generic-logos' plain icon, a file
# Cairn leaves as the package shipped it (ADR-0038). So Cairn keeps its
# own small hicolor folder, which the Guardian's Plasma reads first
# (system_files/etc/xdg/plasma-workspace/env/cairn-guardian-icons.sh).
# The wallpaper, colours, splash and lock screen are later steps.

set -ouex pipefail

mark=/usr/share/icons/hicolor/scalable/places/cairn-logo-on-ink.svg
[[ -f "${mark}" ]]

# The Sand mark, as name-and-logos.sh chose for every Bazzite place name,
# because Valve's panel is dark. Both folders, because hicolor has the
# name in both and a lookup may try either first.
overlay=/usr/share/cairn/guardian-desktop/icons/hicolor
for folder in scalable/apps scalable/places; do
    install -d -m 0755 "${overlay}/${folder}"
    ln -sf "${mark}" "${overlay}/${folder}/start-here.svg"
done
