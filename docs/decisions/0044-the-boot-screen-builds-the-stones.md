# ADR-0044: The boot screen builds the stones

**Status:** proposed. It becomes accepted when the boot screen has been
seen on a real computer, and a disk-password prompt on a computer whose
disk is encrypted.
**Date:** 2026-10-29
**Closes:** (none) — issue #165; part of ROADMAP P1-14

## Context

From power-on to the login screen, a computer running the Cairn image
showed Bazzite's boot splash: Fedora's `bgrt` theme, the firmware's logo
over a spinner. A child's first sight of the laptop was someone else's
brand, and P1-14 says no image leaves the family with it.

The initramfs the base ships was built by Bazzite, with Bazzite's
`os-release` in it. Two things stayed Bazzite's because of it (ADR-0038,
slice 5): the computer's name before anyone set one, which the
initramfs's own `initrd-release` gives, and the boot menu titles, which
libostree writes only when the kernel or initramfs changes.

Plymouth is Fedora's boot splash. A theme is a folder of files and one of
Plymouth's modules to draw them. Checked in the image on 2026-10-29:

- The base has Plymouth 24.004.60 with only the `two-step` module.
  Fedora's `plymouth-plugin-script` is one package away.
- `two-step` plays a fixed list of frames at its own rate, and shows the
  prompt systemd sends for a disk password, which names the disk
  ("Please enter passphrase for disk luks-…").
- The `script` module runs a short script in Plymouth's own language.
  It can fade an image in and out, and it draws any words the script
  gives it. It cannot draw a shape, only images, and it cannot translate.
- In the initramfs, Plymouth draws text with its FreeType plugin, which
  reads one font file: whatever `fc-match` named, with no family asked
  for, when dracut ran. For the base that is Noto Sans.
- No SVG renderer is in the build container (`rsvg-convert` is not;
  ImageMagick is, through Bazzite).

## Decision

**The boot screen is a Plymouth theme on the `script` module, drawn by
`brand/build.py` from the tokens, and the image builds its initramfs
again with it.**

- **What it shows.** The launcher's starting screen (ADR-0028), on the
  same Sand: the mark's stones in Ink, three display sizes tall, the
  bottom stone fading in, then each stone above it, one every
  `motion.stone`, the whole mark for a step, then none, again. Nothing
  else: no spinner, no firmware logo, no words. The same at shutdown and
  restart.
- **The disk password.** When the computer asks for it, nothing is
  working while it waits for a person, so the stones stand whole and
  still. Under them one sentence, "Type the password to start this
  computer.", in Atkinson Hyperlegible Next at the heading size (28 px),
  then a box like the login screen's, with a dot for each letter typed,
  and "Caps Lock is on." when it is. systemd's own words are not shown.
  systemd asks again with the same prompt only after a wrong password,
  so the second time the sentence is "That password did not work. Type
  it again." Any other question before the login screen keeps the
  system's words.
- **The `script` module, not `two-step`.** It is the only one of the two
  that lets the screen say a plain sentence for the password, and it
  needs four stone images instead of a frame for every 20 ms of the
  animation. Its timing comes from the tokens: the module calls the
  script 50 times a second, so a step is `motion.stone` × 50 / 1000
  calls.
- **Drawn in Python, committed, checked.** `brand/boot_screen.py`, which
  `build.py` calls, draws each stone, the box and the dot as a PNG with
  the standard library, from the mark's geometry and colours in
  `tokens.json`. The script's numbers (colours, sizes, the step) come
  from the tokens too, above the hand-written
  `brand/boot-screen.script`. The output is committed in
  `brand/image/plymouth/`, as the image's icons are, so no renderer is
  needed anywhere. `build.py --check` compares a PNG's pixels, not its
  bytes, because another zlib can compress the same pixels differently.
- **The image builds the initramfs again,** last in `build.sh`, with the
  base's own dracut arguments, and stops if it lacks any dracut module
  the base's had. While dracut runs, fontconfig sees only the brand's
  typeface, so the initramfs's one font is Atkinson Hyperlegible Next
  and no other program's font changes. dracut runs with
  `DRACUT_NO_XATTR`, because a rootless build cannot copy SELinux labels
  and the initramfs keeps none, and `/var/roothome` is lent to it for
  the build, so the rebuilt initramfs has exactly the base's files apart
  from the theme and the font. `check-boot-screen.sh` stops the
  build if the theme, its font or Cairn's `initrd-release` is missing
  from the initramfs.

## Consequences

- A child sees the same building at power-on as when a game starts, and
  learns "wait, it is coming" from the first second.
- A computer that updates from a Bazzite-based Cairn image gets the boot
  screen, the name `cairn` where nobody set another, and Cairn Linux
  boot menu titles, because the initramfs changes.
- `plymouth-plugin-script` is one more Fedora package in the image.
- The words at boot are English only. Plymouth's script language has no
  way to translate, so ADR-0007's rule holds everywhere but here. When
  translation work starts (Phase 3), the choice is between one theme per
  language, chosen at install, and going back to systemd's translated
  but technical prompt.
- The screen jumps from the firmware's black to Sand at power-on, and
  from Sand to the login screen's Ink. The brand guide called boot a dark
  surface; the issue asked for the launcher's Sand, which this follows.
  Ink with Sand stones is a change of two semantic names in
  `boot_screen.py` (`ground` and `text` for `dark-ground` and `on.ink`)
  if the maintainer prefers it.
- The images are drawn at one size. On a high-density screen Plymouth
  doubles them, a little soft. The text is sharp at any density.
- A computer with a firmware logo (BGRT) no longer shows it at boot.
- The image is about 2 MB larger for the rebuilt initramfs's font, and
  the image build takes about a minute longer for dracut.
- Checked in the cairn-min VM on 2026-10-29, after one update from a
  slice 5 image: the stones building at boot, from the initramfs, and at
  restart, from the real root; the computer called `cairn`; both boot
  entries "Cairn Linux (ostree:N)"; no failed units. A small LUKS disk
  with `rd.luks.uuid=` on the kernel line brought up the prompt in the
  initramfs, in the brand's typeface, with the dots and Caps Lock; a
  wrong password brought "That password did not work. Type it again.",
  and the right one opened the disk. A question, many dots and Caps
  Lock were also checked with Plymouth started by hand after the login
  screen was stopped.
- Not checked yet: a real laptop's screen, at a high density; a whole
  disk encrypted at install, where the root itself waits for the
  password.
