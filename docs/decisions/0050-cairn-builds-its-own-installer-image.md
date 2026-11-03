# ADR-0050: Cairn builds its own installer image

**Status:** proposed. Mason decided on 2026-11-02 that Cairn builds its
own installer image, as Bazzite does (PR #259, choice 7), and that the
installer queues Universal Blue's Secure Boot key at install (#260).
The thirteen choices under "Choices for Mason" are still open; the
Decision is written with the recommended answer to each. It becomes
accepted when they are answered and the checks under "Checks in the VM"
have run on an ISO from slices 1 to 4 without overturning a bullet of the
Decision.
**Date:** 2026-11-03
**Closes:** (none) — the plan for issue #260 and the answer to PR #259's
choice 7; part of ROADMAP P1-4 and P1-14. Once accepted, amends ADR-0006
and ADR-0038 (slice 6).

## Context

### What the installer shows today

A grown-up writes Cairn's ISO to a USB stick and starts the computer from
the stick to install Cairn.
ADR-0038's slice 6 makes the ISO with bootc-image-builder's `anaconda-iso`
type and `disk_config/iso.toml`.
That type builds the installer itself, from a list of Fedora packages
written into the builder.
`anaconda` in that list needs a package that provides `system-logos`, and
the package solver picks `fedora-logos`.
The installer's own `os-release` is Fedora's, so Anaconda picks Fedora's
profile and Fedora's stylesheet.

The test ISO of 2026-10-22 showed it (ADR-0038, "Test install"):

- Fedora's logo in Anaconda's side panel, and Fedora's colours on its top
  bar.
- Plymouth's `bgrt` boot splash, with `fedora-logos`' watermark.
- "Cairn Linux 44" in the boot menu and in Anaconda's title, which the
  builder writes from the image's `os-release`.
- The volume label `Container-Installer-x86_64`, the builder's name for an
  `ID` it does not know.

Nothing in the builder's settings chooses the installer's artwork, and a
kickstart runs too late to change it.
That was the finding of the research for PR #259 on 2026-11-02.

### What ADR-0006 asks

ADR-0006 lists "ship our own installer artwork" among the compliance work
that blocks publishing any image (P1-14).
`docs/research/base-image-policy.md` says the same of Cairn's ISO, since
Bazzite issue #1104 showed that an installer can carry Fedora's imagery
unless it is replaced.
Fedora's guidelines ask a system built from Fedora to drop `fedora-logos`
and show no Fedora artwork.
The image did that in slice 5; the installer did not.

### What Mason decided about the art

PR #259 laid out two routes (its choice 7):

- Lend the pinned builder a copy of its own package lists, through an
  experimental switch, with `generic-logos` in place of `fedora-logos`.
  That gives Fedora's trademark-free placeholder art.
  A `product.img` with Cairn's pictures, added to the finished ISO, would
  then put Cairn's art over it.
- Build the installer from an image Cairn makes, as Bazzite does.

On 2026-11-02 Mason chose the second: a second Containerfile with
Anaconda and a Cairn profile, no Fedora art and no experimental switch,
in a change of its own with an ADR that changes ADR-0038's slice 6.
There is no generic-art step before it.
ADR-0038 already names the cost, a second image kept just for the ISO
(corrected on 2026-11-02, #259).

### Secure Boot (#260)

The installer is expected to start on a computer with Secure Boot on
(check 1).
Its shim is signed by Microsoft, and its boot loader and kernel by Fedora
(the test ISO's `BOOTX64.EFI`, `grubx64.efi` and `vmlinuz`, read on
2026-11-03).
The computer it installs runs Bazzite's kernel, which only Universal
Blue's keys sign.
Fedora's shim does not trust those keys until one is enrolled in its list
of extra keys, so an installed computer with Secure Boot on is expected
to stop at "bad shim signature" (ADR-0038, slice 6).
Most laptops sold with Windows 8 or later have Secure Boot on.
A friend would watch the install finish, and then the computer would not
start.
Nobody has seen either the start or the stop: both VMs run with Secure
Boot off.

Issue #260 offered three ways: queue the key at install, as Bazzite's
installer does; ask friends to turn Secure Boot off; or run a kernel the
computer already trusts.
On 2026-11-02 Mason chose the first.
A step in the installer's kickstart runs `mokutil --import` on computers
started with EFI, with a password Cairn's install notes give.
The notes show the blue screen a grown-up sees once, at the first start.
It is checked in a VM with Secure Boot on before the first ISO goes to a
friend.

### What the research found

Read on 2026-11-03 in image-builder at v85.0.0, the pinned builder's, and
at v89.0.0, the newest; Anaconda 44.30, the test ISO's; Bazzite's
installer at 59f518b; lorax at 30890c3; mokutil 0.7.2; shim 16.1's
MokManager; Fedora 44's spec files; the test ISO's boot files; and the
images themselves.
Paths below are in those repositories at those versions.

- Three tools turn an installer image into an ISO: image-builder's
  `bootc-generic-iso` (`generic-iso` from v89), bootc-image-builder's
  `bootc-installer`, and titanoboa, the script Bazzite runs.
  Upstream files `bootc-installer` under "Historical" and says "You
  should prefer using `bootc-generic-iso`"
  (`doc/20-advanced/20-bootc/10-isos.md:124-128` at v85).
  The pinned builder's container carries image-builder 85.0.0, which
  builds `bootc-generic-iso`.
- Upstream's definitions call `anaconda-iso` a type "we want to get rid
  of" (`data/distrodefs/bootc-generic/imagetypes.yaml:195-201` at v85).
  From v89 it installs from repositories built into the builder, not the
  image's (`doc/20-advanced/20-bootc/20-legacy-isos.md` at v89), so even
  today's route changes at the next pin move.
- An installer built on Cairn's own image would itself stop at "bad shim
  signature", since its kernel is Bazzite's.
  Bazzite puts Fedora's signed kernel into its live ISO for that reason
  (`installer/titanoboa_hook_preinitramfs.sh:5-23`), and its ISO carries
  the system twice.
- Fedora's own bootc image, `quay.io/fedora/fedora-bootc:44`, has the
  signed chain and `mokutil`, and no logos package.
  In a dry run with Fedora 44's repositories, Anaconda goes on it with
  `generic-logos` named and no `fedora-logos`; with no logos package
  named, the solver picks `fedora-logos`.
- The image already carries Universal Blue's key, the certificate that
  made one of the two signatures on its kernel.
  It is the same file Bazzite's installer downloads.

## Decision

**Cairn's installer is an image of its own, built on Fedora's bootc
image, with Anaconda, `generic-logos` and Cairn's art. The pinned
builder's `image-builder` makes it into the ISO, with its generic ISO
type and the image to install inside it. lorax's `mkksiso` adds the
kickstart to the finished ISO: the family's, or the private test one. On
a computer started with EFI, the kickstart queues Universal Blue's key,
which the build takes from the image being installed and checks against
a pinned fingerprint. A grown-up enrolls it on a blue screen, once, at
the first start, with a password the install notes give.**

Where a part rests on one of the open choices, it names it.

### The installer image

- `installer/Containerfile` starts from `quay.io/fedora/fedora-bootc:44`,
  pinned by digest (choice 3).
  The digest sits beside the builder's pin in `cairn.env`, because the
  builder, this base and the image's Fedora release move together, and
  `build-iso.sh` passes it to the build.
- It is built without root, after the image, as
  `localhost/cairn-installer:dev`.
  It exists only to make the ISO: it runs from the stick and is never
  installed.
- It adds the packages upstream's example names
  (`10-isos.md:26-43` at v85): Anaconda with its boot-image dependencies
  and its dracut module, `dracut-config-generic`, `net-tools`,
  `grub2-efi-x64-cdboot`, Plymouth, and fonts for every language Anaconda
  offers.
  It adds the tools the generic type needs inside the installer image,
  `xorriso`, `isomd5sum` and `squashfs-tools` (`10-isos.md:12` at v85).
  For the stone splash it adds `plymouth-plugin-script` and the brand's
  typeface, as the image's boot screen does (ADR-0044).
- It names `generic-logos`, which gives Anaconda the `system-logos` it
  needs.
  Named, it keeps `fedora-logos` out: the dry run of 2026-11-03 put in
  `generic-logos-18.0.0-27.fc44`, and with no logos package named it put
  in `fedora-logos-42.0.1-3.fc44`.
  The build stops unless `rpm -q generic-logos` succeeds and
  `rpm -q fedora-logos` fails, as `check-name-and-logos.sh` does for the
  image.
- It does by hand what lorax's templates do for Fedora's own installer,
  about ten short steps in upstream's example (`10-isos.md:72-108` at
  v85): the logins on the installer's consoles, `anaconda.target` as the
  default, a shell on the second console, and an initramfs built with
  Anaconda's dracut module and the live-boot modules it needs.
- Its `os-release` says Cairn, as the image's does: `NAME="Cairn Linux"`,
  `ID=cairn`, `ID_LIKE=fedora`, `VERSION_ID=44`.
  Anaconda still reads kickstarts as Fedora 44's, because it takes the
  version from the first `ID_LIKE`
  (`pyanaconda/core/kickstart/version.py:46-76`).
- It writes its own `/.buildstamp`, which names the product in Anaconda's
  title.
  The generic type does not write one, and without it Anaconda calls the
  product "anaconda" (`pyanaconda/core/product.py:83-85`).
- `lorax`, which carries `mkksiso`, and `sbsigntools`, which checks the
  kernel's signature, stay off the stick.
  They go in a build-only stage of the same Containerfile, which
  `build-iso.sh` tags as an image of its own,
  `localhost/cairn-installer-tools:dev`, and uses only while the ISO is
  made.
  The dry run behind the size estimate below did not include them.

### Cairn's art

- **Anaconda's profile.**
  `/etc/anaconda/profile.d/cairn.conf` matches `os_id = cairn`
  (`pyanaconda/core/configuration/profile.py:171-198`) and builds on
  Fedora's (`base_profile = fedora`).
  So Fedora's storage defaults stay, btrfs among them, which each child's
  game copies need (ADR-0033).
  It names Cairn's stylesheet.
  It carries the module lists `iso.toml` holds today, so the installer
  still offers language and keyboard, and still offers no account.
- **The stylesheet and the mark.**
  Anaconda draws its side panel from three style classes,
  `logo-sidebar`, `logo` and `product-logo`, and its top bar from a box
  named `nav-box`
  (`widgets/src/BaseStandalone.c:257-271`, `widgets/src/BaseWindow.c:293`).
  Cairn's stylesheet paints them Ink, with the mark's stones in Sand on
  the side panel and Sand words on the top bar: `semantic.dark-ground` and
  `semantic.on.ink`, the login screen's colours.
  That is one picture, the mark in Sand, and two colours; there are no
  gradients, so there are no background pictures.
  Anaconda loads the stylesheet over its own
  (`pyanaconda/ui/gui/__init__.py:741-750`).
  Both files sit in `/usr/share/cairn/installer/`, and `generic-logos`'
  files stay as packaged.
- **Not a reskin.**
  Anaconda's profiles and product stylesheets exist so that a system puts
  its own name and art in the installer, as Fedora's does.
  Cairn changes only what Fedora's stylesheet changes, and keeps
  Anaconda's layout, words and font (choice 8).
- **Drawn from the tokens.**
  A module beside `brand/boot_screen.py`, such as
  `brand/installer_art.py`, draws the mark's picture and writes the
  stylesheet from `brand/tokens.json`.
  `build.py` calls it, its output is committed in
  `brand/image/installer/`, and `build.py --check` compares it as it does
  the boot screen's, a PNG by its pixels.
  The installer's Containerfile copies that folder, as the image's copies
  `brand/image`.
- **The boot splash.**
  The image's stone theme, `brand/image/plymouth/`, is the installer's
  default too, in its initramfs, so the stick shows the stones building
  from the boot menu until Anaconda opens (choice 8).
- **The boot menu and the stick's name.**
  `/usr/lib/image-builder/bootc/iso.yaml` in the installer image holds
  the volume label and every menu entry
  (`pkg/bib/osinfo/osinfo.go:254-296` at v85).
  Cairn's entries replace the builder's own, in the BIOS menu and in the
  EFI menu (`pkg/manifest/iso_tree.go:225-237` and
  `pkg/manifest/efi_boot_tree.go:107-117` at v85), so Cairn writes three:
  install; check this stick, then install (`rd.live.check`); and install
  with basic graphics (`nomodeset`).
  Plymouth draws only with `rhgb quiet` on the boot line.
  v89 adds both to its own kernel options, and the plan slice 2 reads
  must show them on every entry.
  The label is what a computer shows when the stick is plugged in, such
  as `Cairn-Linux-44-x86_64`, within ISO 9660's 32 characters (choice 8).
  The menu is text only, and English only, since it comes before anyone
  picks a language, as the boot screen's words are (ADR-0044).
- **The title.**
  `/.buildstamp` says `Cairn Linux` and `44`, so Anaconda says "WELCOME
  TO CAIRN LINUX 44.", as now (choice 8).

### How the ISO is made

`tools/build-iso.sh` still runs once under `sudo`, since the builder
still needs root:

1. It builds both targets of `installer/Containerfile` as the person who
   ran `sudo`: the installer image, and the build-only stage as
   `localhost/cairn-installer-tools:dev`.
2. It copies three images into root's storage, the image, the installer
   image and the build-only stage, each with today's check that root's
   copy has the same ID.
3. It runs the pinned container's `image-builder` with the generic type
   (choice 1), the installer image as `--bootc-ref` and the image as
   `--bootc-installer-payload-ref`.
   The builder squashes the installer into the ISO, with a copy of the
   image in the installer's own container storage (`10-isos.md:17` at
   v85).
   It reads no config.
   The generic type takes no blueprint
   (`pkg/distro/defs/bootc_imagetype.go:528-614` at v85), so the
   builder's own kickstart, the one that wipes every disk without asking,
   can never be written.
