# kidscan

Game library scanner (DESIGN §8.7). Finds Steam roots including the Flatpak
path, parses `libraryfolders.vdf` for secondary drives, reads every
`appmanifest_*.acf`, runs `scummvm --detect` per install directory, and emits
a launcher-agnostic JSON manifest plus optional `.desktop` files. Titles
ScummVM recognises get native launch commands; everything else routes through
Steam. Filters out Proton, Steam Linux Runtimes and Steamworks
Redistributables.

**A prototype exists outside this repository and has not been imported yet.**
Phase 0 task **P0-8** brings it in.

## Not yet built (Phase 3)

- GUI for category assignment — a parent never edits app IDs.
- Icon sourcing (DESIGN §14 Q3).
- Launch-timeout handling lives in the launcher, not here (DESIGN §8.3).
