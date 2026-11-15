#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Checks that a child still cannot start or stop the system updater
# (#265). Runs in its own step of the Containerfile after build.sh, so a
# base update that moves one of the pieces below stops the build until it
# is dealt with.

set -ouex pipefail

levels=10-cairn-levels.rules
rule=/usr/share/polkit-1/rules.d/${levels}
action=org.freedesktop.systemd1.manage-units

# polkit reads the rule files of all these folders in one order, by file
# name, and the first answer wins. Universal Blue's uupd.rules says yes to
# any account for the updater's two services, and Cairn's levels rule
# refuses a child before it, so Cairn's must be read first of all.
folders=()
for folder in /etc/polkit-1/rules.d /run/polkit-1/rules.d \
    /usr/local/share/polkit-1/rules.d /usr/share/polkit-1/rules.d; do
    if [[ -d "${folder}" ]]; then
        folders+=("${folder}")
    fi
done
found=$(find "${folders[@]}" -maxdepth 1 -name '*.rules')
mapfile -t files <<<"${found}"
first=$(basename -a "${files[@]}" | LC_ALL=C sort | sed -n 1p)
[[ "${first}" == "${levels}" ]]

# The units the rule refuses, each in its quotes, read from its own list.
# With no list, grep finds nothing and the build stops here.
grep -qF "var manageUnits = \"${action}\";" "${rule}"
refused=$(sed -n '/^var updaterUnits = \[$/,/^\];$/p' "${rule}" |
    grep -oE '"[^"]+"')

# Each is still a unit the image has. If uupd renamed one, the rule would
# refuse a name nothing uses.
while read -r unit; do
    [[ -e "/usr/lib/systemd/system/${unit//\"/}" ]]
done <<<"${refused}"

# No other rule file answers for a unit the rule does not refuse. One
# that names any of systemd's own actions may put in quotes only the
# action, the detail systemd sends the unit's name in, and units on the
# rule's list. A unit uupd adds to its yes later stops the build here.
allowed=$(printf '"%s"\n"unit"\n%s\n' "${action}" "${refused}")
for file in "${files[@]}"; do
    if [[ "${file##*/}" == "${levels}" ]]; then
        continue
    fi
    if grep -q 'org\.freedesktop\.systemd1' "${file}"; then
        quoted=$(grep -oE "\"[^\"]*\"|'[^']*'" "${file}" | tr "'" '"')
        if grep -vxF "${allowed}" <<<"${quoted}"; then
            echo "${file} names more of systemd's than ${levels} refuses" >&2
            exit 1
        fi
    fi
done
