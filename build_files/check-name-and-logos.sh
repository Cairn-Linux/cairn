#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Checks that the image says Cairn, not Bazzite or Fedora, where
# name-and-logos.sh changed it (ADR-0038, slice 5). Runs in its own step
# of the Containerfile after build.sh, so a base update that brings a new
# Bazzite logo or welcome file stops the build until it is dealt with.

set -ouex pipefail

# set -e ignores a failure inverted with "!", so a check that something
# is absent goes through this.
absent() {
    if "$@"; then
        echo "Found what must not be here: $*" >&2
        exit 1
    fi
}

places=/usr/share/icons/hicolor/scalable/places
mark=${places}/cairn-logo-on-ink.svg

# os-release: Cairn's name, the Fedora release bootc-image-builder and
# the package tools need, and the console colour from the brand tokens.
(
    # shellcheck source=/dev/null
    . /usr/lib/os-release
    [[ "${ID}" == cairn ]]
    [[ "${NAME}" == "Cairn Linux" ]]
    [[ " ${ID_LIKE} " == *" fedora "* ]]
    [[ "${VERSION_ID}" == "$(rpm -E %fedora)" ]]
    [[ "${ANSI_COLOR}" == "$(cat /ctx/brand/ansi-color)" ]]
    [[ "${LOGO}" == cairn-logo-on-ink ]]
    # Bazzite's custom-resolution-helper stops when this is empty.
    [[ -n "${OSTREE_VERSION}" ]]
)
grep -qx 'Cairn Linux release [0-9]*' /usr/lib/fedora-release
for file in /usr/lib/os-release /usr/lib/fedora-release; do
    [[ "$(stat -c %a "${file}")" == 644 ]]
done

# No Bazzite or Fedora identity, URL or support channel where the system
# names itself.
absent grep -Ei 'bazzite|ublue|universal.blue|fedoraproject|discord|REDHAT_' \
    /usr/lib/os-release /usr/lib/fedora-release /etc/xdg/kcm-about-distrorc

# Fedora's logos are gone; its generic ones stand in, unchanged, and
# satisfy every package that needs system logos.
absent rpm -q fedora-logos
absent rpm -q fedora-release-notes
rpm -q generic-logos
[[ -z "$(rpm -V --nomtime generic-logos)" ]]
[[ "$(rpm -q --whatprovides system-logos --qf '%{NAME}\n')" == generic-logos ]]
[[ ! -e /etc/favicon.png.rpmsave ]]

# Every Bazzite place name shows the Sand mark.
for icon in distributor-logo distributor-logo-white distributor-logo-steamdeck \
    bazzite-logo bazzite-logo-white; do
    cmp "$(readlink -f "${places}/${icon}.svg")" "${mark}"
done
cmp "${places}/cairn-logo.svg" /ctx/brand/cairn-logo.svg
cmp "${mark}" /ctx/brand/cairn-logo-on-ink.svg

splash_images=/usr/share/plasma/look-and-feel/com.valve.vapor.desktop/contents/splash/images
vgui_images=/usr/share/plasma/look-and-feel/com.valve.vgui.desktop/contents/splash/images
cmp <(zcat "${splash_images}/bazzite_logo.svgz") "${mark}"
cmp <(zcat "${vgui_images}/bazzite_logo.svgz") "${mark}"

# Nothing else with Bazzite in its name where a Guardian sees icons, menu
# entries, autostarted programs or terminal start-up files. Each file
# allowed here is checked above or is listed in ADR-0038 for P1-1.
allowed=$(sort <<EOF
${places}/bazzite-logo.svg
${places}/bazzite-logo-white.svg
${splash_images}/bazzite_logo.svgz
${vgui_images}/bazzite_logo.svgz
/usr/share/applications/bazzite-steam-bpm.desktop
/usr/share/applications/io.github.rfrench3.bazzite-updater.desktop
/usr/share/icons/hicolor/scalable/apps/io.github.rfrench3.bazzite-updater.svg
/usr/share/plasma/shells/org.kde.plasma.desktop/contents/updates/bazzite-pins.js
EOF
)
found=$(find /usr/share/icons /usr/share/pixmaps /usr/share/plasma \
    /usr/share/applications /usr/share/fish /etc/xdg /etc/skel /etc/profile.d \
    -iname '*bazzite*' | sort)
[[ "${found}" == "${allowed}" ]]

# No Bazzite support channel in what a Guardian's session or terminal
# runs at start.
absent grep -rIl 'bazzite\.gg' /etc/xdg /etc/skel /etc/profile.d \
    /usr/share/fish/vendor_conf.d
[[ ! -e /usr/libexec/ublue-motd ]]

# A plain fastfetch draws no logo, Fedora's or Bazzite's. The build's
# root runs it here, with the config an account reads when it has none
# of its own, so every account must be able to read that file.
[[ "$(stat -c %a /etc/xdg/fastfetch)" == 755 ]]
[[ "$(stat -c %a /etc/xdg/fastfetch/config.jsonc)" == 644 ]]

# With only "break", an empty line, to print, all fastfetch writes is its
# logo, and --pipe false makes it draw as it does on a terminal. With no
# config read there must be a logo to see, or this checks nothing. With
# Cairn's, fastfetch writes what it does when told to draw none. cmp
# keeps the drawing out of the build's log.
drawn() {
    fastfetch --pipe false --structure break "$@"
}
absent cmp -s <(drawn --config none) <(drawn --config none --logo none)
cmp <(drawn) <(drawn --config none --logo none)

# Its lines are still there, each from the start of its line, where a
# logo would push it to the right. A config that names no lines prints
# none. The build's root has no home folder, so what the lines look up
# there goes to /tmp. grep -c reads to the end, so fastfetch never
# writes to a closed pipe.
XDG_CACHE_HOME=/tmp/fastfetch-cache fastfetch | grep -c '^OS: Cairn Linux'

# No alias or function gives the word another meaning, as Bazzite's did,
# in these places: what bash, sh and fish read at start for every
# account, the folders fish takes functions from, and what a new account
# is given. -R reads the files that links there point at too, such as
# systemd's two in /etc/profile.d. grep must answer 1, which is "looked
# in every place and found nothing". For a place that is missing it
# answers 2 even when the word is in another, and absent would take that
# for "not found".
status=0
grep -RIlw fastfetch /etc/profile /etc/profile.d /etc/bashrc /etc/skel \
    /etc/fish /usr/share/fish/vendor_conf.d \
    /usr/share/fish/vendor_functions.d || status=$?
[[ "${status}" == 1 ]]

# No new account starts with a fastfetch config of its own, which
# fastfetch would read in place of Cairn's.
[[ ! -e /etc/skel/.config/fastfetch && ! -e /etc/skel/fastfetch ]]

# The icon cache lists the new names, so a deployed system finds them.
# Each name ends in a NUL there, so the Ink name is not matched by the
# Sand one.
for icon in cairn-logo cairn-logo-on-ink; do
    grep -qaP "${icon}\\x00" /usr/share/icons/hicolor/icon-theme.cache
done
[[ -s /usr/share/cairn/icons.sum ]]

# Cairn's licence texts travel with the mark.
for file in LICENSE LICENSE-CC-BY-SA-4.0 NOTICE TRADEMARKS.md; do
    cmp "/usr/share/licenses/cairn/${file}" "/ctx/licenses/${file}"
done
