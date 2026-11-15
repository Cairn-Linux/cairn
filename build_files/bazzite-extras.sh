#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Takes out Bazzite extras that Cairn does not use (ADR-0006, ADR-0038).
# build.sh runs it, and build_files/check-bazzite-extras.sh checks the
# result. The first-boot portal, the updater, Game Mode and Waydroid are
# still P1-1's.
#
# First, Bazzite's first-login step (#261). Every account's own systemd
# started it at login, a child's included. It did its work at the first
# login and again after an update that changed its version or Fedora's
# release. Through pkexec it ran a root step that polkit let any account
# run at any time, without a password: it turned network time on, which
# chronyd already does, and made the account Tailscale's operator. The
# rest of the script is for Bazzite's other images, a Framework laptop's
# start-menu icon and the speakers of two handhelds, so the unit and its
# script go with the link.
#
# No package owns these files. Each goes with a plain rm, so a base update
# that drops or moves one stops the build here until it is looked at.

set -ouex pipefail

# What every account's systemd started at login: the link, the unit and
# its script. By 45.20261112 Bazzite had moved the link here from
# default.target.wants, so the step starts with the graphical session.
rm /etc/systemd/user/graphical-session-pre.target.wants/bazzite-user-setup.service
rm /usr/lib/systemd/user/bazzite-user-setup.service
rm /usr/libexec/bazzite-user-setup

# The root step, the polkit action that let any account run it without a
# password, and Bazzite's rule for that action, which does nothing else.
rm /usr/libexec/bazzite-privileged-user-setup
rm /usr/share/polkit-1/actions/org.bazzite.privileged.user.setup.policy
rm /usr/share/polkit-1/rules.d/20-privileged-ruser.rules
