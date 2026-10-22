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
# the children's game tiles and their own copies of the games, then
# installs the Flatpaks. Each step is cairn-provision.sh's own, loaded from
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

check_image() {
    [ -f "$IMAGE_MARK" ] ||
        refuse "this computer does not run the Cairn image; use provision/cairn-provision.sh instead"
}

# The steps below read what the image's helpers print, word by word. A
# checkout of another commit may expect other words, and then drops lines
# without a word, so each helper must be the checkout's own. The build
# changes /usr/local to /usr in each file it installs (cmake/CairnInstall.cmake).
check_checkout() {
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
    check_checkout
    check_guardian_number

    ensure_image_groups
    ensure_guardian
    ensure_children
    make_steam_entries
    make_shared_library
    relabel_entries
    make_game_tiles
    copy_games
    # Last, because it needs the internet: an offline computer still gets
    # everything above, and the run then stops with flatpak's own message.
    install_flatpaks

    set +x
    echo "done: $changes change(s)"
}

# Run when executed; only load the steps when a test sources the file.
if [ "${BASH_SOURCE[0]}" = "$0" ]; then
    main "$@"
fi
