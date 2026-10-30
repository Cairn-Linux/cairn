#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Cairn Linux image build script. Runs in the Containerfile's final stage,
# after packages.sh has layered the packages and Cairn's own files are in
# place, with the repo's build_files/ mounted at /ctx and system_files/ at
# /ctx/system_files. It does the steps that are not files the build makes
# (ADR-0038, "What the image does itself").

set -ouex pipefail

# Lay system_files/ over /, all but its own README: the groups, the
# folders in /var and the login screen's presets.
find /ctx/system_files -mindepth 1 -maxdepth 1 ! -name README.md \
    -exec cp -avf {} / \;

# Plasma's Users page is hidden, so the family's accounts are changed in
# Cairn's Family app, which System Settings lists in its place (ADR-0043).
/ctx/hide-users-page.sh /etc/xdg/kdeglobals

# SDDM is the login screen (ADR-0016). Plasma Login Manager goes first,
# because both claim display-manager.service and only one may hold it.
systemctl disable plasmalogin.service
systemctl enable sddm.service

# The visit's copy of cage, which a child may run but not read (ADR-0032).
# Linux then keeps the child's other programs from tracing it or copying
# its connection to the Guardian's desktop. Made again at every build, so
# it always matches the base's cage.
install -m 0711 /usr/bin/cage /usr/libexec/cairn/cage-visit

# Only a Guardian may run the window that opens a child's Steam, the
# Guardian tool, or their wrappers. The group is made at first boot, so the
# files take its fixed number from sysusers.d now.
guardian_gid=$(awk '$1 == "g" && $2 == "cairn-guardian" { print $3 }' \
    /usr/lib/sysusers.d/cairn.conf)
[[ "${guardian_gid}" =~ ^[0-9]+$ ]]
guardian_only=(
    /usr/bin/cairn-steam-for /usr/libexec/cairn/cairn-steam-for
    /usr/bin/cairn-guardian /usr/libexec/cairn/cairn-guardian
)
chgrp "${guardian_gid}" "${guardian_only[@]}"
chmod 0750 "${guardian_only[@]}"

# A lockout after wrong Guardian passwords that survives a reboot
# (ADR-0020). authselect writes a checksum of its files to
# /var/lib/authselect, which the image cannot ship; it only uses it to
# notice files changed by hand, and rpm-ostree's tmpfiles remake the
# folder at boot.
authselect enable-feature with-faillock
rm -r /var/lib/authselect
rmdir --ignore-fail-on-non-empty /var/lib

# SELinux labels. pam_faillock may write its count only in a faillog_t
# folder. The login screen and the consoles read the list of children on
# a visit only with etc_t, and PAM lets everyone in when it cannot read it.
semanage fcontext -a -t faillog_t '/var/lib/faillock(/.*)?'
semanage fcontext -a -t etc_t '/run/cairn(/.*)?'

# Cairn's name, mark and licence texts in place of Bazzite's (slice 5).
/ctx/name-and-logos.sh

# The boot screen, and the initramfs built again with it and with Cairn's
# os-release (#165). Last, so the initramfs holds everything above.
/ctx/boot-screen.sh

### Not here, ever: accounts, games.json, a child's "Steam for <child>" menu
### entry and the Flatpaks name a person or are the family's choice.
### provision/cairn-family-setup.sh makes them on the computer (slice 4).
###
### Not here yet: the Guardian's desktop (#166).