4. It adds the kickstart with `mkksiso` (choice 2), as root, in a
   privileged container of the build-only stage: `disk_config/cairn.ks`
   for the family's ISO, into `output/`, or a private kickstart for a
   test ISO, into `output/test/`.
   `mkksiso` puts the file on the ISO's root, adds
   `inst.ks=hd:LABEL=<label>:/cairn.ks` to every boot entry, rebuilds the
   EFI boot image, and implants the media check's checksum again
   (`src/pylorax/cmdline/mkksiso.py:434-548` in lorax at 30890c3).
   Upstream's own boot test adds its kickstart to this ISO type the same
   way (`test/scripts/imgtestlib/boot.py:299-308` at v85).
5. It checks the family's ISO (below) and writes the `.sha256`, as now.
   Its last lines name the ISO, the Cairn version and commit from the
   image's labels, as now (#259), and the key's two fingerprints
   (below).

- The ISO step downloads nothing: the installer's packages come with its
  image.
  The loan of the image's signing keys to the builder goes, and the step
  may run with no network at all (not tried).
- `disk_config/iso.toml` becomes `disk_config/cairn.ks`, a plain
  kickstart, since nothing reads a blueprint any more.
  Its module lists move into the profile.
- The builder pin moves to the newest image-builder release, after
  reading its notes (choice 7).
  Slice 2 pins it and plans an ISO with it, and slice 3 builds with it.
  The generic type's contract moved between v85 and v89.
  v89 puts `root=live:CDLABEL=<label> rd.live.image quiet rhgb
  enforcing=0` in the kernel options itself
  (`pkg/image/container_based_iso.go:72-80` at v89).
  It needs dracut's `dmsquash-live` module in the installer's initramfs
  (`10-isos.md:8` at v89), from `dracut-live`, which the dry run already
  pulled in.
  Its menu lines name the kernel, the root and the initramfs as
  `${kernelpath}`, `${root}` and `${initrdpath}` (`10-isos.md:111-119`
  at v89), where v85's are written whole (`10-isos.md:55-62` at v85).
  So the installer is written once, against the contract upstream now
  documents.
