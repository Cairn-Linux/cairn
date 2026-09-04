# Contributing

Cairn Linux is in Phase 0: the design is written, the code is not, and the
first job is to put a launcher in front of a real child. Read
[`docs/DESIGN.md`](docs/DESIGN.md) first and [`docs/ROADMAP.md`](docs/ROADMAP.md)
second. The design document is authoritative; propose changes to it as an
ADR in `docs/decisions/` plus the edit, in one pull request.

## Where help is most useful right now

- **ScratchJr on Linux** (ROADMAP §6). Independent of everything else and
  useful far beyond this project.
- **Verifying assumptions** listed as research tasks in the Phase 0
  milestone, especially the Steam containment spike and malcontent on Plasma.
- **Reading the design doc as a parent or teacher** and opening an issue
  where it is unclear, over-promises, or hides a caveat.

Code contributions before the first C++ lands are premature; watch the
launcher issue instead.

## Rules

- **Licence.** Code is Apache-2.0; docs and brand assets are CC BY-SA 4.0.
  By contributing you agree your contribution is under the same terms. The
  project's name and mark are trademarks and are not licensed for use on
  modified builds (see `brand/README.md`).
- **Sign your commits.** Every commit carries a Developer Certificate of
  Origin sign-off, which `git commit -s` adds:

  ```
  Signed-off-by: Your Name <you@example.com>
  ```

  This certifies the text at https://developercertificate.org.
- **Source files start with** `// SPDX-License-Identifier: Apache-2.0`.
- **Conventions** for C++, QML, scripts and docs are in [`CLAUDE.md`](CLAUDE.md).
  They apply to humans too.
- **Voice.** Anything a child or parent will read follows the brand guide's
  voice: calm, real words, no baby talk, limitations stated plainly.

## Nothing phones home

No contribution may add telemetry, analytics, accounts, or network calls the
user did not ask for. This is not negotiable and is not a matter of defaults.
