# CLAUDE.md — working in the Cairn Linux repository

Cairn Linux is a bootc-based Linux system for children aged ~5–8 that grows
with them to ~14. This file tells Claude Code sessions how this repo works.

## Source of truth

- `docs/DESIGN.md` is the **specification**. It is authoritative for goals,
  principles, architecture and the capability ladder. Do not silently diverge
  from it. If a task needs something the design doc doesn't cover, or
  contradicts it, write an ADR in `docs/decisions/` and edit DESIGN.md in the
  same change.
- `docs/ROADMAP.md` is the **plan**: pending decisions (D-numbers), phase
  tasks (P0-n, P1-n …), risks. Keep it current when work lands.
- `brand/tokens.css` / `brand/tokens.json` are the **only** place colours,
  fonts, sizes and radii are defined. Never hard-code a hex or a font name in
  UI code; import the tokens.

## Non-negotiables from the design (read before touching UI or system code)

1. **Nothing phones home.** No telemetry, analytics, accounts, or network
   calls the user didn't ask for. Steam and Minecraft accounts are the
   family's business, not ours.
2. **Inherit from upstream; own only the thin layer.** Prefer a Universal
   Blue / Fedora / Flatpak / malcontent / KDE Kiosk mechanism over anything we
   would have to maintain. Never fork an upstream project into this repo.
3. **Do not reskin third-party apps.** Consistency lives in the frame (login,
   launcher, shell, first-party apps), not in GCompris or Tux Paint.
4. **Real command names, never invented kid-verbs.** `ls`, `cd`, `open`,
   `cat`. Vocabulary must transfer to a real shell later.
5. **Failure is safe and boring.** No stack traces, errnos, "Oops!!", fake
   enthusiasm or exclamation-mark rewards in anything a child can see. Errors
   suggest the next step and never assign blame.
6. **Level is a property of the account**, never a mode toggle. Guardian is a
   role beside the ladder, not L5, and is invisible from child sessions.
7. **x86_64 only.** Don't add ARM paths.
8. **Phase 0 has no ISO.** Don't build CI/image plumbing before the launcher
   has been in front of a real child. See ROADMAP §2.

## Voice for any user-facing string

Calm adult who assumes the child is capable. Short sentences, small
vocabulary, sentence case. Child-facing UI text is 18px minimum. Say
"Something needs a grown-up", not "Error: launch failed (exit 1)". Parent
copy is the same tone, just longer, and states limitations plainly.

## Layout

See the table in `README.md`. Image files (`Containerfile`, `build_files/`,
`system_files/`, `disk_config/`) follow `ublue-os/image-template` exactly so
its Justfile and workflows can be adopted wholesale in Phase 1.

## Conventions

- Commit messages: imperative, one line, optional body. Reference issues.
- Shell scripts: `#!/bin/bash`, `set -ouex pipefail`, shellcheck-clean.
- Python (Phase 0 prototypes): 3.12+, stdlib first, `ruff` for lint.
- Docs: Markdown, wrapped at ~80 columns, one sentence per idea.
- Names of not-yet-decided things (the shell, the greeter) are proposals until
  an ADR lands. Say "the restricted shell", not a made-up product name.

## Things that are not here

- `tools/kidscan/` is the only code so far. It is standard-library-only by
  design; run its tests with `python3 tools/kidscan/tests/test_kidscan.py`.
- The brand guide's `docs/brand-guide/_ds/` folder is an unrelated, auto-generated
  design system that conflicts with the Cairn palette. It is git-ignored;
  do not read tokens from it.
