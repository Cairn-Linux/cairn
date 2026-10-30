# system_files

Files copied verbatim over `/` in the image by `build_files/build.sh`,
all but this README.
Mirror the target filesystem:
`system_files/usr/lib/example.conf` lands at `/usr/lib/example.conf`.

Every file Cairn builds or ships from `session/`, `greeter/` and the other
folders is installed by `cmake --install` in the Containerfile's build
stage, at the place `cmake/CairnInstall.cmake` names (ADR-0038).
Provisioning installs those same files on a machine set up by hand.
Only a file the image needs and provisioning does not belongs here:

- `usr/lib/sysusers.d/cairn.conf`: the level groups and
  `cairn-guardian`, with fixed numbers. Provisioning reads the numbers
  from here too, and a computer with this file runs the image.
- `usr/lib/tmpfiles.d/cairn.conf`: `/var/lib/cairn`, its folders,
  `/var/lib/faillock`, and the folder for the family's menu entries.
- `usr/lib/systemd/system-preset/40-cairn.preset`: SDDM, not Plasma Login
  Manager, after a first boot's presets.
- `etc/xdg/kcm-about-distrorc`: empty, so System Settings' About page
  takes the name, logo and website from `os-release` instead of
  Bazzite's copy of this file.
- `etc/xdg/plasma-workspace/env/cairn-icon-cache.sh`: clears a
  Guardian's Plasma icon cache at login after an update changed the
  image's icons, which Plasma cannot tell on its own.
- `etc/xdg/plasma-workspace/env/cairn-guardian-icons.sh`: puts
  `/usr/share/cairn/guardian-desktop`, Cairn's own icons for the
  Guardian's desktop, first in `XDG_DATA_DIRS` at login, so its
  `start-here` is the Cairn mark (#166). `build_files/guardian-desktop.sh`
  fills the folder.

Packages, fonts among them, are installed by dnf in
`build_files/packages.sh`, not copied here.
