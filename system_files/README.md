# system_files

Files copied verbatim over `/` in the image by `build_files/build.sh`.
Mirror the target filesystem:
`system_files/usr/share/wayland-sessions/cairn.desktop` lands at
`/usr/share/wayland-sessions/cairn.desktop`.

Empty in Phase 0.
Phase 1 populates it from `session/` (session entry, dispatcher, display
manager and greeter configuration (D6), and PAM config).
Fonts are installed by dnf in `build_files/build.sh`, not copied here:
`atkinson-hyperlegible-next-fonts` and `atkinson-hyperlegible-mono-fonts`.
