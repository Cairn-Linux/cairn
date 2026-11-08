#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Checks that a child still cannot change their own name, picture or
# language (ADR-0056). Runs in its own step of the Containerfile after
# build.sh, so a base update that moves one of the pieces below stops the
# build until it is dealt with.

set -ouex pipefail

# set -e ignores a failure inverted with "!", so a check that something
# is absent goes through this.
absent() {
    if "$@"; then
        echo "Found what must not be here: $*" >&2
        exit 1
    fi
}

action=org.freedesktop.accounts.change-own-user-data

# AccountsService asks this one action before an account changes its own
# name, picture, language and the rest, and Cairn's levels rule refuses it
# to every child. If AccountsService renamed it, the rule would refuse
# nothing.
grep -q "<action id=\"${action}\">" \
    /usr/share/polkit-1/actions/org.freedesktop.accounts.policy
grep -aq "${action}" /usr/libexec/accounts-daemon
grep -q "\"${action}\"" /usr/share/polkit-1/rules.d/10-cairn-levels.rules

# chfn, util-linux's, set-uid root, lets an account change its own full
# name only when CHFN_RESTRICT in login.defs allows it. With no such line
# it refuses every account but root, before it asks for a password. So no
# file chfn could read may set it.
[[ "$(rpm -qf /usr/bin/chfn)" == util-linux-* ]]
for file in /etc/login.defs /usr/etc/login.defs; do
    if [[ -e "${file}" ]]; then
        absent grep -Eq '^[[:space:]]*CHFN_RESTRICT' "${file}"
    fi
done
for folder in /etc/login.defs.d /usr/etc/login.defs.d; do
    if [[ -d "${folder}" ]]; then
        absent grep -rEqs '^[[:space:]]*CHFN_RESTRICT' "${folder}"
    fi
done

# libuser's lchfn changes the same field, and may only while it is not
# set-uid: run as a child, it cannot write the account database.
if [[ -e /usr/bin/lchfn ]]; then
    [[ ! -u /usr/bin/lchfn ]]
fi
