#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Checks that the image boots with Cairn's boot screen and an initramfs
# that names the computer Cairn (ADR-0044, #165). Runs beside
# check-name-and-logos.sh, after boot-screen.sh has rebuilt the
# initramfs. It stops the build if that initramfs lacks the theme, its
# script module, the text plugin, the brand's fonts or Cairn's
# initrd-release, or if the image has more than one kernel.

set -ouex pipefail

absent() {
    if "$@"; then
        echo "Found what must not be here: $*" >&2
        exit 1
    fi
}

theme=/usr/share/plymouth/themes/cairn

# The theme is the one brand/build.py drew, and the default.
rpm -q plymouth-plugin-script
[[ "$(plymouth-set-default-theme)" == cairn ]]
for file in /ctx/brand/plymouth/*; do
    cmp "${file}" "${theme}/$(basename "${file}")"
done

kernels=(/usr/lib/modules/*)
[[ ${#kernels[@]} -eq 1 ]]
initramfs=${kernels[0]}/initramfs.img
[[ "$(stat -c %a "${initramfs}")" == 600 ]]

lsinitrd "${initramfs}" >/tmp/initramfs-files
in_initramfs() {
    grep -qE "$1" /tmp/initramfs-files
}

# The initramfs starts Plymouth with this theme and its script module.
in_initramfs " usr/share/plymouth/themes/cairn/cairn\.script$"
in_initramfs " usr/share/plymouth/themes/cairn/stone-1\.png$"
in_initramfs " usr/lib64/plymouth/script\.so$"
lsinitrd -f etc/plymouth/plymouthd.conf "${initramfs}" | grep -qx 'Theme=cairn'

# Without the text plugin the password screen shows the box and no words.
in_initramfs " usr/lib64/plymouth/label-freetype\.so$"

# Its fonts are the brand's, so the password prompt is too.
in_initramfs " usr/share/fonts/Plymouth\.ttf -> .*/AtkinsonHyperlegibleNext-Regular\.otf$"
in_initramfs " usr/share/fonts/atkinson-hyperlegible-next-fonts/AtkinsonHyperlegibleNext-Regular\.otf$"
in_initramfs " usr/share/fonts/Plymouth-monospace\.ttf -> .*/AtkinsonHyperlegibleMono-Regular\.otf$"
in_initramfs " usr/share/fonts/atkinson-hyperlegible-mono-fonts/AtkinsonHyperlegibleMono-Regular\.otf$"

# The initramfs's own os-release names the computer before the real root
# is mounted, so it must say Cairn as /usr/lib/os-release does.
lsinitrd -f usr/lib/initrd-release "${initramfs}" >/tmp/initrd-release
grep -qx 'NAME="Cairn Linux"' /tmp/initrd-release
grep -qx 'ID=cairn' /tmp/initrd-release
grep -qx 'DEFAULT_HOSTNAME="cairn"' /tmp/initrd-release
absent grep -qi 'bazzite' /tmp/initrd-release
