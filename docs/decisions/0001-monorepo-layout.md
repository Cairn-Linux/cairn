# ADR-0001: One repository for the image and its first-party components

**Status:** accepted
**Amended by:** ADR-0010 (the scratchjr-linux repository is not created;
ScratchJr is deferred)
**Date:** 2026-09-03

## Context

Cairn Linux is a bootc image plus a small set of first-party components:
the launcher, the restricted shell, session dispatch, the provisioning
script, and the `kidscan` scanner. It is maintained by one person. The
design doc's sustainability argument (DESIGN §1.5, §4.1) is to inherit
everything from upstream and own only a thin layer.

The Universal Blue `image-template` expects `Containerfile`, `build_files/`,
`system_files/` and `disk_config/` at the repository root so that its
`Justfile` and GitHub workflows work unmodified.

## Decision

A single repository, `Cairn-Linux/cairn`, holds the image and every
first-party component. The image files sit at the root in the exact
`image-template` layout. Components live in top-level directories
(`launcher/`, `shell/`, `session/`, `provision/`, `tools/kidscan/`), each
with its own README, and are packaged as RPMs into the image in Phase 1.

Two things stay out:

- **ScratchJr for Linux** gets its own repository, `Cairn-Linux/scratchjr-linux`.
  It is an upstream-facing contribution meant to outlive and be used outside
  this project.
- **The website** (`cairnlinux.com`) gets its own repository when there is
  something to show.

The published image name is `ghcr.io/cairn-linux/cairn`.

## Consequences

- One issue tracker, one CI, one place to read. Right for a one-maintainer
  project at this stage.
- Components that grow their own contributor base (the shell is the likely
  candidate) can be split out later without changing the image build, since
  they enter the image as RPMs either way.
- The `image-template` `Justfile` and workflows can be dropped in during
  Phase 1 without path surgery.
