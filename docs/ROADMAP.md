# Cairn Linux — Roadmap and working plan

**Status:** living document
**Last updated:** 2026-09-03 (D2 closed)

`DESIGN.md` is the specification. This document is the plan for building it:
what has to be decided, in what order things get built, how we know a phase is
done, and what could sink it. Decisions that change the design get an ADR in
`docs/decisions/` and an edit to `DESIGN.md` in the same change.

---

## 0. Where things stand

- Design document at draft, dated 2026-09-03. Brand guidelines v0.1 (mark,
  palette, type, voice) as a design canvas in `docs/brand-guide/`.
- GitHub organisation `Cairn-Linux` created 2026-09-03. This repository is its
  first.
- The `kidscan` scanner prototype is imported at `tools/kidscan/` with tests
  (2026-09-03). No other code yet.
- Brand tokens and the mark have been extracted to `brand/` so code has a
  single source of truth from the first line.
- **Phase 0 begins.** Its whole purpose is to put a launcher in front of a
  real child before any image tooling exists.

---

## 1. Decisions needed

Each has a recommendation. None is final until an ADR lands in
`docs/decisions/`. Ordered by when they bite.

| ID | Decision | Recommendation | Bites when |
|---|---|---|---|
| **D1** | Project licence (DESIGN §12.3) | **Apache-2.0** for code, matching Universal Blue and its template so vendored pieces mix cleanly; **CC BY-SA 4.0** for docs and brand assets. GPL-3.0 is the defensible alternative if copyleft matters more than ecosystem fit. | Before the repo goes public. Nothing else blocks on it. |
| **D2** | First-party language and UI toolkit (DESIGN §14 Q1) | **Closed 2026-09-03, ADR-0002: C++20 + Qt 6 + QML** for every first-party surface, including the restricted shell. Chosen by the maintainer to learn C++; the technical recommendation had been Python + PySide6 with the same QML. Fallback if a component proves unworkable is exactly that, and the QML carries over. | Closed |
| **D3** | Kiosk compositor for L1/L2 (DESIGN §4.5) | Start with **cage**: it exists to run one fullscreen app and stacks spawned windows on top. **But** §8.3's "Steam goes to tray" has no tray in a kiosk, so Steam's forced windows would appear fullscreen. If cage can't hide them, move to **labwc** with window rules. Test both failure paths in P0-4. Whether Steam at L1/L2 is a v1 requirement at all is a scope question still open with the maintainer. | P0-4 |
| **D4** | Base image (DESIGN §4.1 says Fedora/bootc, not which) | **`ghcr.io/ublue-os/bazzite:stable`** (KDE variant). Ships Plasma, Steam, Proton plumbing and hardware enablement that Phases 2–3 need; a kiosk session on it doesn't load Plasma, so the runtime cost on the 2 GB tier is disk, not RAM. Alternative: `kinoite-main` + layered Steam, if Bazzite's footprint or trademark policy is a problem. Check both in P1-1. | P1-1 |
| **D5** | How "level" is stored and enforced (DESIGN §3, §4.3 name the property; nothing names the mechanism) | **Supplementary groups**: `cairn-l1` … `cairn-l4`, `cairn-guardian`; exactly one per account. Level change = group change. PAM, polkit, the session dispatcher and malcontent policy all key off groups, which is Unix-native, inspectable and legible (Principle 2). Per-child name/avatar/allowlists live in a small config the Guardian tool owns. | P0-2 |
| **D6** | Display manager and greeter (DESIGN §4.3 describes the login screen, not the component) | **SDDM with a Cairn QML theme.** Since ADR-0002 the whole first-party layer is QML and SDDM themes are QML, so the login screen is a theme, not a program. `HideUsers` hides Guardians; a PAM rule grants passwordless login to `cairn-l1`/`cairn-l2` members. SDDM is also what Plasma expects. Alternative: greetd with a custom greeter, only if SDDM cannot do the avatar-tile login cleanly. Prototype in P0-3. | P0-3 |
| **D7** | Session dispatch | **One Wayland session entry** (`cairn.desktop` → `cairn-session`) that reads the account's level group and execs either the kiosk compositor + launcher (L1/L2) or `startplasma-wayland` (L3/L4, Guardian). The greeter offers only this session, so a child cannot pick another. | P0-3 |
| **D8** | Localisation and offline stance (DESIGN §14 Q4, Q5) | State them rather than default: **English-only for v1**, with strings externalised from day one so GCompris's translations aren't wasted later. **Offline-capable, not offline-first**: everything works without network except Flatpak/OS updates and the Steam/Minecraft integrations. | Phase 1 docs |
| **D9** | Name and architecture of the restricted shell | Needs a real name before Phase 1 packaging; the child never types it, so it's a package name, not a brand. Architecture proposal since ADR-0002: a Qt/QML text surface with the command interpreter in C++, **not** a PTY program in a terminal emulator. That gives the large type and icon-augmented `ls` directly and makes the hand-off to a real shell at L3 an explicit step. | P1-2 |
| **D10** | Content filtering (DESIGN §9.2 "TBD") | Two options, both local-config-only to honour §9.3: a **filtered upstream DNS resolver** (simple, but sends every query to a third party) or a **local resolver with blocklists** (private, needs list updates via the OS image). Decide in Phase 2 when the browser first appears at L2. | Phase 2 |

