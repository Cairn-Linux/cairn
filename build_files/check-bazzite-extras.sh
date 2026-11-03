#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Checks that the Bazzite extras bazzite-extras.sh takes out stay out
# (ADR-0038). Runs in its own step of the Containerfile after build.sh, so
# a base update that brings one back stops the build until it is dealt
# with.

set -ouex pipefail

# set -e ignores a failure inverted with "!", so a check that something
# is absent goes through this.
absent() {
    if "$@"; then
        echo "Found what must not be here: $*" >&2
        exit 1
    fi
}

# Bazzite's first-login step (#261): none of its files is left. -L as
# well as -e, because -e follows a link and misses one whose unit has
# gone.
for path in \
    /etc/systemd/user/default.target.wants/bazzite-user-setup.service \
    /usr/lib/systemd/user/bazzite-user-setup.service \
    /usr/libexec/bazzite-user-setup \
    /usr/libexec/bazzite-privileged-user-setup \
    /usr/share/polkit-1/actions/org.bazzite.privileged.user.setup.policy \
    /usr/share/polkit-1/rules.d/20-privileged-ruser.rules; do
    [[ ! -e "${path}" && ! -L "${path}" ]]
done

# Nor is any of it in another folder every account's systemd reads units
# and their links from, which would start it at login, or in the folder
# new accounts are copied from. systemctl's is-enabled would miss a link
# under /usr. The list must name the folder the unit was in, or this
# checks nothing.
unit_paths=$(systemd-analyze --global unit-paths)
grep -qx /usr/lib/systemd/user <<<"${unit_paths}"
mapfile -t unit_folders <<<"${unit_paths}"
for folder in "${unit_folders[@]}" /etc/skel; do
    if [[ -d "${folder}" ]]; then
        [[ -z "$(find "${folder}" -name 'bazzite-user-setup*' -print -quit)" ]]
    fi
done

# And no polkit file gives its action back under another name.
absent grep -rqs org.bazzite.policykit.privileged.user.setup \
    /usr/share/polkit-1 /etc/polkit-1
