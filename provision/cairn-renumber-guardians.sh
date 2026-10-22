#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Give cairn-guardian the number the Cairn image needs (ADR-0038, slice 4
# of #164). The image's build gives the Guardian's window that number,
# 465, so on a computer whose group has another one no Guardian can open
# "Steam for <child>". Run it once, as root, on a computer set up by hand
# before it moves to the image, from a text console (Ctrl+Alt+F3) or over
# ssh. A Guardian's own console or ssh login is fine; only their desktop
# must be logged out:
#
#   sudo provision/cairn-renumber-guardians.sh
#
# Done before the move, the new number is in the /etc that bootc carries
# into the image, and in the one a rollback would boot. It also works on a
# computer that moved without it, but there the old deployment keeps the
# old number.
#
# Files first, then the group: a file's group is only a number, so it can
# move before /etc/group says so. The files are walked again after the
# group moves, so a file made meanwhile is not left behind. A run cut off
# before the group moves starts again from the start. One cut off after it
# says so, and is run again with the old number, which walks the files
# once more:
#
#   sudo provision/cairn-renumber-guardians.sh OLD_NUMBER
#
# Only cairn-guardian: the level groups are read by name everywhere, so
# they may keep the numbers they have.
set -ouex pipefail

# shellcheck source=provision/cairn-provision.sh
source "$(dirname "${BASH_SOURCE[0]}")/cairn-provision.sh"

# Where a file Cairn or its family made can carry the group: the shared
# game folder and its ACLs, the hand-provisioned programs under
# /var/usrlocal, the homes and the configuration. Not the rest of /var,
# whose container and Flatpak stores keep numbers of their own.
REOWN_PLACES=(/etc /var/lib/cairn /var/usrlocal /var/home /var/tmp)

# The image's own list when this computer runs it, otherwise the
# checkout's copy of it.
choose_group_numbers() {
    if [ -f "$IMAGE_MARK" ]; then
        GROUP_NUMBERS="$IMAGE_MARK"
    fi
}

# The places that exist here, one per line.
reown_places() {
    local place
    for place in "${REOWN_PLACES[@]}"; do
        [ ! -e "$place" ] || echo "$place"
    done
}

# True when the member has a desktop open: a session of Type x11, wayland
# or mir that is not closing. A text console or ssh login is Type tty, and
# the one running this script is usually one of those, so it does not
# count. Seat would not tell them apart: a Ctrl+Alt+F3 login is on seat0.
on_the_screen() {
    local session
    for session in $(loginctl show-user "$1" --property=Sessions --value 2> /dev/null); do
        [ "$(loginctl show-session "$session" --property=State --value 2> /dev/null)" != closing ] ||
            continue
        case "$(loginctl show-session "$session" --property=Type --value 2> /dev/null)" in
            x11 | wayland | mir) return 0 ;;
        esac
    done
    return 1
}

# A Guardian who is logged in keeps the old number until they log in again,
# so their Steam would lose the shared game folder in the middle of a
# download, and anything it made meanwhile would keep the old number.
guardians_are_out() {
    local member
    for member in $(getent group cairn-guardian | cut -d: -f4 | tr ',' ' '); do
        if on_the_screen "$member"; then
            refuse "$member is logged in on the screen; log out of the desktop, then run this from a text console (Ctrl+Alt+F3) or over ssh, where their own login is fine"
        fi
        if pgrep -u "$member" steam > /dev/null; then
            refuse "Steam is open for $member; close it, then run this again"
        fi
    done
}

# Moves every file in the places from one group number to the other. -xdev
# keeps each walk on its own filesystem, and chgrp -h changes a link itself,
# never what it leads to. A file can vanish during the walk, in /var/tmp or
# a home in use; that one needs no move, so neither find nor chgrp may stop
# the run over it. left_behind names anything a walk missed.
reown_files() {
    local places
    mapfile -t places < <(reown_places)
    find "${places[@]}" -xdev -ignore_readdir_race -gid "$1" -print0 |
        xargs -0r chgrp -h -- "$2" || true
}

