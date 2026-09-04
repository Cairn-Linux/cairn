# ADR-0006: Base image is Bazzite (KDE), one Intel/AMD image for v1

**Status:** accepted
**Date:** 2026-09-03
**Closes:** ROADMAP D4

## Context

DESIGN §4.1 decided on a bootc image built the Universal Blue way but did not
name the base. Three candidates were weighed: Bazzite (KDE variant),
Universal Blue's `kinoite-main`, and plain Fedora bootc.

ADR-0004 made Steam at every level a Phase 1 deliverable, which is what
Bazzite ships and maintains. ADR-0003 set a 64 GB storage floor, which
Bazzite's footprint fits. A policy review (`docs/research/base-image-policy.md`)
found no licence or trademark blocker: Bazzite and Universal Blue are
Apache-2.0, derivatives through `image-template` are their documented and
encouraged path, and the only rules concern their names and marks.

## Decision

- **Base:** `ghcr.io/ublue-os/bazzite:stable`, the KDE desktop variant.
  Pinned to a digest once CI exists (P1-3), renewed by the daily build.
- **One image for v1, Intel and AMD graphics only.** Bazzite's Nvidia
  variants are not built. Nvidia laptops are listed as unsupported in the
  hardware panel until a later release, rather than doubling the build and
  test matrix now.
- **Phase 0 test laptop runs stock Bazzite KDE**, so the provisioning script
  targets the same base the image will use.
- **Compliance is Phase 1 work (P1-14)** and blocks publishing any image:
  rewrite `os-release`, swap `fedora-logos` for `cairn-logos`, replace
  `fedora-release-notes`, display the Fedora "not provided or supported"
  notice, ship our own installer artwork, attribute Bazzite and Universal
  Blue in `NOTICE`.
- **Lineage is stated in prose only:** "Cairn Linux is built on Bazzite, a
  Universal Blue image derived from Fedora." Never their names or marks in
  Cairn's identity, never their support channels in Cairn's UI or docs.
- **Fallback** if Bazzite ever becomes unworkable: `kinoite-main` with Steam
  layered, which shares the Plasma base and most of the Containerfile.

## Consequences

- Steam, Proton, gamescope, controller and GPU enablement, and Plasma arrive
  maintained by a large project; Cairn owns only its thin layer (DESIGN §1.5).
- Bazzite's own first-boot portal, Steam Game Mode session, Waydroid and
  gaming extras must be disabled or removed in the Containerfile (Phase 1).
  Bazzite's docs list "swap out pre-installed system-level applications" as a
  supported reason to derive.
- Disk footprint is the price; acceptable at the 64 GB floor.
- Nvidia laptop owners are turned away plainly by the hardware panel in v1.
