#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Layers the packages Cairn needs on the base (ADR-0038). Runs in the
# Containerfile's final stage before Cairn's files are copied in, so an
# RPM never puts its own file over one of Cairn's in /etc. Fedora's sddm
# ships /etc/pam.d/sddm, which Cairn replaces; installed afterwards, rpm
# would keep Cairn's and leave its own beside it as sddm.rpmnew.

set -ouex pipefail

# The same packages provision/cairn-provision.sh layers on a machine it
# sets up by hand. None is in the Bazzite base.
# - labwc: the children's kiosk compositor.
# - scummvm: the point-and-click games.
# - sddm and its two companions: the login screen (ADR-0016). Bazzite
#   ships Plasma Login Manager instead.
# - The brand's typefaces (DESIGN §6.2, brand/tokens.json). Without them
#   every Cairn screen falls back to another font, losing the letterforms
#   early readers need (I/l/1, O/0).
# sddm requires desktop-backgrounds-compat, whose two wallpaper paths the
# base has as links no package owns. Provisioning has to force that
# package over them in an rpm-ostree transaction of its own. dnf in an
# image build replaces a file no package owns without asking, so here it
# comes in with sddm.
dnf5 install -y \
    labwc \
    scummvm \
    sddm \
    sddm-breeze \
    sddm-wayland-plasma \
    atkinson-hyperlegible-next-fonts \
    atkinson-hyperlegible-mono-fonts

# Fedora's trademark guidelines ask a system derived from Fedora to
# remove fedora-logos. generic-logos is Fedora's own replacement with no
# trademarks in it, and it provides the system-logos that Breeze,
# Plymouth, SDDM and KDE's settings require, so nothing is left broken.
# It stays registered in rpm, and name-and-logos.sh writes none of its
# files, so `rpm -V generic-logos` stays clean. The swap leaves Bazzite's
# changed /etc/favicon.png behind as .rpmsave, which nothing reads.
dnf5 swap -y fedora-logos generic-logos
rm /etc/favicon.png.rpmsave

# dnf's lock and the folder sddm's tmpfiles make at install belong to a
# running machine; systemd makes /run afresh at every boot. Some weeks dnf
# also keeps a counter for a repository in /var/lib/dnf/repos, which an
# image may not ship.
rm -r /run/dnf /run/sddm
rm -rf /var/lib/dnf/repos
