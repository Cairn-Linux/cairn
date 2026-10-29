#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Set up a family on a computer that runs the Cairn image (ADR-0038, slice
# 4 of #164). The image carries every program and setting; what is left
# names a person or is the family's own choice, so it stays out of the
# image. Run as root from a checkout of the commit the image was built
# from. Nothing needs building:
#
#   sudo provision/cairn-family-setup.sh --guardian NAME --child NAME [--child NAME:2]
#
# It makes the accounts, a "Steam for <child>" menu entry for each child,
# the shared game folder and the full list of its games with their
# pictures, then installs the Flatpaks. It copies no games to any child:
# each child has their own list, which a Guardian chooses on the Games page
# of the Guardian tool, "Family", whose Save copies them (ADR-0041). A new
# child has no games until then. The children's accounts and entries are made by
# the image's own cairn-family, the command the Guardian's tool runs too
# (ADR-0039). Then every child's entry is brought up to date, named on this
# run or not. The other steps are cairn-provision.sh's own, loaded from
# that file, so both kinds of machine check accounts the same way. Only the
# steps in main run here: a new step in cairn-provision.sh never runs on an
# image until it is added below.
set -ouex pipefail

# shellcheck source=provision/cairn-provision.sh
source "$(dirname "${BASH_SOURCE[0]}")/cairn-provision.sh"

# Where the image puts Cairn's files, and the image's own list of the
# groups' numbers.
PREFIX=/usr
SHARE=/usr/share/cairn
GROUP_NUMBERS="$IMAGE_MARK"
# Makes each child's account and menu entry (ADR-0039).
FAMILY_COMMAND="$PREFIX/libexec/cairn/cairn-family"
# make_game_tiles makes only the full list here. Each child's tiles come
# from their own list, which stays as a Guardian saved it (ADR-0041).
TILES_NOT_FOLLOWED="so the list of shared games was not made again; the Games page in Family shows the one from the last run, and each child keeps the games on their own list."
TILES_NOT_MADE="the list of shared games could not be made again; the Games page in Family shows the one from the last run, and each child keeps the games on their own list."

check_image() {
    [ -f "$IMAGE_MARK" ] ||
        refuse "this computer does not run the Cairn image; use provision/cairn-provision.sh instead"
}

# The steps below read what the image's helpers print, word by word. A
# checkout of another commit may expect other words, and then drops lines
# without a word, so each helper must be the checkout's own. This script no
# longer runs the copy helpers, but cairn-family save-games does, so a
# difference in them still means the checkout is not the image's. The build
# changes /usr/local to /usr in each file it installs (cmake/CairnInstall.cmake).
#
# cairn-family is compiled, so it cannot be compared with the checkout. It
# must be there; an image built before it is refused. Its answer is one
# line, done or refused and a word, and make_children stops on anything
# but done and prints the line as it came. So an image from another commit
# that answers with a word this checkout does not know still stops the run
# and says what the command said; it is never taken for done.
check_checkout() {
    [ -x "$FAMILY_COMMAND" ] ||
        refuse "this image has no $FAMILY_COMMAND; check out the commit the image was built from"
    local source installed
    for source in provision/cairn-copy-games.py:cairn-copy-games \
        provision/cairn-game-pictures.py:cairn-game-pictures \
        tools/kidscan/kidscan.py:kidscan.py \
        session/bin/cairn-steam-visit:cairn-steam-visit; do
        installed="$PREFIX/libexec/cairn/${source#*:}"
        if ! sed 's|/usr/local/|/usr/|g' "$REPO/${source%%:*}" | cmp -s - "$installed"; then
            refuse "this checkout does not match the image: ${source%%:*} differs from $installed; check out the commit the image was built from"
        fi
    done
}

