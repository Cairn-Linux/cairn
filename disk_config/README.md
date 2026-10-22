# disk_config

`bootc-image-builder` settings for ISOs and disk images. `image-template`
ships `disk.toml`, `iso-gnome.toml` and `iso-kde.toml` here. Cairn carries
one `iso.toml`, the Anaconda installer (ADR-0038, slice 6). A `disk.toml`
for qcow2 test images can follow if the VM needs one. Phase 1 task P1-4.

Build the installer with `sudo tools/build-iso.sh`; `docs/DEVELOPMENT.md`,
"Building the installer", has the steps.

`iso.toml` makes an installer that asks before it touches a disk and makes
no account. Its kickstart must stay: without one, bootc-image-builder
writes its own, which wipes every disk without asking. The first account
comes from the first-boot wizard (ADR-0020, P1-6).

A test ISO for the VM uses a private config, kept on the dev PC outside the
checkout and never committed. An install-time account must create its
Cairn groups first, with their fixed numbers (`cairn-guardian` 465,
`cairn-l1` to `cairn-l4` 461 to 464). The image only makes them at first
boot. bootc-image-builder refuses `[[customizations.user]]` and
`[[customizations.group]]` next to a kickstart, so the groups and the
account are kickstart lines. See ADR-0038, "A first account".
