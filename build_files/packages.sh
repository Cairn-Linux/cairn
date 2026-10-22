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

# dnf's lock and the folder sddm's tmpfiles make at install belong to a
# running machine; systemd makes /run afresh at every boot.
rm -r /run/dnf /run/sddm