---

## 2. Phase 0 — validate the experience (no ISO)

**Goal.** One child aged 5–8, on real 2013–2018 hardware, uses the L1 session
unsupervised for twenty minutes.

**Exit criterion.** The child: logs in from an avatar tile; launches and
returns from at least two apps; uses the shell to open something; hits an
error and recovers without help; never leaves the kiosk. Observations are
written up in `docs/research/`. Then, and only then, Phase 1.

**What Phase 0 deliberately skips.** Guardian GUI (the provisioning script and
a CLI stand in), first-boot wizard, quick-actions overlay, ISO, CI, signing.

| ID | Task | Depends on | Done when |
|---|---|---|---|
| **P0-1** | Repository, planning, brand tokens and mark as code | — | This commit. |
| **P0-2** | **Provisioning script** `provision/` that turns a stock Fedora Kinoite or Bazzite install into a Cairn machine: creates the level groups (D5), one Guardian, one L1 child; installs the kiosk compositor and the Phase 0 app set (Tux Paint, GCompris, ScummVM via Flatpak/dnf); installs the session entry and greeter config. Idempotent; safe to re-run. | D5 | Runs clean twice on a fresh VM. |
| **P0-3** | **Session dispatch and greeter** in `session/`: `cairn-session` dispatcher (D7), `cairn.desktop`, greetd config, PAM rule for passwordless L1/L2. | P0-2 | Child account lands in the kiosk with no password prompt; Guardian account lands in stock Plasma with one. |
| **P0-4** | **Kiosk containment test** with cage running a placeholder client: a launched Tux Paint or ScummVM window appears on top and closing it returns to the launcher; Alt-Tab, Super, and Ctrl-Alt-Fn VT switching are unreachable; a window that steals focus under X11 cannot under Wayland. Record how Steam's forced windows behave (informs D3). | P0-3 | Written checklist in `docs/research/kiosk-containment.md`, all rows pass or have a named mitigation. |
| **P0-5** | **Launcher v0** in `launcher/`: a C++/QML app. Six tiles from `Cairn.Brand.Tokens`, colour by kind; keyboard and mouse navigation; launches apps with `QProcess`; shows "Something needs a grown-up" on launch failure or timeout; logs its own idle RSS. CMake, Qt Test, sanitizers and clang-format set up in this task, since it is the first C++ in the repo. | P0-4 | Runs fullscreen under cage on the target laptop; RSS recorded in `docs/research/launcher-footprint.md`. |
| **P0-6** | **Restricted shell v0** in `shell/`: C++ interpreter behind a QML text surface. L1 vocabulary candidate of five real commands (`ls`, `cd`, `open`, `cat`, `help`), large type, aggressive completion, suggestion-style errors ("I don't know \"opn\" — did you mean open?"), icon-augmented `ls`. Nothing destructive reachable. The interpreter is a pure C++ class with no Qt GUI dependency so it is unit-testable in isolation. | P0-5 | Child can open an app from it. Vocabulary written up for testing (DESIGN §14 Q2). |
| **P0-7** | **malcontent-on-Plasma check** (DESIGN §14 Q7). In a VM: install malcontent, restrict a test user, log into Plasma, confirm restricted Flatpaks refuse to launch and the restriction UI is reachable. | — (parallel) | Result recorded as ADR-0002; if it fails, §4.5/§9.1 get revisited before Phase 2. |
| **P0-8** | **Import `kidscan`** into `tools/kidscan/` with its README; no GUI yet. | — (parallel) | **Done 2026-09-03.** Imported with a fixture-based test suite; still to run against a real Steam library. |
| **P0-9** | **Child test.** Real hardware, real child, twenty minutes, observer takes notes and does not intervene unless asked. Iterate on P0-5/P0-6 and repeat at least once. | P0-2…P0-6 | Write-up in `docs/research/child-test-01.md`. Phase 0 exit criterion met. |

---

## 3. Phase 1 — MVP image (L1 only)

Ships: launcher, shell, core educational apps via Flatpak, ScummVM
integration, Guardian role with account creation and level management,
first-boot wizard, quick-actions overlay, signed image, CI, ISO.

