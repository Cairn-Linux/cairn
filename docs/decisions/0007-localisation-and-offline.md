# ADR-0007: English-only v1 with externalised strings; offline-capable, not offline-first

**Status:** accepted
**Date:** 2026-09-03
**Closes:** ROADMAP D8 (DESIGN §14 Q4, Q5)

## Context

DESIGN §14 left two stances undecided: whether to adopt Endless-style
offline-first architecture, and the localisation scope for v1. Both change
the software catalogue, storage budget and string handling if left to
default.

## Decision

- **Localisation:** v1 ships English only. Every user-facing string in
  first-party code is externalised from the first commit (Qt's `tr()` and
  `qsTr()`, `.ts` files in the repo) so that translation later is a
  translation job, not a refactor. GCompris and KDE arrive already
  translated; the system language setting is honoured for them even in v1.
- **Offline:** offline-capable, not offline-first. Everything a child does
  works with no network. The things that need a network are OS and Flatpak
  updates and the Steam and Minecraft integrations, and each fails calmly
  when offline ("Something needs a grown-up" or a quiet retry later), never
  with a blocking dialog in a child's session. No offline software
  catalogue, no content mirroring.

## Consequences

- The reading-level constraint that runs through the design is tested in one
  language first; vocabulary and error voice findings from the child test
  transfer to translations later because the strings are already isolated.
- A machine with no Wi-Fi is a fully working Cairn machine for a child,
  which suits the "old laptop in a cupboard" story.
- The first-boot wizard's Wi-Fi step (P1-6) is skippable.
