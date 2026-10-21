# system_files

Files copied verbatim over `/` in the image by `build_files/build.sh`,
all but this README.
Mirror the target filesystem:
`system_files/usr/lib/example.conf` lands at `/usr/lib/example.conf`.

Empty for now.
Every file Cairn builds or ships from `session/`, `greeter/` and the other
folders is installed by `cmake --install` in the Containerfile's build
stage, at the place `cmake/CairnInstall.cmake` names (ADR-0038).
Only a file the image needs that the CMake tree does not install belongs
here.
Fonts are installed by dnf in `build_files/build.sh` (slice 3 of #164),
not copied here.
