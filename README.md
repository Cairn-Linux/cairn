<p align="left">
  <img src="brand/mark.svg" alt="Cairn mark" width="72" height="66">
</p>

# cairn

**A computer that grows with your child.**

Cairn Linux is an early Linux project for children aged roughly 5–8, designed
to remain useful into their early teens. It starts with a simple home screen,
apps chosen by an adult, and a small terminal with real command names.
As a child becomes more confident, the plan is to introduce their own files
and eventually a regular desktop on the same machine.

The target is an older family laptop, not a new device bought just for Cairn.
Each child has their own account. An adult chooses their level of access and
administers the computer locally through a separate Guardian role.

A cairn is a small stack of stones left by someone who walked the trail
first — a marker that helps whoever comes next without walking the path for
them. That is the design brief.

> **Status: pre-alpha, Phase 0.** There is a working prototype, but no public
> installer or release ready for everyday family use. The launcher, Footpath
> terminal integration, game scanner and session plumbing have code and
> recorded tests. Permission hardening, recovery paths and remaining Steam
> checks still need work before the real-hardware child trial.
> See the [roadmap](docs/ROADMAP.md) for current progress and the
> [design specification](docs/DESIGN.md) for the intended experience.

## Help shape the experience

We want children to make things, explore, and learn how a computer works
without having to understand an adult desktop on day one. Mistakes should
lead to useful suggestions, and finding the way home should be obvious.

We welcome design help with both the public website and Cairn's interface.
AI-assisted work has helped build the prototype; we want human designers to
question the flows and visual choices, not just polish them. The current
brand is a starting point for proposals, not a request to preserve every
choice. Product constraints remain in the design specification.

The website lives separately in
[`cairn-site`](https://github.com/Cairn-Linux/cairn-site).
Cairn is not a replacement for adult supervision, and its prototype
restrictions are still being tested.

## What makes it different

The design follows these principles; not every planned feature is complete.

- **Tools, not toys.** The emphasis is on making things. Adults choose apps
  for a flat launcher grid rather than receiving a large bundled collection.
- **The machine is legible.** Real command names, deterministic behaviour,
  calm failure. No mascots, no gamification, no baby talk.
- **Capability grows, the system doesn't change.** Level is a property of the
  child's account, not a mode. One visual identity from 5 to 14.
- **Bring your own games.** Point-and-click classics you already own on Steam
  become launcher tiles, run natively through ScummVM, with no store in sight.
- **Nothing phones home.** Cairn's design excludes project accounts,
  telemetry and analytics. Optional services such as Steam have their own
  accounts and network requirements.
- **Built to outlive its maintainer.** A bootc image on top of a Universal
  Blue base, not a fork. Upstream keeps shipping updates whether or not this
  project is active that month.

## Repository layout

| Path | What lives here |
|---|---|
| `docs/DESIGN.md` | The specification. Authoritative for *what* and *why*. |
| `docs/ROADMAP.md` | The plan: phases, decisions pending, tasks, risks. |
| `docs/DEVELOPMENT.md` | The development setup, checked against the real dev PC. |
| `docs/decisions/` | Architecture decision records (ADRs) for choices made after the design doc. |
| `docs/research/` | Research notes, measurements and child-test observations. |
| `docs/brand-guide/` | Brand guidelines v0.1 as a design canvas (`.dc.html`). |
| `brand/` | Brand as code: colour/type tokens (`tokens.css`, `tokens.json`) and the mark as SVG. |
| `Containerfile`, `build_files/`, `system_files/`, `disk_config/` | The bootc image, following the Universal Blue `image-template` layout. Not built until Phase 1. |
| `cairn.env` | Universal Blue image-template variables; Phase 1, see issue #37. |
| `CMakeLists.txt`, `CMakePresets.json` | The C++ build, first landed 2026-09-04. |
| `launcher/` | The fullscreen tile launcher — the product. |
| `external/footpath/` | Footpath, the child-facing terminal, a submodule pinned to a tag (ADR-0014; its own repository is `Cairn-Linux/footpath`). |
| `session/` | Login-to-session plumbing: one session entry that dispatches by account level. |
| `provision/` | Phase 0 script that turns a stock install into a Cairn machine, for testing before any ISO exists. |
| `tools/kidscan/` | Game library scanner: Steam + ScummVM detection → launcher manifest. |
| `NOTICE` | Upstream attribution and notices. |

## Hardware targets

These are design targets, not a tested compatibility list. Older-laptop
validation remains part of Phase 0.

x86_64 only. ARM and Raspberry Pi are out of scope for v1.

| | Minimum | Recommended |
|---|---|---|
| Laptop age | About 2013 or newer | About 2015 or newer |
| Processor | 64-bit Intel or AMD | Same |
| Memory | 4 GB | 8 GB |
| Graphics | Intel HD 4000 or newer integrated | Discrete, or modern integrated (Iris, Vega) |
| Storage | 64 GB | 128 GB |
| Firmware | UEFI | UEFI |
| Runs | Launcher, shell, educational stack, ScummVM titles, Plasma at L3/L4 | Everything above, plus Steam/Proton titles and Minecraft Java |

Nvidia graphics are unsupported in v1 (ADR-0006), so "Discrete" in the
Recommended column means AMD or Intel.

Below Minimum the honest answer is "not this machine".

## Contributing

See [`CONTRIBUTING.md`](CONTRIBUTING.md). Commits are signed off under the
Developer Certificate of Origin.

## Licence

Code is licensed under the [Apache License 2.0](LICENSE). Documentation and
brand assets are licensed under
[CC BY-SA 4.0](LICENSE-CC-BY-SA-4.0). The name "Cairn Linux" and the
stacked-stones mark are trademarks of the project and are not covered by
either licence; a modified build may not call itself Cairn Linux. See
`brand/README.md`.

---

Cairn Linux · cairnlinux.com · © 2026 Cairn Linux contributors.
"Linux" is a registered trademark of Linus Torvalds; sublicence via the Linux
Mark Institute is pending.
