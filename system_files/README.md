# system_files

Files copied verbatim over `/` in the image by `build_files/build.sh`.
Mirror the target filesystem: `system_files/etc/greetd/config.toml` lands at
`/etc/greetd/config.toml`.

Empty in Phase 0. Phase 1 populates it from `session/` (session entry,
dispatcher, greetd and PAM config) and with the Atkinson Hyperlegible fonts.
