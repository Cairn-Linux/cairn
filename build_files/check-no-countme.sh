#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Checks that rpm-ostree's "count me" report stays off (no-countme.sh).
# Runs in its own step of the Containerfile after build.sh, so a base
# update or a later build step that turns it back on stops the build.

set -ouex pipefail

fail() {
    echo "The \"count me\" report could run: $*" >&2
    exit 1
}

# is-enabled exits non-zero for a masked unit, so its answer is read and
# its exit code left aside.
for unit in rpm-ostree-countme.timer rpm-ostree-countme.service; do
    state=$(systemctl --root=/ is-enabled "${unit}" || true)
    if [[ "${state}" != masked ]]; then
        fail "systemctl is-enabled ${unit} says '${state}', not 'masked'."
    fi
done

# The mask itself, in /etc where no-countme.sh made it. is-enabled says
# masked for a mask in any folder systemd reads, so alone it does not show
# that this one is there.
for unit in rpm-ostree-countme.timer rpm-ostree-countme.service; do
    link=/etc/systemd/system/${unit}
    if [[ "$(readlink "${link}" || true)" != /dev/null ]]; then
        fail "${link} is not a link to /dev/null."
    fi
done

# And no link that would start the timer at every start. -L as well as -e,
# because -e follows a link and misses one that leads nowhere.
wants=/etc/systemd/system/timers.target.wants/rpm-ostree-countme.timer
if [[ -e "${wants}" || -L "${wants}" ]]; then
    fail "${wants} is there."
fi

# No repository asks for the count, so neither `rpm-ostree countme` by
# hand nor a metadata download sends it. Any countme line but countme=0
# fails, because dnf reads yes, true and on as well as 1. The folders are
# checked first, since grep finding nothing in a missing folder would pass.
repo_folders=(/usr/share/dnf5/repos.d /etc/yum.repos.d)
for folder in "${repo_folders[@]}"; do
    [[ -d "${folder}" ]] || fail "${folder} is missing, so it was not checked."
done
asking=$(grep -rHE --include='*.repo' '^[[:space:]]*countme[[:space:]]*=' \
    "${repo_folders[@]}" | grep -vE '=[[:space:]]*0[[:space:]]*$' || true)
if [[ -n "${asking}" ]]; then
    fail "a repository asks for it, file and line: ${asking}"
fi