| ID | Task |
|---|---|
| P1-1 | Choose and pin the base image (D4). Measure image size and first-boot RAM on both hardware tiers. |
| P1-2 | Package launcher, shell and session as RPMs (in-tree spec files, built in a Containerfile stage or via a COPR). Name the shell (D9). |
| P1-3 | Adopt `image-template`'s `Justfile` and `build.yml`; generate cosign keys; add `SIGNING_SECRET`; publish to `ghcr.io/cairn-linux/cairn`. |
| P1-4 | ISO via `bootc-image-builder` (`disk_config/iso.toml`); `build-disk.yml`. Test install on both tiers. |
| P1-5 | Guardian tooling: a CLI first (`create-child`, `set-level`, `set-pin`, `allow-app`), then a minimal GUI on it. Level changes symmetric (DESIGN §3). |
| P1-6 | First-boot wizard (DESIGN §4.3.1): Guardian account, PIN with its warning, first child, game scan offer, where creations live. Add Wi-Fi setup, which the design doc's list omits. |
| P1-7 | Quick-actions PIN overlay with the fixed allowlist (DESIGN §14 Q8): extend time, unlock one app this session, log out, switch user. Nothing persistent. |
| P1-8 | Child account reset and creations export (DESIGN §11): skeleton home, one protected creations directory, sync to a Guardian-readable location. |
| P1-9 | Time limits and schedules. malcontent does not do this; needs a session-side timer with a calm end-of-time screen and logind integration. (Gap in the design doc; add a section.) |
| P1-10 | Update policy: `bootc upgrade` timer, apply on reboot; rollback documented as a one-command Guardian operation. |
| P1-11 | Documentation site skeleton: install, hardware tiers stated plainly, "what this does not do" (no remote management), rollback, getting creations out. |
| P1-12 | Guardian-loss recovery path (DESIGN §14 Q9) decided and documented. |

---

## 4. Phase 2 — the ladder (L2, L3)

- L2: launcher grows; dock gains a files view; ~12-command vocabulary.
- L3: Plasma session with a KDE Kiosk profile; malcontent integration (only if
  P0-7 passed); browser present and filtered (D10).
- Level up and level down both routine from the Guardian tool.
- Expanded shell: real shell at L3, no sudo; decide how the restricted shell
  hands off to bash without a vocabulary cliff.

## 5. Phase 3 — games and polish

- Steam: `-silent` at login, `steam -applaunch` tiles, Family View PIN,
  launch watchdog with the "needs a grown-up" screen. Steam Families setup
  documented as the family's relationship with Valve.
- Minecraft Java as a first-class integration, not a tile.
- `kidscan` GUI for category assignment; icon sourcing decided (DESIGN §14 Q3).
- One first-party app in the house style (draw or music) so the aesthetic has
  somewhere to live (DESIGN §6.3).
- Accessibility beyond typography (DESIGN §14 Q6): high-contrast mode, dwell
  click, screen reader survey.

---

## 6. Parallel workstreams (start any time)

| Workstream | Where | Notes |
|---|---|---|
| **ScratchJr on Linux** | Separate repo `Cairn-Linux/scratchjr-linux` | Upstream-facing; the most valuable contribution this project can make. Independent of everything else. |
| **Linux Mark Institute sublicence** | Legal | Free and routine but has lead time. Start now. |
| **Trademark reviews** | Legal | Universal Blue / Fedora / Bazzite derivative policies; Scratch Foundation branding; "Cairn" search in software classes. |
| **Website** `cairnlinux.com` | Separate repo, later | Hold until Phase 1 has something a parent can install. |
| **Brand v0.2** | `docs/brand-guide/` | Iconography, motion, avatars, spacing, sound policy. See the review notes in issue tracker. |

---

## 7. Risks

| Risk | Signal | Mitigation |
|---|---|---|
| C++ is new to the maintainer and most code is AI-written | Bugs that can't be read; crashes without messages | Conventions in `CLAUDE.md`; sanitizers on in Debug; tiny classes; tests from the first commit; the trust boundaries get the most tests and the plainest code. |
| malcontent doesn't work cleanly on Plasma | P0-7 | Revisit §4.5/§9.1 before Phase 2; KDE Kiosk alone may suffice for app gating. |
| Kiosk escape via VT switch or forced windows | P0-4 | logind `HandleXXX` and compositor config; labwc window rules (D3). |
| Steam changes client behaviour under us | Phase 3 | Detection + graceful failure, never rely on a quiet client. |
| Single maintainer | Always | Inherit upstream; keep first-party code small; document every mechanism. |
| Base image trademark constraints | Legal workstream | Review before publishing any image. |
| Atkinson Hyperlegible Mono licence | Phase 1 packaging | Verify the Mono's licence permits redistribution in the image before it becomes the terminal face. |

---

## 8. How work is tracked

- **GitHub milestones** per phase: Phase 0, Phase 1, Phase 2, Phase 3,
  Pre-release legal.
- **Labels**: `area:launcher`, `area:shell`, `area:session`, `area:image`,
  `area:guardian`, `area:games`, `area:brand`, `area:docs`, `decision`,
  `research`, `upstream`, `legal`.
- **ADRs** in `docs/decisions/` for every D-number when it closes.
- `DESIGN.md` changes go through a pull request, even from the maintainer, so
  the reasoning is recorded.
