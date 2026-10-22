# disk_config

`bootc-image-builder` configuration for ISOs and disk images. `image-template`
ships `disk.toml`, `iso-gnome.toml` and `iso-kde.toml` here; Cairn will carry
one `iso.toml` (the installer is the Anaconda/KDE flavour) and `disk.toml`
for qcow2 test images. Phase 1 task P1-4. Nothing here yet.

An install-time account must create its Cairn groups first, with their
fixed numbers (`cairn-guardian` 465, `cairn-l1` to `cairn-l4` 461 to 464).
The image only makes them at first boot. See ADR-0038, "A first account".