# The image gives the Guardian's window cairn-guardian's fixed number. A
# computer that made the group with another number must renumber it first,
# with no Guardian logged in (provision/cairn-renumber-guardians.sh). The
# level groups may keep any number, because everything reads them by name.
check_guardian_number() {
    local current wanted
    current="$(getent group cairn-guardian | cut -d: -f3)" || return 0
    wanted="$(fixed_gid cairn-guardian)"
    [ "$current" = "$wanted" ] ||
        refuse "cairn-guardian is number $current here and the image needs $wanted, so no Guardian could open Steam for a child; run provision/cairn-renumber-guardians.sh first"
}

# systemd-sysusers makes the groups at first boot. One that is missing now
# is made with its fixed number, and groupadd stops the run if another
# group has it.
ensure_image_groups() {
    local group
    for group in "${LEVEL_GROUPS[@]}"; do
        if ! getent group "$group" > /dev/null; then
            groupadd --system --gid "$(fixed_gid "$group")" "$group"
            changed "group $group"
        fi
    done
}

# cairn-family refuses an account Cairn did not make (ADR-0039), so this is
# checked before anything changes: each child named that is already there
# must be in a level group. check_accounts has refused administrators.
# Giving a level to an account made some other way waits for #113.
check_children_are_cairns() {
    local child level found
    for child in "${children[@]}"; do
        id "$child" > /dev/null 2>&1 || continue
        found=no
        for level in cairn-l1 cairn-l2 cairn-l3 cairn-l4; do
            if in_group "$child" "$level"; then
                found=yes
            fi
        done
        [ "$found" = yes ] ||
            refuse "$child is an account Cairn did not make; name a new account for each child"
    done
}

# What a run of cairn-family can change about one child, so a run that
# changed nothing is not counted.
child_state() {
    getent passwd "$1" || true
    id -nG "$1" 2> /dev/null || true
    passwd --status "$1" 2> /dev/null | cut -d' ' -f2 || true
    cat "$APPS/cairn-steam-for-$1.desktop" 2> /dev/null || true
}

# Each child's account, level and "Steam for <child>" entry, from the same
# command a Guardian's window runs. The command answers in one line.
make_children() {
    local i child answer before
    for i in "${!children[@]}"; do
        child="${children[$i]}"
        before="$(child_state "$child")"
        answer="$("$FAMILY_COMMAND" create-child "$child" "${levels[$i]}" < /dev/null)" || true
        [ "$answer" = "done" ] ||
            refuse "$child's account was not made or changed; cairn-family said: ${answer:-nothing} (journalctl -t cairn-family)"
        if [ "$(child_state "$child")" != "$before" ]; then
            changed "child $child at level ${levels[$i]}"
        fi
    done
}

relabel_entries() {
    if [ -d "$APPS" ] && command -v restorecon > /dev/null; then
        restorecon -R "$APPS"
    fi
}

main() {
    parse_arguments "$@"
    check_image
    [ "$(id -u)" -eq 0 ] || { echo "run as root" >&2; exit 1; }
    check_accounts
    check_children_are_cairns
    check_checkout
    check_guardian_number

    ensure_image_groups
    ensure_guardian
    make_children
    # Every child's entry, not only those named on this run: one left off
    # the command line, or one made by hand with /usr/local paths, gets the
    # image's entry too. It writes the bytes cairn-family writes, so the
    # entries of the children named are left as they are. It also removes
    # the entry of an account that is no longer a child.
    make_steam_entries
    make_shared_library
    relabel_entries
    # The full list of shared games and their pictures, so the Games page
    # shows every game. No copy_games: each child gets the games on their
    # own list when a Guardian saves it (ADR-0041).
    make_game_tiles
    # Last, because it needs the internet: an offline computer still gets
    # everything above, and the run then stops with flatpak's own message.
    install_flatpaks

    set +x
    echo "note: no child has games until a grown-up chooses them: open Family from the menu, then its Games page, and press Save."
    echo "done: $changes change(s)"
}

# Run when executed; only load the steps when a test sources the file.
if [ "${BASH_SOURCE[0]}" = "$0" ]; then
    main "$@"
fi
