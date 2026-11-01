#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Checks the Guardian's desktop where guardian-desktop.sh and
# kde-pictures.sh changed it (#166). Runs in its own step of the
# Containerfile after build.sh, so a base update that renames the
# start-menu icon, gives Breeze one of its own, or moves KDE's pictures
# to another package stops the build until it is dealt with.

set -ouex pipefail

mark=/usr/share/icons/hicolor/scalable/places/cairn-logo-on-ink.svg
env_script=/etc/xdg/plasma-workspace/env/cairn-guardian-icons.sh
cairn_folder=/usr/share/cairn/guardian-desktop

# What XDG_DATA_DIRS becomes after Plasma sources the script at login,
# given what it was before. The inner shell expands its own variables.
# shellcheck disable=SC2016
after_login() {
    env -i XDG_DATA_DIRS="$1" sh -c '. "$1"; . "$1"; printf "%s" "${XDG_DATA_DIRS}"' \
        sh "${env_script}"
}
# Cairn's folder goes first, once, however often the script runs, and
# what was there stays in its order.
[[ "$(after_login '')" == "${cairn_folder}:/usr/local/share:/usr/share" ]]
[[ "$(after_login /a/share:/usr/share)" == "${cairn_folder}:/a/share:/usr/share" ]]
[[ "$(after_login "${cairn_folder}:/usr/share")" == "${cairn_folder}:/usr/share" ]]

# Cairn's folder holds icons and nothing else, so putting it first
# changes no menu entry, file type or other data a program looks up.
[[ "$(ls -A "${cairn_folder}")" == icons ]]

# Valve's two looks, the one a new Guardian gets among them, still name
# this icon for the start-menu button.
for look in com.valve.vapor.desktop com.valve.vgui.desktop; do
    grep -qxF 'applet.writeConfig("icon", "distributor-logo-steamdeck")' \
        "/usr/share/plasma/look-and-feel/${look}/contents/plasmoidsetupscripts/org.kde.plasma.kickoff.js"
done

# Breeze has neither name at any size, so its lookups fall through to
# hicolor, where Cairn's folder answers first.
found=$(find /usr/share/icons/breeze /usr/share/icons/breeze-dark \
    \( -name 'start-here.*' -o -name 'distributor-logo*' \) -print)
[[ -z "${found}" ]]

# The start-menu names a Guardian's button may hold lead to the mark,
# asked as Plasma's own icon loader asks, with Valve's Breeze Dark, with
# Breeze, and with no icon theme set.
config=$(mktemp -d)
for theme in breeze-dark breeze ''; do
    if [[ -n "${theme}" ]]; then
        printf '[Icons]\nTheme=%s\n' "${theme}" >"${config}/kdeglobals"
    else
        rm -f "${config}/kdeglobals"
    fi
    for icon in distributor-logo-steamdeck start-here; do
        path=$(env -i HOME="${config}" XDG_CONFIG_HOME="${config}" \
            XDG_DATA_DIRS="$(after_login '')" QT_QPA_PLATFORM=offscreen \
            kiconfinder6 "${icon}")
        cmp "$(readlink -f "${path}")" "${mark}"
    done
done
rm -r "${config}"

# Plasma's picture folder holds every one of KDE's own pictures and
# nothing else, so Family's Your account page offers no game art
# (ADR-0043). grep stops the build here if plasma-workspace ships none.
avatars=/usr/share/plasma/avatars
kde_pictures=$(rpm -ql plasma-workspace | grep "^${avatars}/" | LC_ALL=C sort)
[[ "$(find "${avatars}" -mindepth 1 | LC_ALL=C sort)" == "${kde_pictures}" ]]
# Among them, what Family lists: KDE's drawings, and its photos.
ls "${avatars}"/*.png "${avatars}"/photos/*.png >/dev/null

# GNOME's face folder, where Bazzite kept its copies of the game art,
# holds nothing that no package owns.
if [[ -e /usr/share/pixmaps/faces ]]; then
    find /usr/share/pixmaps/faces -print0 | xargs -0 rpm -qf
fi

# The icon cache's fingerprint covers Cairn's folder too.
[[ -s /usr/share/cairn/icons.sum ]]
