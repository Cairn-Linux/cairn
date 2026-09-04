# ADR-0003: Hardware floor, presented as Minimum and Recommended

**Status:** accepted
**Date:** 2026-09-03
**Closes:** ROADMAP D11

## Context

DESIGN §7 said two things that did not agree: the target machine is a laptop
from roughly 2013 to 2018, and the minimum is about 2 GB of RAM with Intel
integrated graphics, which describes laptops from roughly 2008 to 2011.

The gap mattered for the launcher toolkit (now moot after ADR-0002), for what
the documentation promises a parent, for the base image's disk footprint
(D4), for whether Plasma at L3/L4 is usable at the floor, and for whether the
bootc installer has to support BIOS-only machines.

Three options were weighed: a very old floor (2 GB, any Intel GPU, BIOS), the
target-era floor (4 GB, Intel HD 4000 or newer, UEFI), and a single 8 GB tier.

## Decision

Two tiers, published the way the back of a PC game box did it, because that
is a format parents already know how to read.

| | Minimum | Recommended |
|---|---|---|
| Laptop age | About 2013 or newer | About 2015 or newer |
| Processor | 64-bit Intel or AMD | Same |
| Memory | 4 GB | 8 GB |
| Graphics | Intel HD 4000 or newer integrated | Discrete, or modern integrated (Iris, Vega) |
| Storage | 64 GB | 128 GB |
| Firmware | UEFI | UEFI |
| Runs | Launcher, shell, educational stack, ScummVM titles, Plasma at L3/L4 | Everything above, plus Steam/Proton titles and Minecraft Java |

Below Minimum the answer is "not this machine", stated plainly. BIOS-only
machines are not supported in v1.

## Consequences

- The child test laptop for Phase 0 is a **Minimum**-tier machine, so the
  floor is what gets tested, not the comfortable case.
- Bazzite's disk footprint is acceptable at a 64 GB floor, which removes one
  objection from D4.
- Qt Quick runs hardware-accelerated on everything at or above the floor; no
  software-rendering fallback is needed in Phase 0.
- The Minimum/Recommended panel is a brand pattern: it appears on the website,
  in the installer, and in the docs in the same form (brand v0.2, issue #20).
- Netbooks and pre-2012 laptops are out. The documentation says so rather
  than letting a parent discover it.
