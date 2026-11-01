#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Leaves only KDE's own pictures where a grown-up chooses their picture
# (ADR-0043, #166). build.sh runs it, and
# build_files/check-guardian-desktop.sh checks the result.
#
# Bazzite adds Valve's game art to Plasma's picture folder,
# /usr/share/plasma/avatars: links that no package owns, to copies it
# keeps under GNOME's names in /usr/share/pixmaps/faces, and three
# folders of the Steam Deck's look, which steamdeck-kde-presets-desktop
# owns. Family's Your account page, and any other picker that reads the
# folder, would offer them. So this removes every file, link and folder
# there that KDE's plasma-workspace does not own, and every one in
# /usr/share/pixmaps/faces that no package owns.
#
# Usage: kde-pictures.sh [--dry-run] [ROOT]. With --dry-run it only
# prints what it would remove. ROOT is the system to change, / by
# default; rpm is asked about that system's packages.

set -ouex pipefail

dry_run=no
if [[ "${1:-}" == --dry-run ]]; then
    dry_run=yes
    shift
fi
root=${1:-/}
root=${root%/}

avatars=${root}/usr/share/plasma/avatars
faces=${root}/usr/share/pixmaps/faces

# The packages that own a path, one name a line. rpm fails when no
# package owns it, and judges a link by the link, not what it leads to.
owners() {
    rpm --root "${root:-/}" -qf --qf '%{NAME}\n' "${1#"${root}"}"
}

# Whether a path stays: in Plasma's picture folder only what KDE's
# plasma-workspace ships, in GNOME's face folder what any package ships.
stays() {
    local names
    names=$(owners "$1") || return 1
    case "$1" in
    "${avatars}" | "${avatars}"/*) grep -qxF plasma-workspace <<<"${names}" ;;
    *) return 0 ;;
    esac
}

# Removes a path with the command given, rm or rmdir; with --dry-run it
# only says so.
remove() {
    if [[ "${dry_run}" == yes ]]; then
        printf 'Would remove %s\n' "${2#"${root}"}"
    else
        "$1" "$2"
    fi
}

# rpm must answer before anything goes, so a broken package database
# stops the build rather than empty the folder.
stays "${avatars}"

# Files and links first, then folders, deepest first, which are empty by
# then. rmdir refuses a folder that still holds something that stays,
# which stops the build rather than lose a KDE picture.
for top in "${avatars}" "${faces}"; do
    if [[ ! -d "${top}" ]]; then
        continue
    fi
    mapfile -d '' files < <(find "${top}" -mindepth 1 ! -type d -print0)
    mapfile -d '' folders < <(find "${top}" -depth -type d -print0)
    for path in "${files[@]}"; do
        if ! stays "${path}"; then
            remove rm "${path}"
        fi
    done
    for path in "${folders[@]}"; do
        if ! stays "${path}"; then
            remove rmdir "${path}"
        fi
    done
done
