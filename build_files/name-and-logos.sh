#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Gives the image Cairn's name and mark in place of Bazzite's (ADR-0038,
# slice 5). build.sh runs it after system_files/ is laid down, with the
# generated brand files at /ctx/brand and the licence texts at
# /ctx/licenses. packages.sh has already swapped fedora-logos for
# generic-logos. build_files/check-name-and-logos.sh checks the result.

set -ouex pipefail

places=/usr/share/icons/hicolor/scalable/places

# The base's own line for each key that stays: VERSION_ID is the Fedora
# release, which dnf, rpm-ostree and bootc-image-builder all depend on.
base_value() {
    sed -n "s/^$1=//p" /usr/lib/os-release | tr -d "\"'"
}
version_id=$(base_value VERSION_ID)
support_end=$(base_value SUPPORT_END)
ostree_version=$(base_value OSTREE_VERSION)
[[ "${version_id}" == "$(rpm -E %fedora)" ]]
[[ -n "${support_end}" && -n "${ostree_version}" ]]
ansi_color=$(cat /ctx/brand/ansi-color)
[[ "${ansi_color}" =~ ^0\;38\;2\;[0-9]+\;[0-9]+\;[0-9]+$ ]]

# install writes a new file with the mode it is given, so the result never
# depends on the old file or on how the storage driver shares it.
cat >/tmp/os-release <<EOF
NAME="Cairn Linux"
ID=cairn
ID_LIKE="fedora"
VERSION_ID=${version_id}
VERSION="${version_id}"
PRETTY_NAME="Cairn Linux"
ANSI_COLOR="${ansi_color}"
LOGO=cairn-logo-on-ink
DEFAULT_HOSTNAME="cairn"
HOME_URL="https://cairnlinux.com/"
BUG_REPORT_URL="https://github.com/Cairn-Linux/cairn/issues"
SUPPORT_END=${support_end}
OSTREE_VERSION='${ostree_version}'
IMAGE_ID=cairn
EOF
install -m 0644 /tmp/os-release /usr/lib/os-release

# bootupd names a new install's firmware boot entry from the words before
# "release" here.
printf 'Cairn Linux release %s\n' "${version_id}" >/tmp/fedora-release
install -m 0644 /tmp/fedora-release /usr/lib/fedora-release

# The licence texts for Cairn's own files in the image, the mark among
# them.
install -d -m 0755 /usr/share/licenses/cairn
install -m 0644 /ctx/licenses/* /usr/share/licenses/cairn/

# The mark, square so no icon loader stretches it. Every Bazzite place
# name now leads to the Sand mark, because each one is drawn on a dark
# ground today: the Guardian's panel and the splash. os-release's LOGO
# names it too, because System Settings' About page draws it in the
# Guardian's dark Vapor window colours.
install -m 0644 /ctx/brand/cairn-logo.svg /ctx/brand/cairn-logo-on-ink.svg \
    "${places}/"
ln -sf cairn-logo-on-ink.svg "${places}/distributor-logo.svg"
ln -sf cairn-logo-on-ink.svg "${places}/distributor-logo-white.svg"
# distributor-logo-steamdeck.svg, bazzite-logo.svg and
# bazzite-logo-white.svg already link to the two above.

# Bazzite logos nothing names.
rm "${places}/bazzite-logo-le.svg"
rm /usr/share/icons/hicolor/*x*/bazzite-logo-icon.png

# The login splash of both Valve looks. Their Splash.qml names this file,
# so it keeps Bazzite's file name.
for look in com.valve.vapor.desktop com.valve.vgui.desktop; do
    splash=/usr/share/plasma/look-and-feel/${look}/contents/splash/images
    gzip -n -9 -c /ctx/brand/cairn-logo-on-ink.svg >/tmp/bazzite_logo.svgz
    install -m 0644 /tmp/bazzite_logo.svgz "${splash}/bazzite_logo.svgz"
done

# Bazzite's welcome and support channels, in files no package owns: the
# banner in every terminal, its fastfetch logo, notices from Bazzite, the
# documentation entry, and the first-login portal of every new account.
rm /usr/libexec/ublue-motd
rm /etc/profile.d/bazzite-neofetch.sh
rm /usr/share/fish/vendor_conf.d/bazzite-neofetch.fish
rm /etc/xdg/autostart/bazzite-announcement.desktop
rm /usr/share/applications/bazzite-documentation.desktop
rm /etc/skel/.config/autostart/bazzite-portal.desktop

# Every file in the image has the same time once deployed, so programs
# never see that the icons changed. They look a new name up in this
# cache, which must list it.
gtk-update-icon-cache -f /usr/share/icons/hicolor

# build.sh takes the icons' fingerprint once guardian-desktop.sh has
# added Cairn's own.
