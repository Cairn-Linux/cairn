# ADR-0002: The first-party layer is C++ with Qt 6 and QML

**Status:** accepted
**Date:** 2026-09-03
**Closes:** ROADMAP D2 (DESIGN §14 Q1)

## Context

Every first-party surface (launcher, greeter, Guardian tool, quick-actions
overlay, first-boot wizard, restricted shell, any house-style app) needs one
language and one UI toolkit, because a single maintainer cannot carry two and
because DESIGN §6.3 wants one visual language across sessions.

Five options were weighed (see the discussion recorded in the issue for D2):
Python + GTK4 + WebKitGTK, Python + PySide6 + QML, C++ + Qt + QML, Rust, and a
kiosk browser with a local daemon. The recommendation on technical grounds was
Python + PySide6 + QML, for the gentler learning curve from the maintainer's
PHP background. The maintainer chose C++ deliberately: they have wanted to
learn it for years, most code will be written with AI assistance, and
committing to it is the forcing function.

Costs acknowledged at decision time: C++ is the hardest of the options for a
beginner to debug and audit; crashes give no message; the build system is its
own subject. The security boundaries of this system (session dispatch, PAM
rules, PIN overlay, what the shell refuses to run) will be in a language the
maintainer is still learning.

## Decision

- **Language:** C++20. **Toolkit:** Qt 6 (Fedora's packaged version).
  **UI:** QML for every screen; C++ for logic, exposed to QML with
  `QML_ELEMENT`. No business logic in QML JavaScript.
- **Build:** CMake + Ninja. **Tests:** Qt Test for C++ classes,
  `qmltestrunner` for QML where it earns its place. **Style:** clang-format
  with the checked-in config; clang-tidy; `-Wall -Wextra -Werror` in
  development builds; AddressSanitizer and UndefinedBehaviorSanitizer on in
  Debug.
- **The restricted shell is also C++/Qt.** One language across the repo
  matters more for a learning maintainer than Python's iteration speed, and
  a QML text surface gives the large type and icon-augmented `ls` the design
  asks for without a PTY. Python remains only for `tools/kidscan` (already
  written, standard library) and glue scripts.
- **Brand tokens** are generated from `brand/tokens.json` into a QML
  singleton (`Cairn.Brand.Tokens`) alongside the CSS, by `brand/build.py`.
- **Greeter:** because SDDM themes are QML, D6 now favours **SDDM with a
  Cairn theme** over greetd with a custom greeter. Recorded when D6 closes.
- **Dependencies:** Qt only. Anything else needs its own ADR.

## Consequences

- The greeter, the launcher, the Guardian tool and the L3 Plasma look-and-feel
  are one skill. GCompris is the same stack.
- Footprint is the smallest of the realistic options; the hardware floor
  question (ROADMAP §1) loses most of its urgency.
- Phase 0 will be slower to first pixel than a Python prototype. Mitigation:
  keep components tiny, one class per file, tests from the first commit,
  sanitizers on by default so crashes do give a message.
- AI-written code must be readable by a learner. `CLAUDE.md` carries the
  conventions that make that true, including explaining a C++ or Qt concept
  the first time it appears in the repo.
- If a component proves unworkable, the fallback is Python + PySide6 with
  the same QML, since QML is shared between the two. The UI investment is
  never lost.
