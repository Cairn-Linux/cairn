#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Hides Plasma's Users page from System Settings with KDE Kiosk (ADR-0043),
# so a parent changes the family's accounts in Cairn's Family app, where
# Cairn knows about them. Family is listed in System Settings in its place.
#
# Bazzite already ships /etc/xdg/kdeglobals with a [KDE Control Module
# Restrictions][$i] group of its own (kcm_updates=false), so this adds
# kcm_users=false to that group, or the group to the end of the file when
# there is none, and keeps every other line as it was. Run again, it
# changes nothing. Usage: hide-users-page.sh [FILE], /etc/xdg/kdeglobals by
# default.

set -ouex pipefail

file="${1:-/etc/xdg/kdeglobals}"
group='[KDE Control Module Restrictions]'
key='kcm_users=false'
immutable="[\$i]"

# Whether the group, with or without [$i], already has the key.
has_key() {
    awk -v group="${group}" -v key="${key}" '
        /^\[/ { in_group = (index($0, group) == 1) }
        in_group && $0 == key { found = 1 }
        END { exit !found }
    ' "${file}"
}

touch "${file}"
if has_key; then
    exit 0
fi

if grep -qF "${group}" "${file}"; then
    # The key goes on the line after the group's first header.
    awk -v group="${group}" -v key="${key}" '
        { print }
        !done && index($0, group) == 1 { print key; done = 1 }
    ' "${file}" > "${file}.cairn-new"
    cat "${file}.cairn-new" > "${file}"
    rm "${file}.cairn-new"
else
    # $i makes the group immutable, so no account's own kdeglobals can
    # show the page again, as in Bazzite's own group.
    if [[ -s "${file}" && -n "$(tail -c 1 "${file}")" ]]; then
        echo >> "${file}"
    fi
    printf '\n%s%s\n%s\n' "${group}" "${immutable}" "${key}" >> "${file}"
fi

has_key
