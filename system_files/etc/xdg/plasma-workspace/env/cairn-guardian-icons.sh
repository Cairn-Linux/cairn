# SPDX-License-Identifier: Apache-2.0
# shellcheck shell=sh
# Plasma looks for icons in Cairn's folder before the system's, so a
# name only Cairn changes there, such as `start-here`, shows the Cairn
# mark (#166). Every Plasma login sources this: the Guardian's, and an
# L3 or L4 child's, whom cairn-session also sends to Plasma. Both should
# see the mark, so it checks no group. The file the system has under that name
# belongs to a package Cairn does not change (ADR-0038). Cairn's folder
# holds icons and nothing else. Plasma sources this at login.

cairn_icons=/usr/share/cairn/guardian-desktop
case ":${XDG_DATA_DIRS:-}:" in
*":${cairn_icons}:"*) ;;
*)
    XDG_DATA_DIRS="${cairn_icons}:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}"
    export XDG_DATA_DIRS
    ;;
esac
unset cairn_icons
