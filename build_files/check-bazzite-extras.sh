#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Checks that the Bazzite extras bazzite-extras.sh takes out stay out
# (ADR-0038). Runs in its own step of the Containerfile after build.sh, so
# a base update that brings one back stops the build until it is dealt
# with.

set -ouex pipefail

# set -x alone shows only the test that failed, so each failure also
# says in words what was found.
fail() {
    echo "$*" >&2
    exit 1
}

# set -e ignores a failure inverted with "!", so a check that something
# is absent goes through this.
absent() {
    if "$@"; then
        fail "Found what must not be here: $*"
    fi
}

# Bazzite's first-login step (#261): none of its files is left. -L as
# well as -e, because -e follows a link and misses one whose unit has
# gone. The link's old place stays on the list, because a base that put
# it back there would bring the step back.
for path in \
    /etc/systemd/user/default.target.wants/bazzite-user-setup.service \
    /etc/systemd/user/graphical-session-pre.target.wants/bazzite-user-setup.service \
    /usr/lib/systemd/user/bazzite-user-setup.service \
    /usr/libexec/bazzite-user-setup \
    /usr/libexec/bazzite-privileged-user-setup \
    /usr/share/polkit-1/actions/org.bazzite.privileged.user.setup.policy \
    /usr/share/polkit-1/rules.d/20-privileged-ruser.rules; do
    [[ ! -e "${path}" && ! -L "${path}" ]] ||
        fail "Found what must not be here: ${path}"
done

# Nor is any of it in another folder every account's systemd reads units
# and their links from, which would start it at login, or in the folder
# new accounts are copied from. find looks in every *.wants folder below
# each one, so a link Bazzite moves again is found even if the list above
# misses it. systemctl's is-enabled would miss a link under /usr.
# systemd's list of folders must name the ones the link and the unit
# were in, or this checks nothing.
unit_paths=$(systemd-analyze --global unit-paths)
for folder in /etc/systemd/user /usr/lib/systemd/user; do
    grep -qx "${folder}" <<<"${unit_paths}" ||
        fail "systemd no longer reads user units from ${folder}, so this check would not look there"
done
mapfile -t unit_folders <<<"${unit_paths}"
for folder in "${unit_folders[@]}" /etc/skel; do
    if [[ -d "${folder}" ]]; then
        found=$(find "${folder}" -name 'bazzite-user-setup*' -print -quit)
        [[ -z "${found}" ]] || fail "Found what must not be here: ${found}"
    fi
done

# And no polkit file gives its action back under another name.
absent grep -rqs org.bazzite.policykit.privileged.user.setup \
    /usr/share/polkit-1 /etc/polkit-1
