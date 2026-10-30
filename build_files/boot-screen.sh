#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Makes Cairn's boot screen the image's own (ADR-0044, #165): the stones
# building from power-on to the login screen, and at shutdown. build.sh
# runs it last, after name-and-logos.sh, with brand/image at /ctx/brand.
# build_files/check-boot-screen.sh checks the result.

set -ouex pipefail

# The theme brand/build.py drew from the tokens.
theme=/usr/share/plymouth/themes/cairn
install -d -m 0755 "${theme}"
install -m 0644 /ctx/brand/plymouth/* "${theme}/"

# Writes /etc/plymouth/plymouthd.conf, which a computer that never
# changed it takes with the next update.
plymouth-set-default-theme cairn

# Plymouth starts from the initramfs, which the base built with its own
# theme, os-release and hostname in it. So it is built again here, for
# the image's one kernel, with the base's own dracut arguments
# (`lsinitrd` prints them). libostree notices the new initramfs and
# writes new boot menu titles from os-release.
kernels=(/usr/lib/modules/*)
[[ ${#kernels[@]} -eq 1 ]]
kernel_version=$(basename "${kernels[0]}")
initramfs=/usr/lib/modules/${kernel_version}/initramfs.img

# The dracut modules in an initramfs, one a line, from what lsinitrd
# prints between "dracut modules:" and its next rule.
dracut_modules() {
    lsinitrd "$1" | awk '/^dracut modules:$/ { listing = 1; next }
        /^=+$/ { listing = 0 } listing' | sort
}
dracut_modules "${initramfs}" >/tmp/base-dracut-modules
[[ -s /tmp/base-dracut-modules ]]

# Plymouth's text in the initramfs uses two fonts: the one fc-match
# names with no family asked for, and the one it names for "monospace",
# as dracut runs (plymouth-populate-initrd). Fontconfig is pointed at the
# brand's two typefaces only while dracut runs, so the password prompt is
# in Atkinson Hyperlegible Next and fixed-width text in Atkinson
# Hyperlegible Mono, without changing the font any other program gets.
# This file reads none of the system's rules, so it says itself which
# family answers "monospace".
cat >/tmp/boot-screen-fonts.conf <<'EOF'
<?xml version="1.0"?>
<!DOCTYPE fontconfig SYSTEM "urn:fontconfig:fonts.dtd">
<fontconfig>
  <dir>/usr/share/fonts/atkinson-hyperlegible-next-fonts</dir>
  <dir>/usr/share/fonts/atkinson-hyperlegible-mono-fonts</dir>
  <alias>
    <family>sans-serif</family>
    <prefer><family>Atkinson Hyperlegible Next</family></prefer>
  </alias>
  <alias>
    <family>monospace</family>
    <prefer><family>Atkinson Hyperlegible Mono</family></prefer>
  </alias>
  <cachedir>/tmp/boot-screen-fontconfig</cachedir>
</fontconfig>
EOF

# /root is a link to /var/roothome, which only a deployed computer has.
# dracut copies the link and its folder into the initramfs, as the base's
# has them, and dracut-install will not copy a link whose target is
# missing ("ERROR: installing '/root'"). So the folder is lent for the
# build and taken away again, since an image may not ship anything in
# /var.
roothome_lent=false
if [[ -L /root && ! -e /var/roothome ]]; then
    install -d -m 0700 /var/roothome
    roothome_lent=true
fi

# DRACUT_NO_XATTR: a rootless build may not copy SELinux labels, and
# dracut counts each file it cannot copy with them as an error. The
# initramfs's cpio archive keeps no labels anyway.
DRACUT_NO_XATTR=1 FONTCONFIG_FILE=/tmp/boot-screen-fonts.conf dracut \
    --no-hostonly \
    --kver "${kernel_version}" \
    --reproducible \
    --zstd \
    --add ostree \
    --add fido2 \
    --tmpdir /tmp \
    --force \
    "${initramfs}"
chmod 0600 "${initramfs}"
if [[ "${roothome_lent}" == true ]]; then
    rmdir /var/roothome
fi

# Every dracut module the base put in its initramfs is in this one too,
# so a disk the base could unlock or mount, this image can.
dracut_modules "${initramfs}" >/tmp/cairn-dracut-modules
missing=$(comm -23 /tmp/base-dracut-modules /tmp/cairn-dracut-modules)
if [[ -n "${missing}" ]]; then
    echo "The rebuilt initramfs lacks dracut modules the base had: ${missing}" >&2
    exit 1
fi
