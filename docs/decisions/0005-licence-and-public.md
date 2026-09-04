# ADR-0005: Apache-2.0 for code, CC BY-SA 4.0 for docs and brand, public repository

**Status:** accepted
**Date:** 2026-09-03
**Closes:** ROADMAP D1

## Context

DESIGN §12.3 left the project licence TBD, and the repository was private
because a public repository with no licence is all-rights-reserved by
accident. Three code licences were weighed: Apache-2.0, GPL-3.0-or-later,
and MIT. The values question was whether copyleft's defence against a
proprietary fork mattered more than ecosystem fit and contributor ease.

## Decision

- **Code:** Apache License 2.0 (`LICENSE`). Matches Universal Blue and its
  `image-template`, whose build files this repo adopts; includes a patent
  grant; compatible with Qt's LGPL when dynamically linked, which Fedora
  packaging always is.
- **Documentation and brand assets** (everything under `docs/` and `brand/`
  other than code): Creative Commons Attribution-ShareAlike 4.0
  International (`LICENSE-CC-BY-SA-4.0`).
- **The name "Cairn Linux", the word "cairn" as used for this project, and
  the stacked-stones mark are trademarks of the project**, carved out of both
  licences. Anyone may build and redistribute the code; a modified build may
  not call itself Cairn Linux. Policy in `brand/README.md`. The Linux Mark
  Institute sublicence (DESIGN §12.2) assumes this control of the name.
- **Contributions** are accepted under the Developer Certificate of Origin
  (`Signed-off-by:` on every commit), not a contributor licence agreement.
- **Source files carry an SPDX header**: `SPDX-License-Identifier: Apache-2.0`.
- **The repository is public** from this decision onward.

## Consequences

- Anyone can take the launcher or shell into another project, including a
  proprietary one, without publishing changes. Accepted: the defence of the
  project's values is the trademark on the name, not the code licence.
- Copying from `ublue-os/image-template` (Apache-2.0) needs only attribution
  in `NOTICE`.
- Going public with no installable artefact is deliberate. The README says so
  in its first screen; the design document is the thing worth reading today.
- Fedora, Universal Blue and Bazzite trademark reviews (issue #22) are still
  required before any *image* is published; this ADR covers only the
  repository.
