<p align="left">
  <img src="brand/mark.svg" alt="Cairn mark" width="72" height="66">
</p>

# cairn

**A computer that grows with your child.**

Cairn Linux is a Linux system for children aged roughly 5–8 that keeps
working for them through age ~14. It runs on the 2013–2018 laptop most
households already have. A five-year-old gets a fullscreen launcher and a
five-word terminal; a twelve-year-old gets a real, locked-down desktop on the
same machine with the same visual language. Adults administer it locally,
from a Guardian role that children never see.

A cairn is a small stack of stones left by someone who walked the trail
first — a marker that helps whoever comes next without walking the path for
them. That is the design brief.

> **Status: pre-alpha, Phase 0.** Nothing is installable yet. The design is
> written; the code is not. See [`docs/ROADMAP.md`](docs/ROADMAP.md) for what
> happens next and [`docs/DESIGN.md`](docs/DESIGN.md) for why.

## What makes it different

- **Tools, not toys.** The front door is instruments — draw, music, build,
  write, terminal. Drills live in a folder.
- **The machine is legible.** Real command names, deterministic behaviour,
  calm failure. No mascots, no gamification, no baby talk.
- **Capability grows, the system doesn't change.** Level is a property of the
  child's account, not a mode. One visual identity from 5 to 14.
- **Bring your own games.** Point-and-click classics you already own on Steam
  become launcher tiles, run natively through ScummVM, with no store in sight.
- **Nothing phones home.** No telemetry, no accounts, no analytics.
- **Built to outlive its maintainer.** A bootc image on top of a Universal
  Blue base, not a fork. Upstream keeps shipping updates whether or not this
  project is active that month.

## Repository layout

| Path | What lives here |
|---|---|
| `docs/DESIGN.md` | The specification. Authoritative for *what* and *why*. |
| `docs/ROADMAP.md` | The plan: phases, decisions pending, tasks, risks. |
| `docs/decisions/` | Architecture decision records (ADRs) for choices made after the design doc. |
| `docs/brand-guide/` | Brand guidelines v0.1 as a design canvas (`.dc.html`). |
| `brand/` | Brand as code: colour/type tokens (`tokens.css`, `tokens.json`) and the mark as SVG. |
| `Containerfile`, `build_files/`, `system_files/`, `disk_config/` | The bootc image, following the Universal Blue `image-template` layout. Not built until Phase 1. |
| `launcher/` | The fullscreen tile launcher — the product. |
| `shell/` | The restricted, child-facing terminal. |
| `session/` | Login-to-session plumbing: one session entry that dispatches by account level. |
| `provision/` | Phase 0 script that turns a stock install into a Cairn machine, for testing before any ISO exists. |
| `tools/kidscan/` | Game library scanner: Steam + ScummVM detection → launcher manifest. |

## Hardware

x86_64 only. ARM and Raspberry Pi are out of scope for v1.

| | Minimum | Recommended |
|---|---|---|
| Laptop age | About 2013 or newer | About 2015 or newer |
| Memory | 4 GB | 8 GB |
| Graphics | Intel HD 4000 or newer | Discrete, or modern integrated |
| Storage | 64 GB | 128 GB |
| Firmware | UEFI | UEFI |
| Runs | Launcher, shell, educational apps, ScummVM titles, the L3/L4 desktop | Everything, plus Steam/Proton titles and Minecraft Java |

Below Minimum the honest answer is "not this machine".

## Contributing

See [`CONTRIBUTING.md`](CONTRIBUTING.md). The highest-value independent
workstream is ScratchJr on Linux (`docs/ROADMAP.md` §6). Commits are signed
off under the Developer Certificate of Origin.

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