# An ACL names a group by its number. getfacl's dump with every entry for
# the old number given the new one; the header names the file's own group,
# which reown_files has moved already.
acl_moved() {
    sed -E "s/^(default:)?group:$1:/\\1group:$2:/"
}

move_acls() {
    local places
    mapfile -t places < <(reown_places)
    { getfacl --recursive --physical --absolute-names --numeric --skip-base "${places[@]}" 2> /dev/null || true; } |
        acl_moved "$1" "$2" | { setfacl -P --restore=- || true; }
}

# The first file still carrying the old number, by group or in an ACL.
# awk reads getfacl's dump to the end, because getfacl would fail on a
# closed pipe, and pipefail would stop the run.
left_behind() {
    local places
    mapfile -t places < <(reown_places)
    {
        find "${places[@]}" -xdev -ignore_readdir_race -gid "$1" -print || true
        { getfacl --recursive --physical --absolute-names --numeric --skip-base "${places[@]}" 2> /dev/null || true; } |
            awk -v old="$1" '/^# file: / { file = substr($0, 9) }
                $0 ~ "^(default:)?group:" old ":" { print file }'
    } | awk 'NR == 1'
}

# Moves the files from the old number, then names the first one left.
move_files() {
    reown_files "$1" "$2"
    move_acls "$1" "$2"
}

report() {
    local left
    left="$(left_behind "$1")"
    set +x
    if [ -n "$left" ]; then
        echo "note: $left still has the old number $1; set its group by hand: chgrp $2, or setfacl for an ACL"
    fi
    echo "done: $changes change(s)"
    echo "log in again: anyone logged in now keeps the old number until then"
}

# Printed when a run stops after the group moved, because a second run
# without the old number would find 465 and do nothing.
stopped_after_groupmod() {
    echo "stopped after cairn-guardian moved; run again with: $0 $1" >&2
}

main() {
    [ $# -le 1 ] || { echo "usage: $0 [OLD_NUMBER]" >&2; exit 2; }
    [ "$(id -u)" -eq 0 ] || { echo "run as root" >&2; exit 1; }
    choose_group_numbers
    local current wanted other old="${1:-}"
    current="$(getent group cairn-guardian | cut -d: -f3)" ||
        refuse "there is no cairn-guardian group here; nothing to renumber"
    wanted="$(fixed_gid cairn-guardian)"
    if [ -n "$old" ]; then
        [[ "$old" =~ ^[0-9]+$ ]] && [ "$old" != "$wanted" ] ||
            refuse "$old is not an old number to move from; nothing was changed"
        [ "$current" = "$wanted" ] ||
            refuse "cairn-guardian is still number $current; run this without a number first"
        if other="$(getent group "$old")"; then
            refuse "number $old belongs to the group ${other%%:*} here, so its files are not cairn-guardian's; nothing was changed"
        fi
        guardians_are_out
        move_files "$old" "$wanted"
        report "$old" "$wanted"
        return 0
    fi
    if [ "$current" = "$wanted" ]; then
        set +x
        echo "cairn-guardian is number $wanted already"
        echo "done: 0 change(s)"
        return 0
    fi
    if other="$(getent group "$wanted")"; then
        refuse "number $wanted belongs to the group ${other%%:*} here, and the image needs it for cairn-guardian; nothing was changed"
    fi
    guardians_are_out

    echo "note: this looks at every file in the homes and the shared game folder, which can take some minutes"
    move_files "$current" "$wanted"
    groupmod --gid "$wanted" cairn-guardian
    RERUN_FROM="$current"
    trap 'stopped_after_groupmod "$RERUN_FROM"' EXIT
    changed "cairn-guardian is number $wanted now (was $current)"
    move_files "$current" "$wanted"
    report "$current" "$wanted"
    trap - EXIT
}

# Run when executed; only load the steps when a test sources the file.
if [ "${BASH_SOURCE[0]}" = "$0" ]; then
    main "$@"
fi
