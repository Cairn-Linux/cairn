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
- `brand/tokens.json` is the **only** place colours, fonts, sizes and radii
  are defined; `brand/build.py` generates the CSS and the QML singleton from
  it. Never hard-code a hex or a font name in UI code; import the tokens.

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

## Language and toolkit (ADR-0002)

All first-party code is **C++20 with Qt 6 and QML**. The maintainer chose this
to learn C++ and most code is AI-written, so the code exists to be read by
someone learning the language from it. That shapes every rule below.

- **QML draws, C++ decides.** Every screen is QML. Every decision (what to
  launch, whether a command is allowed, what level a user is) is a C++ class
  exposed with `QML_ELEMENT`. No business logic in QML JavaScript.
- **Tiny classes, one per `.h`/`.cpp` pair, under ~200 lines.** If a file is
  growing, split it. Names say what a thing is, not how it works.
- **Ownership is always visible.** `QObject`s get a parent. Everything else is
  `std::unique_ptr` or a plain value. No raw `new` without a parent, no
  `delete`, no `std::shared_ptr` without a comment saying why.
- **Boring over clever.** No templates beyond what Qt needs, no macros of our
  own, no operator overloading, no inheritance deeper than `QObject` → ours.
  Use `std::optional` for "maybe"; use `enum class` for kinds; return errors
  as values, never throw across a Qt boundary.
- **Build:** CMake + Ninja, `-Wall -Wextra -Werror` in Debug and Release.
  Debug builds turn on AddressSanitizer and UndefinedBehaviorSanitizer so a
  crash prints where and why. clang-format with the repo's `.clang-format`
  before every commit; clang-tidy clean.
- **Tests from the first commit.** Qt Test for every C++ class with logic.
  The trust boundaries (session dispatch, PAM, PIN overlay, what the shell
  refuses to run) get the most tests and the plainest code in the repo.
- **Dependencies: Qt only.** Anything else needs an ADR.
- **Every user-facing string is translatable from the first commit**
  (ADR-0007): `tr()` in C++, `qsTr()` in QML, `.ts` files in the repo.
  Translation later is a translation job, not a refactor.
- **Every QML control has an `Accessible.name` from the first commit**
  (ADR-0008). It costs nothing now and makes screen-reader support possible
  in Phase 3.
- **Explain the new thing once.** The first time a C++ or Qt concept appears
  in the repo (a signal/slot, a `Q_PROPERTY`, a lambda capture, RAII, a move),
  the commit message or PR body says in one or two sentences what it is and
  why it is used here. Not a tutorial; a signpost. Never in code comments,
  which describe *why this code*, not *what C++ is*.
- **Brand tokens** come from `brand/qml/Cairn/Brand/Tokens.qml` (generated by
  `brand/build.py` from `tokens.json`). Never a literal colour, size or font in
  QML or C++.

## Licence and sign-off (ADR-0005)

- Code is Apache-2.0; docs and brand are CC BY-SA 4.0; the name and mark are
  trademarks (`brand/README.md`). Do not add code under another licence
  without an ADR; record any vendored code in `NOTICE`.
- Every source file starts with an SPDX line: `// SPDX-License-Identifier:
  Apache-2.0` in C++ and QML, `# SPDX-License-Identifier: Apache-2.0` in
  scripts and CMake.
- Every commit carries a Developer Certificate of Origin sign-off. Use
  `git commit -s`; the line is `Signed-off-by: Name <email>`.

## Other conventions

- Commit messages: imperative, one line, optional body. Reference issues.
- Shell scripts: `#!/bin/bash`, `set -ouex pipefail`, shellcheck-clean.
- Python is for `tools/kidscan` and glue only: 3.12+, standard library first.
- Docs: Markdown, wrapped at ~80 columns, one sentence per idea.
- Names of not-yet-decided things (the shell) are proposals until an ADR
  lands. Say "the restricted shell", not a made-up product name.

## Things that are not here

- The only code so far is `tools/kidscan/` and the brand generator
  `brand/build.py`; both are standard-library Python by design. Run the
  tests with `python3 tools/kidscan/tests/test_kidscan.py` and
  `python3 brand/build.py --check`.
- The brand guide's `docs/brand-guide/_ds/` folder is an unrelated, auto-generated
  design system that conflicts with the Cairn palette. It is git-ignored;
  do not read tokens from it.
