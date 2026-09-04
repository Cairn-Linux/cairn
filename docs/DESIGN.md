# Cairn Linux — Design Document

**Status:** Draft
**Last updated:** 2026-09-03
**Home:** cairnlinux.com

A Linux system for children aged roughly 5–8 that grows with them, built for
public release.

A cairn is a small stack of stones left by someone who walked the trail
first — a marker that helps whoever comes next without walking the path for
them. That is the design brief.

---

## 1. Context

### 1.1 The problem

Young children get handed either a tablet (consumption-oriented, hides the
machine entirely) or an adult desktop OS (overwhelming, breakable, full of
things they shouldn't see). There's little in between that treats a
six-year-old as someone capable of learning how a computer actually works.

Meanwhile most households have a laptop from 2013–2018 sitting unused that is
more than adequate for this purpose.

### 1.2 Goals

- A system a 5-year-old can use unsupervised without getting lost or breaking it.
- Teach the machine, not just apps: keyboard, mouse, files, sequencing,
  and eventually the command line.
- Grow with the child through age ~14 rather than being outgrown at 8.
- Work as a shared family computer, with each child at their own level.
- Run the games and software children actually want, not only educational software.
- Survive its maintainer going quiet for six months.

### 1.3 Non-goals

- Not a classroom management or fleet deployment system (see Debian Edu for that).
- Not a general-purpose distro that happens to have a kids' theme.
- Not a walled garden. The system should make itself legible, not hide itself.
- Not a replacement for adult supervision.

### 1.4 Who the user actually is

**The user is a parent or teacher, not the child.** A six-year-old does not
download ISOs, evaluate distros, or configure parental controls. Every
acquisition decision, install decision, and trust decision is made by an adult
who has never heard of this project.

This has consequences that run through the whole design:

- Installation must be boring and safe. A parent will not wipe a working laptop
  for an unsigned image from a stranger.
- Documentation is a first-class deliverable, not an afterthought.
- Anything that requires the adult to hand-edit config is a design failure.
- The pitch has to be concrete. "Another educational Linux" is not a reason to
  spend an evening on this.

### 1.5 Why previous attempts died

DoudouLinux, Qimo4Kids, LinuxKidX, and Kano OS were all reasonable projects that
are now dead or dormant. The common cause was not technical failure. It was that
each was a **fork** maintained by one or two people, so security updates,
hardware support, and package maintenance all landed on them personally until
they burned out.

Debian Edu / Skolelinux survived because it is a package set and configuration
layer on top of Debian rather than a fork.

**Design consequence:** inherit everything possible from upstream. Own only the
thin layer that makes this project distinct. This principle drives the base OS
decision in §4.

---

## 2. Principles

1. **Tools, not toys.** The child makes things. Activities and drills are
   available but are not the front door. Respect reads as capability to a kid
   faster than cartoon mascots do.

2. **The machine is legible.** The Linux underneath is invisible at Level 1, but
   the *computer* is not. Deterministic behaviour, real vocabulary, real
   consequences at an appropriate scale.

3. **Capability grows, the system doesn't change.** A child moves up levels on
   the same machine with the same visual language. No "big kid mode" that looks
   like a different product.

4. **No storefront in front of a child.** Games yes, shops no. See §8.

5. **Failure is safe and boring.** Wrong commands, broken states, and confused
   six-year-olds are normal. Every failure path leads somewhere calm.

6. **Nothing phones home.** No telemetry, no accounts, no analytics. See §9.3.

---

## 3. The capability ladder

The spine of the design. Level is a **property of a user account**, not a mode
toggle, so a shared family machine serves multiple children at different levels
simultaneously with proper file separation and per-child controls.

| Level | Age | Shell | Desktop | Games |
|---|---|---|---|---|
| L1 | 5–6 | ~5 commands, launches apps only | Launcher only, no filesystem | Curated tiles, incl. allowlisted Steam titles |
| L2 | 7–8 | ~12 commands, sees own files | Launcher + own files | Curated tiles, incl. allowlisted Steam titles |
| L3 | 9–11 | Real shell, no sudo | Plasma, locked down | Steam, allowlisted |
| L4 | 12+ | Full bash | Stock Plasma + controls | Full |

An adult raises a child's level from the Guardian role (§3.2). Levels are not
age-gated automatically.

**Level changes are symmetric.** Dropping a child from L3 to L2 must be as
routine as raising them. A level is a fit assessment, not a rank, and lowering
one should not feel like a punishment ritual.

### 3.1 The terminal

The most distinctive part of the design, and the reason the ladder works.

The premise: a generation of children learned computing by typing `cd games`
and a filename into MS-DOS to launch a floppy. That worked, and it worked
because of four properties — not because a terminal is inherently good for
children.

1. **A reward on the other side.** The child wasn't learning DOS. They were
   getting to the game. The command was a door.
2. **A tiny memorizable vocabulary.** Perhaps five things total.
3. **Perfect determinism.** Same input, same result, every time. This is what
   builds a mental model of a machine rather than a magic box.
4. **Consequence-free failure.** "Bad command or file name." Nothing broke.

Modern bash has none of these: unbounded surface area, cryptic errors, genuine
destructive power, and no reward at the end.

**Therefore:** a purpose-built restricted shell, not bash with a constrained
`.bashrc`. Design requirements:

- Real command names (`ls`, `cd`, `open`, `cat`), not invented kid-verbs. The
  entire value is transfer to a real system later; made-up vocabulary means
  relearning.
- Large type, generous line height, high contrast.
- Aggressive autocomplete and hinting. A 5-year-old typing at one character per
  five seconds is expected and fine — that *is* the keyboard lesson.
- Errors that suggest rather than scold. Never a stack trace, never a raw errno.
- Nothing destructive reachable at L1 or L2.
- Icon-augmented output where useful (`ls` showing pictures alongside names for
  pre-readers).

Vocabulary per level is an open question (§14).

### 3.2 The Guardian role

Adult administration is a **role beside the ladder, not a level on top of it.**

It is deliberately *not* "L5". Three reasons:

1. An L4 teenager should not perceive themselves as one rung from
   administering the machine. Levelling up should never terminate in root.
2. Households have more than one adult. A role supports N guardians; a ladder
   rung implies one.
3. A rung invites the wrong implementation — "same session type, more
   permissions" — when what's needed is a real privilege boundary with its own
   authentication.

The Guardian role is **invisible from child sessions.** No greyed-out entry, no
locked icon, nothing to probe.

#### What Guardians own

| Area | Actions |
|---|---|
| Accounts | Create, archive, delete child accounts; set name and avatar |
| Levels | Raise or lower any child's level |
| Permissions | Per-child app allowlist, game allowlist, browser access |
| Time | Schedules and daily limits |
| System | OS updates and rollback (§10), Wi-Fi, printers |
| Games | Run the scanner, assign categories, manage tiles (§8.7) |
| Data | Reset a child account; export creations (§11) |

#### Two access paths

**Full administration — separate login.** Requires the Guardian account
password. This is where account creation, level changes, updates, rollback, and
data export live.

**Quick actions — PIN-gated overlay inside a child session.** Because the common
case is a parent standing at the child's shoulder wanting to grant fifteen more
minutes or unlock one app. Logging the child out and back in for that is enough
friction that parents will simply disable the controls instead.

**Assume the PIN is compromised.** Children watch hands; an eleven-year-old will
have it within a week. The design constraint follows directly: the overlay
exposes a small, fixed, allowlisted set of actions and is **not a door to the
full admin surface.** A leaked PIN should cost fifteen extra minutes of
screen time, not the machine.

Candidate quick actions: extend time, unlock a specific app for this session,
exit to the login screen, switch user. Nothing that changes persistent
configuration.

#### No remote management (deliberate)

Parents are accustomed to approving requests from a phone — Steam Families works
this way. Doing the same here would require a network service and accounts,
which §9.3 explicitly rules out.

**We accept the limitation.** All administration is local, at the machine. This
is a real disadvantage versus commercial parental control products and should be
stated plainly in user-facing documentation rather than discovered.

---

## 4. Architecture

### 4.1 Base OS: bootc (decided)

The system is built as an OCI container image and booted natively via `bootc`,
following the Universal Blue `image-template` pattern: a `Containerfile`, a
GitHub Actions workflow that builds and signs the image, and
`bootc-image-builder` to produce installable ISOs. The base is **Bazzite**
(KDE variant), a Universal Blue image derived from Fedora, chosen in ADR-0006
after a licence and trademark review; one Intel/AMD image in v1.

**Why, over Debian + live-build:**

- **Sustainability.** CI is a container build, not bespoke ISO tooling. The base
  image keeps receiving updates whether or not this project is actively
  maintained that month. This addresses §1.5 more directly than anything else.
- **Immutability.** `/usr` is read-only. A child, a bad install, or a confused
  parent cannot damage the OS in place.
- **Atomic updates and free rollback.** See §4.6.
- **Reproducibility.** The image is the same everywhere. Bug reports become
  tractable.

**Costs accepted:**

- Fedora-based, so a faster release cadence and less familiar territory than
  Debian for a Debian-experienced maintainer.
- Package layering is more constrained than `apt install`. Most software must
  come via Flatpak (§4.2), which is fine and arguably better here.
- Smaller precedent for this specific use case.

**Rejected: Ubuntu.** Snap packaging is the disqualifier. Firefox and
increasingly Steam ship as snaps, and snap first-launch latency on 2015-era
hardware means a child taps an icon and stares at nothing for many seconds. That
reads as "broken" to both the child and the parent watching. Secondary reasons:
stricter and more actively enforced trademark policy for derivatives, and
Ubuntu Pro upsell messaging in MOTD, which is advertising on a child's machine.

**Rejected: Debian + live-build.** A perfectly defensible conservative choice
and the fallback if bootc proves unworkable. Loses on structural durability:
snapshot-based rollback via Timeshift/btrfs is something you configure and
maintain, versus something that is structurally true.

### 4.2 Application delivery: Flatpak

- Sandboxing by default.
- The integration point for `malcontent` parental controls (§9.1).
- Current application versions rather than whatever the base froze.
- Fits the immutable base cleanly; avoids package layering.

RPM layering is reserved for things that genuinely must be in the image:
the launcher, the shell, session plumbing, ScummVM.

### 4.3 Sessions and accounts

**Accounts, not modes.** Each child has a real user account. The level is a
property of that account and determines which session type launches at login.

- Login screen: large avatar tiles, no passwords for L1/L2 accounts.
- Guardian accounts (§3.2) are standard administrative accounts, password
  protected, and are not shown as avatar tiles alongside the children.
- File separation, per-child parental controls, and per-child app allowlists all
  fall out of standard Linux multi-user for free.

### 4.3.1 First boot

The first thing a parent sees after installation, and therefore the project's
first impression. A wizard that:

1. Creates the first Guardian account and its password.
2. Sets the quick-actions PIN (§3.2), with a plain warning that children will
   learn it and that it intentionally unlocks very little.
3. Creates the first child account: name, avatar, level.
4. Offers to scan for installed games (§8.7) if any are present.
5. Explains where creations are stored and how to get them out (§11).

Nothing here may require editing a config file. If a step can't be done in the
wizard, it isn't ready to ship.

A mode toggle within a single account was rejected: no file separation, controls
can't differ per child, and any child can flip any other child's mode.

### 4.4 Display stack

**Wayland, with XWayland for games.**

The deciding factor is focus stealing. Under Wayland a rogue window cannot grab
focus or escape the kiosk surface, which matters when the entire L1/L2 design
premise is that the child cannot get out of the launcher.

### 4.5 Sessions per level

| Level | Session |
|---|---|
| L1–L2 | Kiosk compositor running the launcher fullscreen: `labwc` in kiosk configuration, because §8.3 needs window rules and foreign-toplevel notifications that a single-app compositor such as `cage` lacks (ADR-0004) |
| L3–L4 | KDE Plasma |

**L1/L2 use no desktop environment at all.** There is no panel, no window
management, no application menu — those are precisely what's being removed.
Installing a DE and then disabling it for months would be the wrong path.

**Plasma for L3/L4** primarily for the KDE Kiosk framework, which provides
first-class infrastructure for locking down and hiding desktop features via
immutable configuration keys. Nothing in GNOME or Xfce matches it for "this
desktop is real but constrained," which is exactly what L3 needs. Secondary
benefit: Qt/QML alignment with GCompris.

The two session types share a visual identity (§6) but not a desktop.

### 4.6 What rolls back and what doesn't

An important distinction that the "immutable OS" framing tends to obscure:

| Swappable | Persistent |
|---|---|
| `/usr` — read-only, shipped in the image | `/home` — drawings, settings, saves |
| Multiple deployments on disk; bootloader picks | `/var` — Flatpak data, game installs |

`bootc rollback` plus a reboot restores the previous OS exactly. It does
**nothing** to a child's mangled settings or lost files. OS reset and child-account
reset are two separate problems; bootc solves only the first. See §11.

---

## 5. Software catalog

### 5.1 The make/practice split

The front door is **instruments**, not exercises. This is the Papert
constructionist lineage — "objects to think with" — and it's the main thing
distinguishing this from previous kids' distros, which were largely activity
collections with a themed wallpaper.

**Front page (make):** draw, music, build (block coding), write/story, camera,
terminal.

**In a folder (practice):** GCompris and the Tux suite. Excellent software,
genuinely useful, but not the identity of the system.

### 5.2 Bundled applications

| Application | Role | Notes |
|---|---|---|
| GCompris (Qt variant) | Practice activities, ages 2–10 | Actively maintained; v26 has ~197 activities. The GTK-era `gcompris` package is unmaintained — avoid. |
| Tux Paint | Drawing | Proven, dated-looking |
| TuxMath, Tux Typing | Practice | |
| ScummVM | Game engine | See §8 |
| ScratchJr | Block coding, ages 5–7 | **Problem — see below** |
| Scratch 3 | Block coding, ages 8+ | Offline builds are unofficial on Linux |
| First-party launcher + shell | Identity | §6, §3.1 |

**The ScratchJr problem.** ScratchJr is the canonical block-coding tool for
exactly this age range and it effectively does not exist on Linux. There is no
official Linux build; the community Electron port targets Mac and Windows with
Linux explicitly not implemented, and the AUR package builds from that repo with
an external patch.

This is real work, but it is also the most valuable upstream contribution this
project could make, and it would benefit people far beyond this distro.
Treat as a distinct workstream.

---

## 6. Interface and visual design

### 6.1 The launcher

The launcher **is the product.** Everything else is packaging.

- Fullscreen grid of large tiles. No window management, no taskbar, no
  filesystem, nothing to get lost in.
- No reading required beyond app names, which are paired with distinct icons.
- L1: ~6 tiles plus a small dock. L2: grid grows, dock gains a files view.
- Colour groups by *kind* (make / practice / machine), not one colour per app.

### 6.2 Visual direction

The brief is "creative workspace," not "toy." Concretely:

- **Restrained palette.** Four or five colours on a neutral ground. Not primary
  colours, not rainbow.
- **Flat.** No bevels, gloss, gradients, or skeuomorphism.
- **Generous whitespace.** Large touch/click targets with real space around them.
- **Typography:** a geometric sans with high legibility. Atkinson Hyperlegible
  is a strong candidate — designed to disambiguate letterforms like I/l/1 and
  O/0, which matters disproportionately for early readers.
- **No** sound effects on every interaction, sticker rewards, gamification
  streaks, or mascot characters.

### 6.3 The consistency limit (accepted)

You can control the frame completely: login screen, launcher, dock, terminal,
transitions, and first-party apps. You **cannot** make GCompris and Tux Paint
match it. Retheming ~197 GCompris activities is not realistic, and Tux Paint
looks its age.

Every project in this space has hit this wall. The workable answer is:

1. Make the frame consistent enough that app windows read as *contents* rather
   than as parts of the system.
2. Write one or two first-party apps in the house style so the aesthetic has
   somewhere to live. A web-based drawing or music tool in a chromeless kiosk
   window is achievable and would carry much of the identity.

Do not attempt to reskin third-party applications.

---

## 7. Hardware

**x86_64 only.** ARM and Raspberry Pi are explicitly out of scope for v1:
Raspberry Pi OS already serves that niche well, and a second architecture roughly
doubles build, test, and support surface. Steam and Proton are x86 regardless.

**Two documented tiers**, presented the way the back of a PC game box did
it — Minimum and Recommended — because that is a format parents already know
how to read. The games decision (§8) is what separates them. Decided in
ADR-0003; the previous 2 GB minimum contradicted the 2013–2018 target above.

| | Minimum | Recommended |
|---|---|---|
| Laptop age | About 2013 or newer | About 2015 or newer |
| Processor | 64-bit Intel or AMD | Same |
| Memory | 4 GB | 8 GB |
| Graphics | Intel HD 4000 or newer integrated | Discrete, or modern integrated (Iris, Vega) |
| Storage | 64 GB | 128 GB |
| Firmware | UEFI | UEFI |
| Runs | Launcher, shell, educational stack, ScummVM titles, Plasma at L3/L4 | Everything above, plus Steam/Proton titles and Minecraft Java |

State this plainly in the documentation, in this form, everywhere it appears.
Below Minimum the honest answer is "not this machine". A parent installing on
a 2009 netbook and concluding the system is broken is a worse outcome than a
parent who reads the table and buys nothing. BIOS-only machines are not
supported in v1.

---

## 8. Games

### 8.1 Why this matters

Children are pragmatic. If their friends play something and this system doesn't
run it, it becomes the boring computer. Bundled educational software alone has
never been enough to drive adoption for any project in this space.

### 8.2 SCUMM-era titles: no Steam client required

The Humongous Entertainment catalogue (Putt-Putt, Freddi Fish, Pajama Sam, Spy
Fox — 35 games in the Complete Pack) are SCUMM engine titles, and the Steam
releases are ScummVM builds. Several shipped as native Linux releases in 2014.

Therefore: no Proton, and no Steam client on the child's machine for these.
Install once from an adult account, point native ScummVM at the data directories
under `steamapps/common/`, and each game becomes a launcher tile that starts
ScummVM directly with the target ID. The child taps Freddi Fish and Freddi Fish
starts.

These games are an unusually good fit for ages 5–8: point-and-click problem
solving, fully voice-acted, no reading required.

### 8.3 Steam and Proton: available, but never visible

Steam titles are supported at all levels, including L1 and L2, because
exploration games matter for adoption and because the library a family already
owns is the point of §8.6. This is a **v1 requirement** (ADR-0004), not a
stretch goal. But the **client UI** is never what a child touches.

Mechanism:
- Steam starts with `-silent` at session login, without a window. There is no
  tray in the L1/L2 kiosk, so the compositor keeps the client's windows off the
  screen (mitigation 3 below).
- Each game is a tile running `steam -applaunch <appid>`.
- Proton work happens in the background; the game comes up fullscreen.

**Known failure modes.** Steam forces its window open for client updates, game
updates, login expiry, Steam Guard prompts, and cloud save conflicts. Mitigations:

1. Enable Family View with a PIN on the client itself, so that even when the
   window surfaces, store/community/friends are locked. Defence in depth.
2. The launcher watches for failed or slow launches and shows a plain
   "something needs a grown-up" screen rather than leaving the child staring at
   whatever Steam decided to display. **Graceful failure matters more at this age
   than at any other.**
3. The kiosk compositor enforces containment. Window rules keep Steam's own
   windows (main client, login, update progress, Steam Guard) minimised or off
   the screen, and the compositor's foreign-toplevel protocol tells the launcher
   when one has appeared so it can show the grown-up screen instead. This is
   why L1/L2 use `labwc` rather than a single-app kiosk compositor (§4.5). It
   is proven in Phase 0 before the launcher is finished (§13).

Combined with a child Steam account in a Steam Family with an explicit game
allowlist, the exposed surface is small.

### 8.4 Steam Families

Steam Families replaced the older Family Library Sharing and Family View. Up to
six members share a pooled library, each with their own saves and achievements.
Adults can allowlist which games a child sees and can launch, set playtime
schedules, restrict store/community/friends access, and approve purchase
requests.

Caveats to document: developers can opt titles out of Family Sharing at any
time, and two people cannot play the same title simultaneously unless the family
owns multiple copies.

### 8.5 Minecraft Java

Not on Steam; a separate integration. Java Edition has an official native Linux
launcher, no Proton involved. Bedrock has no official Linux build — Java is the
right target regardless.

Worth treating as a **first-class integration rather than one more tile**,
because it is the best available expression of the ladder in §3: play → command
blocks → resource packs → datapacks → mods → actual Java. A child can enter at
six and still be climbing at fourteen on the same install.

Requires a Microsoft account per player; document as a family setup step.

### 8.6 Bring your own library

The project ships the **plumbing**, never the games. ScummVM, launcher
integration, the detection tool, and tile generation all distribute freely. Game
data belongs to whoever bought it.

Position this as a feature: *turn the point-and-click games you already own into
a child's home screen.* That is a far more concrete pitch to a parent than
another list of bundled edutainment.

### 8.7 Tooling

A scanner (`kidscan`) exists in prototype: finds Steam roots including the
Flatpak path, parses `libraryfolders.vdf` for secondary drives, reads every
`appmanifest_*.acf`, runs `scummvm --detect` per install directory, and emits a
launcher-agnostic JSON manifest plus optional `.desktop` files. Titles ScummVM
recognises get native launch commands; everything else routes through Steam.
Filters out Proton, Steam Linux Runtimes, and Steamworks Redistributables so
they don't appear as games.

Not yet built: a GUI for category assignment (a parent should never edit
appids), icon sourcing (§14), and the launch-timeout handling described in §8.3.

---

## 9. Safety, privacy, and parental controls

### 9.1 Two complementary control layers

- **`malcontent`** — app-level control. Which applications can run, OARS age
  ratings, installation permissions. Originated at Endless; integrates with
  Flatpak. A D-Bus service and library rather than a GNOME-only component.
- **KDE Kiosk** — desktop-level control. Which UI elements exist at all, via
  immutable configuration keys.

These solve different problems and the design wants both. **Verify early that
malcontent works cleanly on Plasma** — if it doesn't, the L3/L4 desktop choice
may need revisiting.

### 9.2 Content filtering

DNS-level filtering as a baseline. Browser access is absent at L1, curated at
L2–L3. Details TBD.

### 9.3 No telemetry, no accounts

The system collects nothing and requires no account to function. This is:

- The correct default for software used by children.
- A legitimate selling point to parents.
- The simplest way to sidestep COPPA (US) and GDPR children's-data obligations
  (EU) entirely, rather than trying to comply with them.

Exception: Steam and Minecraft require their own accounts. These are the
family's relationship with those vendors, not this project's, and should be
documented as such.

---

## 10. Build and release

- `Containerfile` + GitHub Actions, following the Universal Blue image-template
  pattern.
- Images signed with cosign; published to a container registry.
- ISOs generated via `bootc-image-builder`.
- Update cadence inherited from the base image. `bootc upgrade` on a schedule,
  applied at reboot.
- **Rollback is a documented, supported, one-command parent operation**, not an
  advanced recovery procedure.

---

## 11. User data and reset

Two separate reset problems (see §4.6):

**OS reset** — solved by bootc. `bootc rollback`, reboot.

**Child account reset** — must be built. Design:

- Everything in a child's home is **disposable except one creations directory**.
- Reset wipes the rest and re-copies from a skeleton.
- Creations sync automatically to a parent-accessible location.

This makes resetting a stuck six-year-old's account safe and boring. Without it,
reset means losing their work and no parent will ever dare do it.

**A child's drawings are the emotional payload of this system.** "I couldn't get
my kid's pictures off it" would sink the project regardless of technical merit.
Getting creations out must be trivial and obvious.

---

## 12. Legal

### 12.1 Name

"Cairn Linux", at cairnlinux.com. Due diligence completed 2026-09-03:

- **No existing Linux distribution** uses the name. This is the collision that
  would have mattered and it is clear.
- Known unrelated uses: Cairn.info (Francophone academic publishing, since
  2005), CairnOpen (CEA-Liten energy simulation), a survival-climbing game by
  The Game Bakers, and several AI developer tools. Different categories; no
  category overlap with an operating system.
- **Accepted cost — search.** Because the climbing game runs on Linux via
  Proton, the exact phrase "Cairn Linux" currently returns Proton compatibility
  pages and gameplay videos. Expect to share the search term for a while.
  Games fade from results faster than distributions do.
- **Accepted cost — Cairo.** `cairo` is a foundational Linux graphics library
  and sits one letter away. End users never encounter it; developers can cope.
  A small ongoing tax, judged worth paying.
- `cairnlinux.org` is registered by a third party. `.com` chosen instead, which
  is also the better fit for a parent audience (Ubuntu, Linux Mint, Zorin, and
  EndeavourOS all use `.com`).

### 12.2 Pre-release tasks

- **Linux Mark Institute sublicense.** "Linux" is a registered trademark held by
  Linus Torvalds and administered by the LMI under the Linux Foundation. Using
  it in a product name requires a sublicense. These are granted free and the
  process is routine, but it is paperwork with a lead time — start early, not at
  release.
- **Universal Blue / Fedora trademark policy** for derivative images — review
  before publishing.
- **Scratch Foundation branding guidelines** — review before using the name or
  marks anywhere in UI or documentation.
- **Trademark search** on "Cairn" in software classes for the target
  jurisdiction. Cairn.info is the nearest adjacent-sector holder.

### 12.3 Licensing

- **ScummVM is GPLv2**; the Steam releases note this explicitly. Redistribution
  of the engine is fine; game data is not ours to ship (§8.6).
- **Project licence** — Apache-2.0 for code, CC BY-SA 4.0 for documentation
  and brand assets, with the name and mark as project trademarks (ADR-0005).

---

## 13. Roadmap

### Phase 0 — validate the experience (no ISO)

Build the launcher shell and a provisioning script that turns a stock install
into this system. Put it on a real machine in front of a real 5–8 year-old and
watch. Iterate on the design before investing in build tooling.

*The point: avoid months of image plumbing on a design that hasn't been tested
with a child.*

### Phase 1 — MVP image

L1 only. Launcher, restricted shell, core educational apps via Flatpak, ScummVM
integration, Steam integration with the silent client and containment
(§8.3, ADR-0004). Guardian role with account creation and level management (§3.2),
first-boot wizard (§4.3.1), quick-actions overlay. Signed image, CI, ISO.

*Note: the Guardian surface is not deferrable to a later phase. Without it there
is no way to create a child account, so there is no product.*

### Phase 2 — the ladder

L2 and L3. Plasma session, malcontent + Kiosk integration, files view, expanded
shell vocabulary.

### Phase 3 — games and polish

Minecraft Java integration, `kidscan` GUI, icon system. (Steam integration
moved to Phase 1 by ADR-0004.)

### Ongoing / parallel

ScratchJr Linux packaging (§5.2). Independent of everything else; could start
any time.

---

## 14. Open questions

| # | Question | Notes |
|---|---|---|
| 1 | Launcher implementation | Web-based gives fastest iteration on the visual design, but Electron on a 2 GB machine is rough. WebKitGTK or QtWebEngine in a kiosk window gets most of the benefit at a fraction of the memory. Decide deliberately; don't default to Electron. |
| 2 | L1/L2 command vocabulary | Which ~5 and ~12 commands, exactly. Needs testing with an actual child. |
| 3 | Icon art source | Steam caches capsule art under `appcache/librarycache`, but using it drags the store's visual language onto the home screen. First-party icon set? Commissioned? Generated? |
| 4 | Offline-first? | **Decided (ADR-0007):** offline-capable, not offline-first. Everything a child does works without a network; updates and the Steam/Minecraft integrations need one and fail calmly. |
| 5 | Localization scope | **Decided (ADR-0007):** English-only v1, every first-party string externalised from the first commit so translation later is a translation job. |
| 6 | Accessibility beyond typography | **Decided (ADR-0008):** high-contrast and large-text modes in v1 as token variants; screen reader and dwell click in Phase 3; `Accessible.name` on every control from the start. |
| 7 | Does malcontent work cleanly on Plasma? | If not, §4.5 and §9.1 need revisiting. Verify early — this is cheap to test and expensive to discover late. |
| 8 | Guardian quick-actions: exact allowlist | §3.2 sets the constraint (small, non-persistent). The specific set needs deciding. "Extend time" in particular — by how much, how many times, and does it need a cap? |
| 9 | Can a household have zero Guardians? | If the only Guardian account is lost or forgotten, what's the recovery path? Rescue via bootloader is the honest answer but needs documenting, and it's also a bypass a determined teenager will find. Full-disk encryption closes it and makes a forgotten password brick the family computer. |

---

## 15. Prior art

| Project | Why it's worth studying |
|---|---|
| Raspberry Pi OS | The de facto kids' learning Linux. Scratch, Thonny, sane defaults. |
| Debian Edu / Skolelinux | The sustainable architecture: config layer, not a fork. |
| Endless OS | Immutable OSTree base, offline-first, `malcontent` origin. |
| Sugar / OLPC | Radical UI rethink for children. Also a cautionary tale about diverging too far from what children see everywhere else. |
| DoudouLinux, Qimo4Kids, LinuxKidX, Kano OS | The graveyard. Read §1.5. |
| Universal Blue (Bluefin, Bazzite, Aurora) | The build pattern this project adopts. Bazzite in particular for gaming on an immutable base. |