- No kickstart is ever inside an image.
  That is so today too: the builder puts its kickstart on the ISO's root.

### The kickstart

`disk_config/cairn.ks` holds what `iso.toml`'s kickstart and the
builder's own say together today, and #260's step:

- `network --hostname=cairn`, as now (ADR-0038, "The hostname").
- The payload, from the copy in the installer's container storage, the
  way Bazzite's installer installs its own
  (`installer/titanoboa_hook_postrootfs.sh:109`):
  `ostreecontainer --url=localhost/cairn:dev
  --transport=containers-storage --no-signature-verification`.
  Anaconda 44.30 passes the transport through
  (`pyanaconda/modules/payloads/payload/rpm_ostree/installation.py:941-942`).
- The builder's switch, in `%post`:
  `bootc switch --mutate-in-place --transport registry localhost/cairn:dev`,
  which the builder writes today (ADR-0038, "Updates on an installed
  computer"), until P1-3 puts the image in a registry.
- The key step, after Bazzite's
  (`installer/titanoboa_hook_postrootfs.sh:126-153`), is a plain
  `%post --nochroot`, with no `--erroronfail` and no `set -e` or
  `set -u`, so it never stops an install.
  Anaconda stops only for a script marked `--erroronfail` that fails
  (`pyanaconda/kickstart.py:79`).
  Bazzite's step carries `--erroronfail` and `set -oue pipefail`
  (`installer/titanoboa_hook_postrootfs.sh:128-129`); Cairn's leaves both
  out.
  - It does nothing on a computer not started with EFI, where
    `/sys/firmware/efi` is missing.
    ADR-0003 supports only UEFI, but such an install must still finish.
  - `mokutil --timeout -1` makes the blue screen wait for the grown-up,
    instead of going on by itself after a few seconds.
  - The password goes twice to `mokutil --import`, on its standard input,
    with the key.
  - Every command also ends with `|| true`, a second guard.
    A second install on the same computer is told the key "is already
    enrolled" or "is already in the enrollment request"
    (`src/mokutil.c:815-824` in mokutil 0.7.2).
  - It writes a log, which the VM checks read before the restart.
  - Bazzite also skips Steam Decks.
    Cairn is for laptops (ADR-0003), so its step does not.
- No `user`, `sshkey`, `rootpw` or `group` line, as now.

The password is not a secret.
It only lets the blue screen tell that the person at the keyboard is the
person who asked for the key.
Someone with root on a running computer could queue a key with a
password of their own, so publishing Cairn's weakens nothing against
them.
It could help someone who hopes a grown-up types the familiar password at
a blue screen that comes at some other time.
So the notes say the screen comes once, after installing, and say to
choose "Continue boot" at any other time.
They also give what "View key" must show.
Anyone who makes a certificate can give it any name, so the name is only
a second cue.
The check is the fingerprint: MokManager shows the certificate's SHA-1
under "[Fingerprint]", in two lines of ten pairs
(`MokManager.c:373-376, 440-446, 571` in shim 16.1), and the notes give
the same pairs.
The password is in the kickstart and in the notes, both public
(choice 4).

### Universal Blue's key

- **Where it comes from.**
  The build-only stage copies `/etc/pki/akmods/certs/akmods-ublue.der`
  and the kernel from the image being installed, and checks them (below).
  The installer image's last build step takes the key from that stage
  into `/usr/share/cairn/installer/`, so the installer cannot be built
  without the checks (choice 5).
  The image has the key from `ublue-os-akmods-addons` (MIT).
  On 2026-11-03 it is the same certificate as ublue-os/akmods'
  `certs/public_key.der` at 6d6d70c, which Bazzite's installer downloads
  from `main`, unpinned, at every build
  (`installer/titanoboa_hook_postrootfs.sh:23, 33-35`).
- **The pin.**
  Its SHA-256 is kept in `cairn.env`:
  `4e5c68474cb133fd8984d9599762cece9100c3e6cd8a9709aeaabd85dd9e70d1`,
  subject "O=Universal Blue, OU=kernel signing, CN=ublue kernel", valid
  from 2024-07-11 to 2124-06-17.
  The build stops if the copied key differs, or if no pin is given.
  Its SHA-1 fingerprint is
  `2B:E9:91:E3:B1:B5:40:70:F4:3D:80:BB:13:EB:C6:57:E5:A3:78:0D`, which
  the blue screen shows as `2b e9 91 e3 b1 b5 40 70 f4 3d` and
  `80 bb 13 eb c6 57 e5 a3 78 0d`.
  `build-iso.sh` prints both fingerprints after every build, and the
  install notes give the SHA-1 in the blue screen's form.
- **The kernel's signature.**
  The build also stops unless the image's kernel carries a valid
  signature by that certificate.
  `sbverify --cert`, from Fedora's `sbsigntools`, checks it in the
  build-only stage.
  On 2026-11-03 the kernel, `7.2.11-ogc4.1.fc44`, carries two
  signatures, which `sbverify --list` names: one by this key, and one by
  the older "ublue akmods" key, valid to 2033.
  `sbverify` from `sbsigntools-0.9.5-14.fc44`, run in Fedora's `fedora:44`
  container, passed with each of the two certificates.
  It failed with Fedora's kernel signer, and on a copy of the kernel with
  one byte changed.
- So the key on the stick always belongs to the kernel on the stick.
  If Universal Blue changes its key, the build stops instead of making a
  stick that queues the wrong one, and moving the pin is a decision, like
  moving the builder's.

### The test ISO

- A test ISO is the same installer ISO with the private kickstart in
  place of the family's, written to `output/test/`, as now.
- The private kickstart is the family's file with the test Guardian added
  in `%post` lines: the five groups with their fixed numbers, a Guardian
  in `wheel` and `cairn-guardian`, the dev PC's ssh key, and `sshd` turned
  on (choice 6).
  So both ISOs share one installer configuration, and a test install also
  walks the family's path with no account screen.
- The private kickstart stays on the dev PC, never committed or shipped
  (ADR-0038, "A first account").
- `build-iso.sh` checks every family ISO: its `/cairn.ks` must be the
  checkout's `disk_config/cairn.ks`, byte for byte, and that file has no
  line that makes an account or adds a key.
  Otherwise nothing is written to `output/`.

### Nothing phones home

- The ISO step downloads nothing.
  The installer image's packages come from Fedora's repositories once,
  when `podman build` makes it, as the image's do.
- The key comes from the image being installed, never from the internet.
- Anaconda's geolocation asks `geoip.fedoraproject.org` for the
  computer's time zone and language (`data/anaconda.conf:333-349`).
  An install with `inst.ks` never starts it
  (`pyanaconda/startup_utils.py:717-728`), and `mkksiso` puts `inst.ks`
  on every entry.
  The profile also leaves `geolocation_provider` empty, which Anaconda's
  own settings say turns the lookup off, unless someone types a provider
  on the boot line.
  Bazzite's `use_geolocation = False` is not enough on its own: it keeps
  the answer from choosing the language, but the lookup still runs and
  sets the time zone (`pyanaconda/startup_utils.py:760-812`;
  `pyanaconda/modules/timezone/initialization.py:41-45`).
- The installer image has no NetworkManager connectivity check: the
  package that sets it, `NetworkManager-config-connectivity-fedora`, is
  in neither Fedora's bootc image nor the dry run.
  The installed image's check is issue #36's.
- A VM check records every address the installer reaches.

### Where it applies

Only the ISO.
Computers already installed, the VMs and the kids' laptop do not change.
A computer installed without the step is expected not to start with
Secure Boot on, so a grown-up first turns Secure Boot off in the
firmware.
Then the image's own `ujust enroll-secure-boot-key`, Bazzite's recipe,
queues the key with the password `universalblue`, and Secure Boot goes
back on after the blue screen, as Bazzite's guide says.

## Slices

Each is a change of its own, smallest first.
No ISO goes to anyone outside the family before slice 5, and then only
once P1-14, the Guardian audit (#86) and child test 2 (P1-17) have passed
(ADR-0031).

1. **The art.**
   `brand/installer_art.py` draws the mark's picture and writes the
   stylesheet from the tokens into `brand/image/installer/`, with tests,
   and `build.py --check` covers both.
   Why first: either builder type would use it, and it needs no root and
   no image.
2. **The installer image.**
   `installer/Containerfile` with the profile, `os-release`,
   `/.buildstamp`, `iso.yaml` and the splash, and the build's checks:
   `generic-logos` in, `fedora-logos` out, the profile and the stylesheet
   where Anaconda reads them, the stone theme the default, and Anaconda's
   and the live-boot modules in the initramfs.
   Built without root and read with `podman run`; its size measured.
   The builder release of choice 7 is pinned in `cairn.env` beside
   `BIB_IMAGE`, which today's ISO keeps until slice 3.
   Its `manifest` step, which plans a build without making it, runs
   without root against the installer image, as ADR-0038's did for
   `iso.toml` (not yet tried for this type).
   The plan must hold Cairn's label, Cairn's three menu entries and no
   others, and `rhgb quiet` on each, so `iso.yaml` is checked against the
   release it is written for.
   Why alone: it changes no ISO, and a mistake here costs no root build.
3. **The ISO.**
   `tools/build-iso.sh` builds both targets, copies three images into
   root's storage, runs the generic type with slice 2's pin in place of
   `BIB_IMAGE`, and adds `disk_config/cairn.ks` with `mkksiso`.
   `iso.toml` goes, and so does the signing-key loan.
   A test ISO comes from the private kickstart.
   Checked in a VM with Secure Boot off, as today, in an order that ties
   a failure to one change.
   First the builder's own ISO, before `mkksiso` touches it, started with
   the VM's network off: it must reach Anaconda with Cairn's label, menu
   and art (check 2), or the fault is the builder's or the installer
   image's.
   Then `mkksiso`: whether it runs in a privileged container as it does
   on a host is the first thing to see.
   Then its ISO: checks 3, 4, 11 and 14, and check 7's list but for
   Secure Boot.
   Why before the key: the new way of making the ISO is the larger risk,
   and it can be seen working without Secure Boot.
4. **The key.**
   The kickstart's step, the key copied from the image, the pinned
   SHA-256, the check of the kernel's signature, the two fingerprints
   `build-iso.sh` prints, and the password (choice 4).
   Checked in a VM with Secure Boot on: every check below.
   Each blue screen is captured for the notes.
5. **The install notes** (choice 9): the Minimum and Recommended panel
   (choice 13), how to start from the stick, the password, the blue
   screen step by step with slice 4's captures, the key's name and
   fingerprint the screen must show, and what to do if it comes at any
   other time.
   Checked when a grown-up who did not write them, such as Mason, follows
   them on the Secure Boot VM, from the stick's first start to Cairn's
   login screen, with nothing else to go on.
   A friend's ISO goes with them.

## Checks in the VM

In a new VM, not `cairn-min` or `cairn-iso`, which run with Secure Boot
off and hold other work.
Its firmware has Secure Boot on and Microsoft's keys: the dev PC's
`/usr/share/qemu/firmware/30-edk2-ovmf-4m-qcow2-x64-sb-enrolled.json`
(edk2-ovmf-20260812-8.fc44), whose `db` holds Microsoft's UEFI CA 2011
and 2023.
`virt-install` is called as in `docs/DEVELOPMENT.md`, with both firmware
features turned on:

```
--boot uefi,firmware.feature0.name=secure-boot,firmware.feature0.enabled=yes,firmware.feature1.name=enrolled-keys,firmware.feature1.enabled=yes
```

The family's ISO has no account, and the image ships `sshd` off, so a
computer installed from it cannot be logged in to.
Checks that need a login run on the test ISO, which is the family's
installer with the test Guardian added (choice 6).
Check 4's path with no account, and check 14, run on the family's ISO.

### With Secure Boot on and Microsoft's keys

1. **The installer starts.**
   No "bad shim signature".
   On the installer's second console, `mokutil --sb-state` says Secure
   Boot is enabled.
2. **Cairn's art everywhere, and no Fedora art.**
   The boot menu has Cairn's words, and the stones build before Anaconda
   opens (choice 8).
   Anaconda's side panel and top bar are Ink, with the Sand mark, and its
   title says Cairn Linux.
   `anaconda.log` says the `cairn` profile was found, and no stylesheet
   failed to load.
   `blkid` shows Cairn's label on the stick.
   No picture from `fedora-logos` or `generic-logos` shows anywhere.
3. **The media check** passes, from the "check this stick" entry, so
   `mkksiso`'s checksum is right.
4. **The install** stops for language, disk, keyboard and time, offers
   no account (the family's ISO), and finishes.
   A VM with 4,096 MB, ADR-0003's floor, installs too.
   ostree needs room in `/var/tmp`, which in a live installer is memory;
   Bazzite gives its installer a larger one for that reason
   (`installer/build.sh:91-111`).
5. **The key is queued.**
   Before the restart, on the second console: the step's log shows the
   import, and `mokutil --list-new` lists the key, SHA-256
   `4e5c…70d1`.
6. **The blue screen, once.**
   At the first start the blue screen waits.
   "Enroll MOK", "View key", "Continue", "Yes", the password, "Reboot".
   "View key" must show the fingerprint `2b e9 91 e3 b1 b5 40 70 f4 3d`
   and `80 bb 13 eb c6 57 e5 a3 78 0d`, the one `build-iso.sh` printed,
   and name Universal Blue's "ublue kernel".
   Each screen is captured with `virsh screenshot` for the notes.
   At the next start it does not come back.
7. **The installed computer starts with Secure Boot on.**
   Cairn's login screen comes.
   `mokutil --sb-state` says enabled, and `mokutil --list-enrolled` lists
   the key.
   `systemctl --failed` is empty.
   Upstream warns that a bootc system installed through Anaconda fails
   `systemd-remount-fs.service` (`10-isos.md:118-119` at v85); the test
   install of 2026-10-22 did not.
   The computer is called `cairn`, the groups are 461 to 465, and
   `bootc status` is as before.
   `restorecon -nvR /etc /var` lists nothing, and SELinux denies nothing.
   Also the two items slice 6 left: `efibootmgr`'s entry name, and the
   fallback entry's "Fedora".
8. **"Continue boot" instead of enrolling.**
   What the grown-up sees next, and whether the request comes back at the
   next start.
9. **A wrong password, three times.**
   What the screen does then, for the notes.
10. **A second install on the same VM.**
    The step says the key is already enrolled, and the install finishes.
11. **Nothing phones home.**
    The VM's traffic, captured on the host from power-on to the end of
    the install, has no request to `geoip.fedoraproject.org` or to
    Fedora's connectivity check.
    Every other address the installer reached is written into this ADR.

### With Secure Boot off

12. **The blue screen still comes once.**
    Enrolling does no harm, and "Continue boot" works too.
    With the key enrolled, then Secure Boot turned on in the firmware,
    the computer still starts, as after a firmware update that turns it
    back on and keeps the firmware's variables.
13. **A legacy BIOS VM.**
    Not supported in v1 (ADR-0003), but the step skips itself and the
    install finishes.

### The test config never in a published ISO

14. The family's ISO holds `/cairn.ks` equal to the checkout's
    `disk_config/cairn.ks`, with no account line.
    `build-iso.sh` stops when it is handed a changed file, and writes
    nothing to `output/`.
    A test ISO goes only to `output/test/`, and its test Guardian logs in
    over ssh, as on 2026-10-22.

What the VM cannot show: its firmware trusts both of Microsoft's
certificates.
A real computer may trust only the 2011 one, which Fedora's shim's two
signatures cover, or have Microsoft's third-party certificate turned off,
which needs a firmware setting for any system that starts through shim.
P1-4's installs on both hardware tiers are where real computers show it.

## Consequences

- A friend's computer shows Cairn's art from the stick's name to the end
  of the install, and starts with Secure Boot on after one blue screen.
- **The blue screen is the hard part.**
  It is in English only, reads every keyboard as a US one, and shows
  nothing while the password is typed.
  It comes before the first-boot wizard (P1-6), so on a computer started
  with EFI it, not the wizard, is the first thing a grown-up sees after
  installing.
  DESIGN §4.3.1 says a step the wizard cannot do is not ready to ship;
  this one cannot move into the wizard, Mason accepted it for #260, and
  this change names it in §4.3.1 as the one exception (choice 11).
  The install notes carry it.
  Bazzite's installer also shows a window with a link to its guide at the
  end of the install; that installer runs in a live desktop, which
  Cairn's does not have, so Cairn's notes do that job.
- A firmware update that turns Secure Boot back on later keeps the
  computer starting, as long as it keeps the firmware's variables.
  The enrolled key is one of them, kept under shim's own name
  (`MokListRT`, `src/mokutil.c:819-821` in mokutil 0.7.2).
  An update or a reset that clears them brings back "bad shim
  signature", and "Where it applies" has the way back.
- **Building.**
  The installer image's first build downloads about 1 GB of base and
  0.5 GB of packages, and podman keeps them.
  A package Fedora replaces mid-download then fails a `podman build`,
  which a rerun mends, rather than the ISO step after hours of downloads.
  The ISO step downloads nothing; it should take a few minutes, plus a
  minute or two for `mkksiso` (estimates, measured in slice 3).
  Root keeps three images: 12 GB for the image, an estimated 4 GB for the
  installer, and the build-only stage, which shares the installer's base.
- **Size.**
  An estimated 6.4 to 7.1 GiB: 1.3 to 1.6 GiB of installer and 4.75 to
  5.25 GiB of payload.
  The test ISO of 2026-10-22 was 6.0 GiB, but its payload came before the
  boot screen's initramfs, which has added about 254 MB to the image since
  2026-10-29 (#262, ADR-0044).
  It fits an 8 GB stick (7.45 GiB), with little room left at the top of
  the estimate.
  P1-3's rechunk step may win some of it back; slice 3 measures the ISO.
- **More to keep.**
  A second Containerfile, its profile, `iso.yaml`, the kickstart and the
  art.
  Three pins move together: the builder, `fedora-bootc:44`'s digest and
  the image's Fedora release.
  Bazzite has announced a "clean break" for Bazzite 45 (ublue-os/bazzite
  f50287b).
- **image-template.**
  Its disk workflow still builds `anaconda-iso` with
  `bootc-image-builder:latest`
  (`.github/workflows/build-disk.yml:30, 43` at 72e9b7c).
  ADR-0038 kept `anaconda-iso` "until image-template moves"; that reason
  goes.
  P1-3 still adopts image-template's `Justfile` and `build.yml`, but
  Cairn keeps its own ISO step, and `disk_config/` holds a kickstart, not
  image-template's ISO settings (choice 10).
  Where a friend's ISO is built, on the dev PC or by a workflow, is
  choice 12.
- **The installer's kernel is Fedora's**, while the system runs
  Bazzite's.
  A computer that only Bazzite's kernel supports may not show the
  installer well.
  Bazzite's own installer accepts the same trade.
- **A key change stops the build.**
  Universal Blue has used two keys, and Bazzite took its copies of both
  out of its repository on 2026-10-29 (f50287b).
- **Two passwords**, if choice 4 picks a word of Cairn's: the install
  notes give Cairn's, and the image's `ujust enroll-secure-boot-key` uses
  `universalblue`.
- **Upstream may still move.**
  The generic type was renamed and reshaped within four releases, and
  upstream's CI boot-tests only cloud images of bootc systems
  (`test/scripts/imgtestlib/bootcsource.py:11-15` at v85).
  So the pin moves only on purpose, and every check above runs again when
  it does.
- **Licences.**
  Nothing new in kind goes on the stick: today's installer already
  carries Anaconda, Plymouth, GRUB and shim under the same licences, and
  `fedora-logos` is the one package that must stay out.
  The new files in the repository are Cairn's own: Apache-2.0 for the
  Containerfile, profile, `iso.yaml` and kickstart, and CC BY-SA 4.0 for
  the art.
  Patterns from upstream's example (osbuild/image-builder) and Bazzite's
  installer are Apache-2.0, and `NOTICE` records any that are taken.
  Nothing comes from ondrejbudai/bootc-isos, which Bazzite's build cites
  and which has no licence.
- No new dependency for Cairn's code.
  The build's new tools come in containers: `image-builder` in the pinned
  container, and `mkksiso` and `sbverify` in the installer's build-only
  stage.
- ADR-0003 has the Minimum and Recommended panel appear in the installer
  too (brand v0.2, #20).
  These slices put it in the install notes (slice 5), and the panel
  inside Anaconda waits for a change of its own (choice 13); ROADMAP
  P1-4 lists it as left.
- Once accepted, it amends:
  - **ADR-0006:** the installer is built on Fedora's bootc image, not on
    Bazzite, and shows Cairn's installer art and no Fedora art; the
    system it installs stays Bazzite's, with Bazzite's kernel, so the
    installer queues Universal Blue's Secure Boot key.
  - **ADR-0038, slice 6:** the ISO comes from Cairn's installer image
    through image-builder's generic type, not from `anaconda-iso`.
    `iso.toml` becomes `disk_config/cairn.ks` and an Anaconda profile, and
    the loan of signing keys goes.
    The kickstart queues Universal Blue's key, which answers the slice's
    "Secure Boot" bullet, and the installer's art is Cairn's, which
    answers "What the builder shows".

## Alternatives considered

- **Generic art first, then a `product.img` with Cairn's art** (PR #259's
  research of 2026-11-02): about 15 lines in `build-iso.sh` for the
  experimental switch, then about 25 more and four pictures.
  Not chosen (Mason, 2026-11-02).
  It rests on an experimental switch in a type upstream wants to drop,
  which stops reading the image's repositories at v89.
  A `product.img` leaves `fedora-logos` installed underneath, and whether
  the bootc installer's boot reaches the step that unpacks it was never
  tried.
- **Asking friends to turn Secure Boot off before installing** (#260's
  second way).
  Nothing to build, but it lowers the computer's protection, the steps
  differ on every make, and a firmware update can turn it back on.
  Not chosen (Mason, 2026-11-02).
- **Fedora's kernel in the image** (#260's third way).
  Every computer would start with Secure Boot on, with no blue screen.
  Not chosen: it gives up Bazzite's kernel, with its hardware support and
  the extra modules Universal Blue builds and signs for it, so the system
  would no longer be the Bazzite ADR-0006 chose.
  ADR-0006's fallback, `kinoite-main`, is that larger change.
- **titanoboa, the script Bazzite runs.**
  94 lines on a personal fork's branch, which make UEFI-only ISOs with no
  media checksum (`build_iso.sh:31-35, 83-94` at 7737f47).
  The pinned builder does the same job.
- **CentOS Stream's bootc image as the base.**
  Its Anaconda would read kickstarts as RHEL's, and the templates it runs
  are RHEL's, for a Fedora 44 system.
- **A smaller base built with `bootc-base-imagectl`.**
  Fedora's page says that "the implementation of this command (especially
  the configuration files that it reads) are subject to change"
  (`bootc-base-imagectl.md:56-57` at 4ca40d0), and Anaconda's own
  packages outweigh what it would save.
- **A change upstream to `anaconda-iso`,** so that the installer takes
  the image's own logos package.
  It is a few dozen lines of Go, but upstream calls the type legacy, so a
  change may not be taken.

The other side of each open choice is under "Choices for Mason".

## Edits in the same change

In this change:

- ADR-0006 and ADR-0038 each get an "Amended by: ADR-0050, proposed"
  entry.
  Nothing else in them changes until this ADR is accepted.
- DESIGN §4.1 lists the installer image among the costs accepted, §10
  says how ISOs are made, and §4.3.1 names the blue screen as the one
  step before the wizard (choice 11).
  Each is marked ADR-0050, proposed.
- ROADMAP §0 gets a bullet for this ADR, beside the other image work.
- ROADMAP P1-4 names this ADR and says, marked proposed, that Cairn keeps
  its own ISO step rather than image-template's `build-disk.yml`.
  Its list of what is left keeps an ISO workflow in CI (choice 12) and
  adds the slices, the install notes and the Minimum and Recommended
  panel in the installer (choice 13).
- ROADMAP P1-14 names this ADR, and PR #259's choice 7 as answered by it.
- `docs/DEVELOPMENT.md`'s "Building the installer" says that this ADR
  replaces the steps it describes, which stay true until slice 3.

With the slices:

- Slice 1: `brand/README.md`'s "Regenerating" names the new outputs.
- Slice 2: `README.md`'s layout table names `installer/`, and `NOTICE`
  records any pattern taken from upstream's example or Bazzite's
  installer.
- Slice 3: `docs/DEVELOPMENT.md`'s "Building the installer" and
  `disk_config/README.md` describe the new steps, ADR-0038's slice 6
  points here, and ROADMAP P1-3 says Cairn keeps its own ISO step.
  CLAUDE.md's "Layout" names `installer/` and Cairn's kickstart in
  `disk_config/`, if Mason says yes to choice 10.
- Slice 4: `docs/DEVELOPMENT.md` gains the Secure Boot VM, and ROADMAP
  P1-4 records the checks.
- Slice 5: the install notes; `docs/research/base-image-policy.md` ticks
  "Installer branding", and ROADMAP P1-14 moves the installer's art to
  done.

## Choices for Mason

Each has a recommendation, why, and the other way.
The Decision above is written with each recommendation.

1. **Which builder type.**
   Recommended: `bootc-generic-iso` (`generic-iso` from v89).
   Why: upstream says to prefer it, it is the contract Bazzite's installer
   follows, only it lets Cairn word every menu entry, and it can never
   write the builder's disk-wiping kickstart.
   The other: `bootc-installer`, the smaller step from today.
   `iso.toml` and the private test config stay as they are, and the
   builder runs lorax's templates and dracut itself.
   But upstream files it under "Historical", it starts Anaconda in text
   mode unless `inst.graphical` is added to the boot line
   (`pkg/image/anaconda_container_installer.go:110-121` at v85; read, not
   tried), and upstream skips its install test.
   The builder names its stick `Container-Installer-x86_64`, but the same
   `mkksiso` step could rename it with `--volid`, which also changes the
   name in every boot entry (`mkksiso.py:464-503` at 30890c3; not tried).
2. **Where the kickstart lives.**
   Recommended: added to the finished ISO with `mkksiso`.
   Why: no kickstart is ever inside an image, and a test ISO is the same
   installer with another file.
   The other: `/usr/share/anaconda/interactive-defaults.ks` inside the
   installer image, as Bazzite and upstream's example do.
   It is simpler, but Anaconda 44.30 marks the file deprecated
   (`data/interactive-defaults.ks:1-8`), skips it whenever `inst.ks` is
   given, and a test ISO would need a test installer image.
   Without `inst.ks`, Anaconda also counts the install as interactive and
   starts its geolocation lookup, which only the profile's empty provider
   would then stop.
3. **What the installer is built from.**
   Recommended: `quay.io/fedora/fedora-bootc:44`, pinned by digest.
   Why: its boot chain is signed by Microsoft and Fedora, so the
   installer is expected to start with Secure Boot on (check 1), it has
   no logos package, and the system is not on the stick twice.
   The other: Cairn's own image, as Bazzite builds its installer on
   Bazzite.
   Mason's "as Bazzite does" was about building an installer image; this
   choice is what that image starts from.
   Cairn's image would need Fedora's signed kernel swapped in, as Bazzite
   does, and the stick would carry the system twice, about 12 GB each
   before compression.
4. **The password on the blue screen.**
   Recommended: a short lowercase word of Cairn's that uses none of a, q,
   w, z, y or m, such as `stones`.
   Why: the blue screen reads every keyboard as a US one, and those
   letters sit elsewhere on French and German keyboards.
   The other: `universalblue`, which matches Bazzite's guide and the
   image's `ujust enroll-secure-boot-key`, but has two a's.
5. **Where the key comes from.**
   Recommended: from the image being installed, checked against a pinned
   SHA-256 and against the kernel's own signature.
   Why: the key always belongs to the kernel on the stick, and a change
   upstream stops the build.
   The others: the `.der` file kept in the repository, which is simple
   but tied to no kernel; or ublue-os/akmods' `certs/public_key.der`
   fetched at a pinned commit, which is the file Bazzite fetches unpinned,
   and a download at every build.
6. **How a test ISO gets its test Guardian.**
   Recommended: `%post` lines in the private kickstart.
   Why: one installer configuration for both ISOs, and the test install
   also walks the family's path with no account screen.
   The other: a `cairn-test` Anaconda profile in the installer image that
   turns the Users module on, chosen with `inst.profile=cairn-test` on the
   test ISO's boot line.
7. **Which builder version.**
   Recommended: the newest image-builder release (v89 on 2026-11-03),
   after reading its notes, in the container they name.
   Slice 2 pins it and plans an ISO with it; slice 3 builds with it.
   Why: the generic type changed between v85 and v89, and upstream's docs
   and example now describe v89, so the installer is written once against
   the contract upstream keeps.
   v89's notes name `ghcr.io/osbuild/image-builder-cli` as the container
   for the `image-builder` command
   (`doc/20-advanced/20-bootc/50-migration.md:11-13`).
   The other: stay on v85, where the type is `bootc-generic-iso` and each
   menu line is written whole, and rewrite `iso.yaml` at the next move.
8. **What the installer shows.**
   Recommended: the label `Cairn-Linux-44-x86_64`; the menu entries
   "Install Cairn Linux", "Check this stick, then install Cairn Linux"
   and "Install Cairn Linux with basic graphics"; "Cairn Linux 44" in the
   title, as now, since it matches `os-release`; the stones at boot; and
   Anaconda's own font.
   Why the font stays: Cantarell is Anaconda's own
   (`pyanaconda/ui/gui/__init__.py:722-724`), another font may not fit
   its labels (not tried), and CLAUDE.md asks that third-party apps keep
   their own look.
   The mark and the colours are what Anaconda's profiles are made for.
   The others: other words or another label; Cairn's own version in the
   title once P1-3 numbers releases; Anaconda's default splash; or
   Atkinson Hyperlegible Next in Anaconda.
9. **Where the install notes live.**
   Recommended: `docs/INSTALL.md` in this repository, written in slice 5
   with the VM's captures.
   Why: the password is in `disk_config/cairn.ks` beside it, so one change
   keeps both true, and the website (`cairn-site`) and P1-11's
   documentation can take the page from there.
   The other: the website alone, or a printed sheet that goes with each
   stick.
10. **What CLAUDE.md says about the image's files.**
    Recommended: with slice 3, CLAUDE.md's "Layout" names `installer/`,
    and says that `disk_config/` holds Cairn's kickstart rather than
    image-template's ISO settings.
    The rest of the image's files keep image-template's layout.
    Why: CLAUDE.md says the image's files follow image-template exactly,
    and from slice 3 that is no longer true of `disk_config/`.
    A later session that read only CLAUDE.md would put `iso.toml` back.
    The other: CLAUDE.md stays as it is, and this ADR alone records the
    difference.
11. **When DESIGN §4.3.1 names the blue screen.**
    Recommended: now, in this change, marked proposed, as it stands.
    Why: §4.3.1 says a step the wizard cannot do is not ready to ship.
    The blue screen is such a step, and Mason accepted it for #260, so
    the exception sits beside the rule while the slices are built.
    The other: leave that paragraph out of this change and add it when
    this ADR is accepted, so that DESIGN holds only what is accepted.
12. **Where a friend's ISO is built.**
    Recommended: on the dev PC, with `tools/build-iso.sh` under `sudo`,
    until ADR-0031's gates have passed.
    A CI workflow that runs the same steps can follow once P1-3 puts the
    image in a registry, and ROADMAP P1-4 keeps it on its list.
    Why: the private test kickstart never leaves the dev PC, no ISO
    leaves the family before the gates, and the repository is public, so
    a workflow would first need a private place for its ISO.
    DESIGN §10 still holds: GitHub Actions build and sign the image
    (P1-3), and the ISO comes from upstream's tools, not tooling of
    Cairn's own (§4.1).
    The other: a workflow from slice 3 on, as Bazzite builds its ISOs
    (`.github/workflows/build_iso.yml:130-156` at 59f518b), with its ISO
    kept private; or the dev PC only, with no workflow for the ISO.
13. **The Minimum and Recommended panel.**
    ADR-0003 says it appears on the website, in the installer and in the
    docs, in the same form (#20).
    Recommended: the install notes show it in slice 5, before a friend's
    ISO, and the panel inside Anaconda waits for a change of its own,
    listed as left in ROADMAP P1-4.
    Why: a grown-up needs it before writing the stick, not once the
    installer runs.
    Anaconda 44.30 shows no slides, so inside Anaconda the panel needs an
    add-on, which is Python and GTK, outside ADR-0002, and so needs an
    ADR of its own.
    The other: in these slices, as a picture in Anaconda's side panel,
    whose words could not be translated or read aloud; or later in both
    places, with the website.
