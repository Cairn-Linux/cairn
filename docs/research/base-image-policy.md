# Base image policy review: Bazzite, Universal Blue, Fedora

**Date:** 2026-09-03
**For:** ROADMAP D4 (base image) and issue #22 (trademark reviews)
**Result:** No blocker. Building Cairn on Bazzite is permitted and encouraged,
with a short compliance checklist below that Phase 1 must satisfy.

## What was checked

| Source | What it is | Checked how |
|---|---|---|
| `ublue-os/bazzite` repository | The image we would build on | Licence, top-level files, code search for trademark/derivative terms, `press_kit/README.md`, build scripts |
| `docs.bazzite.gg` | Bazzite documentation | "Creating A Custom Bazzite Image" page (source in `ublue-os/docs.bazzite.gg`) |
| universal-blue.org | Parent project site | Front page and mission page |
| Fedora trademark guidelines | Apply to everything built from Fedora packages | Source of `docs.fedoraproject.org/en-US/legal/trademarks/` from the `fedora/legal/fedora-legal-docs` GitLab repo (the rendered pages are bot-gated) |
| Bluefin build script | How a sibling Universal Blue image handles Fedora branding | `build_files/shared/build.sh` |
| Press coverage, January 2026 | Bazzite's founder asking GPD to stop using the name | gamingonlinux.com, 2026-01-28 |

## Findings

### Bazzite and Universal Blue

- **Licence: Apache-2.0** for the whole `ublue-os/bazzite` repository and
  every other Universal Blue repository checked (main, bluefin, aurora,
  image-template, akmods, packages). Compatible with Cairn's Apache-2.0
  (ADR-0005); attribution goes in `NOTICE`.
- **Derivatives are the intended use.** universal-blue.org: "Universal Blue
  images are endlessly customizable and may be derived into new images
  through the use of a Containerfile." The Bazzite README: "Creating your own
  custom version of it is as easy as forking this repository, adding a private
  signing key, and enabling the fork's GitHub Actions." The docs page
  "Creating A Custom Bazzite Image" recommends exactly the `image-template`
  route this repository already uses, and lists "different desktop
  environments or window managers versus what Bazzite offers" as a use case,
  which is what Cairn's L1/L2 kiosk is.
- **There is no written derivative-naming or trademark policy.** The only
  branding document is `press_kit/README.md`. It governs use of the Bazzite
  and Universal Blue names and logos: do not distort or modify the artwork;
  do not say "Bazzite OS", "Fedora Bazzite", or "uBlue"; say "built with
  Universal Blue" or "a Universal Blue image". Contact for questions:
  press@bazzite.gg.
- **They do enforce the name.** On 2026-01-28 the founder publicly asked GPD
  to "cease using the Bazzite name, logos, or other intellectual property"
  after GPD advertised an "official Adaptation" of Bazzite and pointed users
  to an unofficial support channel. The complaint was about implied
  affiliation and support, not about someone building on Bazzite.
- **Bazzite's own docs shipped in the image are GPL-3.0** and its portal
  icons are Font Awesome Free (CC BY 4.0 / OFL). Cairn ships neither.
- **Bazzite rebrands `/usr/lib/os-release`** at build time (`NAME`, `ID`,
  `PRETTY_NAME`, `LOGO`, `CPE_NAME`, URLs) but does not appear to swap the
  `fedora-logos` package. Bluefin does: `dnf -y swap fedora-logos
  generic-logos`, then erases `generic-logos` from the RPM database with a
  comment that an empty `fedora-logos` package may not be shipped.

### Fedora

Quoted from the guidelines ("Distributing combinations of Fedora Materials
with non-Fedora or modified Fedora Materials"):

> the fedora-logos, fedora-release, and fedora-release-notes RPM packages are
> removed, although you may replace these with packages of your own devising
> not containing the Fedora Trademarks; and a notice is prominently displayed
> indicating that: the software provided is not provided or supported by the
> Fedora Project; and official Fedora Materials are available through the
> Fedora Project website, and linking to the Fedora Project website at
> https://fedoraproject.org/.

- A derivative **may say it is derived from Fedora** in plain text. It may
  not use the Fedora logo, trade dress, or the word "Fedora" in its own
  name, domain, or marks. The optional "Fedora Remix" mark is available but
  not required; Universal Blue does not use it and neither will Cairn.
- Fedora's own footer requirement applies wherever the Fedora word mark is
  used: the ® symbol on first mention and the attribution statement.

## Compliance checklist for Cairn (Phase 1, before any image is published)

- [ ] **Never use the Bazzite name, logo or artwork** in Cairn's name, UI,
  website or ISO. Describe the lineage in prose only: "Cairn Linux is built
  on Bazzite, a Universal Blue image derived from Fedora." Not "Bazzite
  Cairn", not "Fedora Cairn".
- [ ] **Say plainly that Cairn is not affiliated with or supported by**
  Bazzite, Universal Blue, Red Hat or the Fedora Project. Put it in the
  README, the website footer, the About screen, and `NOTICE`.
- [ ] **Own support channels only.** Cairn's docs and UI never point users at
  Bazzite's or Universal Blue's Discord, forums or issue trackers. This was
  the substance of the GPD complaint.
- [ ] **Rewrite `/usr/lib/os-release`** in the Containerfile the way
  Bazzite's `image-info` script does: `NAME`, `ID=cairn`, `ID_LIKE`,
  `PRETTY_NAME`, `LOGO`, `HOME_URL`, `SUPPORT_URL`, `BUG_REPORT_URL`,
  `CPE_NAME`, and drop the `REDHAT_*` keys.
- [ ] **Swap `fedora-logos` for a `cairn-logos` package** (or `generic-logos`
  as Bluefin does) so no Fedora artwork ships. Replace or remove
  `fedora-release-notes`. `fedora-release` provides files many packages
  depend on; follow Bazzite's practice of rewriting its `os-release` in place
  rather than removing the package, and record that choice in the ADR.
- [ ] **Display the Fedora notice** where the OS identifies itself (About
  screen, website download page, docs): not provided or supported by the
  Fedora Project; official Fedora is at https://fedoraproject.org/.
- [ ] **Installer branding.** Bazzite issue #1104 shows the installer can
  carry Fedora imagery unless replaced. Cairn's ISO (P1-4) must ship its own
  installer artwork.
- [ ] **`NOTICE`:** add Bazzite and Universal Blue attribution (Apache-2.0)
  when the image first builds on them.
- [ ] **Optional courtesy:** email press@bazzite.gg once there is something to
  show, so the name appears nowhere they would object to. Not required.

## Still open on issue #22

Scratch Foundation branding guidelines, and the "Cairn" trademark search in
software classes. Neither concerns the base image.
