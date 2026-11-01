# ADR-0044: The boot screen builds the stones

**Status:** proposed. It becomes accepted once the boot screen has been
seen on a real computer. Seeing the disk-password prompt on a computer
whose disk was encrypted at install is a later check, listed under
Consequences, not a condition of acceptance.
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
  reads two font files: whatever `fc-match` named, with no family asked
  for and for `monospace`, when dracut ran. For the base those are Noto
  Sans and Noto Sans Mono.
- No SVG renderer is in the build container (`rsvg-convert` is not;
  ImageMagick is, through Bazzite).

### What the maintainer chose

The first version of this ADR, on 2026-10-29, put the stones on the
launcher's Sand and left four questions. Mason answered them on
2026-10-31:

1. **Ink, with Sand stones.** The screen has the login screen's ground
   from power-on, as the brand guide's "Boot splash is the mark alone,
   centered on Ink" asks.
2. **English only at boot until Phase 3.**
3. **The first sentence names the grown-up:** "A grown-up needs to type
   the password to start this computer." The second sentence stays.
4. **Seeing it on a real computer is the bar for acceptance.** The
   prompt on a disk encrypted at install becomes a later check.

## Decision

**The boot screen is a Plymouth theme on the `script` module, drawn by
`brand/build.py` from the tokens, and the image builds its initramfs
again with it.**

- **What it shows.** The launcher's starting screen (ADR-0028), on the
  login screen's Ink: the mark's stones in Sand, three display sizes
  tall, the bottom stone fading in, then each stone above it, one every
  `motion.stone`, the whole mark for a step, then none, again. Nothing
  else: no spinner, no firmware logo, no words. The same at shutdown and
  restart.
- **The disk password.** When the computer asks for it, nothing is
  working while it waits for a person, so the stones stand whole and
  still. Under them one sentence, "A grown-up needs to type the password
  to start this computer.", in Atkinson Hyperlegible Next at the heading
  size (28 px), then a box like the login screen's, with a dot for each
  letter typed, and "Caps Lock is on." when it is. systemd's own words
  are not shown. systemd asks again with the same prompt only after a
  wrong password, so the second time the sentence is "That password did
  not work. Type it again." Any other question before the login screen
  keeps the system's words.
- **The colours are the login screen's token names.** The ground is
  `dark-ground`, Ink. The stones and the words take its label colour,
  `on.ink`, Sand, as the mark's rule asks on a dark ground. The box is a
  `card`, Paper, wearing the `focus` ring, Sky. The dots and any typed
  answer in it take the card's label colour, `on.paper`, Ink, because
  Sand would all but vanish on Paper (1.09:1). Every pair measures
  7.45:1 or more.
- **Each sentence fits on one line across a screen 800 px wide.**
  Plymouth's script cannot wrap a line. The longest sentence, the first,
  is 769 px wide in the initramfs, where Plymouth draws text with
  FreeType, and 762 px after the root is mounted, where it uses Pango.
  On an 800 by 600 screen that leaves at least 15 px on each side; a
  screen 640 px wide cuts it at both ends. ADR-0003 leaves netbooks out,
  and a laptop at its Minimum tier has a screen wider than 800 px, even
  where Plymouth draws at double size, which Fedora's does only above
  about 236 dots per inch.
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
  The script gives each colour channel as a share of 255, rounded up at
  the sixth decimal place. Plymouth cuts off the fraction when it makes
  a byte of the share again, so a share rounded down drew Ink's blue as
  59, not 60.
- **The image builds the initramfs again,** last in `build.sh`, with the
  base's own dracut arguments, and stops if it lacks any dracut module
  the base's had. While dracut runs, fontconfig sees only the brand's
  two typefaces, so the initramfs's fonts are Atkinson Hyperlegible Next
  and Atkinson Hyperlegible Mono, and no other program's font changes.
  dracut runs with `DRACUT_NO_XATTR`, because a rootless build cannot
  copy SELinux labels and the initramfs keeps none. `/var/roothome` is
  lent to it for the build, because dracut copies the `/root` link and
  its folder, as the base's initramfs has them, and will not copy a link
  whose target is missing. `check-boot-screen.sh` stops the build if the theme,
  its script module, the text plugin, its fonts or Cairn's
  `initrd-release` is missing from the initramfs.

## Consequences

- A child sees the same building at power-on as when a game starts, and
  learns "wait, it is coming" from the first second.
- A computer that updates from a Bazzite-based Cairn image gets the boot
  screen, the name `cairn` where nobody set another, and Cairn Linux
  boot menu titles, because the initramfs changes.
- `plymouth-plugin-script` is one more Fedora package in the image.
- The words at boot are English only until Phase 3, as the maintainer
  decided on 2026-10-31. Plymouth's script language has no way to
  translate, so ADR-0007's rule holds everywhere but here. When
  translation work starts in Phase 3, the choice is between one theme
  per language, chosen at install, and going back to systemd's
  translated but technical prompt.
- The screen goes from the firmware's black to Ink at power-on and stays
  Ink, to the last bit of blue, into the login screen. The brand guide's
  "Boot splash is the mark alone, centered on Ink" holds again, and
  `tokens.json` counts boot among Ink's grounds again. Issue #165 asked
  for the launcher's Sand; the launcher's starting screen keeps it.
- Ink took three colour names, not the two the first version of this ADR
  expected. The dots and a typed answer sit on the Paper box, not on the
  ground, so they keep Ink through `on.paper`; with the ground's Sand
  they would all but vanish.
- On Ink the password box and its ring stand out from the ground: Paper
  is 12.37:1 against it, and the Sky ring 7.45:1. On Sand they were
  1.09:1 and 1.52:1, which is what #216 is about. When #216 gives light
  grounds a Fjord ring and keeps Sky for Ink in a token of its own, this
  box must take that token, since it sits on Ink. A brand test fails
  until it does.
- The images are drawn at one size. On a high-density screen Plymouth
  doubles them, a little soft. The text is sharp at any density.
- A computer with a firmware logo (BGRT) no longer shows it at boot.
- The image is about 2 MB larger for the rebuilt initramfs's font, and
  the image build takes about a minute longer for dracut.
- Checked in the cairn-min VM on 2026-10-29, on the first version's Sand
  and with its first sentence, after one update from a slice 5 image:
  the stones building at boot, from the initramfs, and at restart, from
  the real root; the computer called `cairn`; both boot entries "Cairn
  Linux (ostree:N)"; no failed units. A small LUKS disk with
  `rd.luks.uuid=` on the kernel line brought up the prompt in the
  initramfs, in the brand's typeface, with the dots and Caps Lock; a
  wrong password brought "That password did not work. Type it again.",
  and the right one opened the disk. A question, many dots and Caps
  Lock were also checked with Plymouth started by hand after the login
  screen was stopped.
- Checked on 2026-10-31 on Ink, without a VM: Fedora 44's Plymouth
  24.004.60 in a rootless container, drawing on a virtual X screen
  through the x11 renderer that `plymouth-devel` carries. The stones
  built; the prompt showed with its dots; a wrong password brought the
  second sentence; a question showed its typed answer. Each was seen with
  both text plugins at 1366 by 768, 1280 by 800, 1024 by 768 and 800 by
  600. The ground drew as #1C333C, the login screen's Ink, and the words
  as Sand with either plugin. Caps Lock cannot be shown that way.
- Checked on 2026-10-31 on Ink in the cairn-iso VM. Its image is older
  than the theme, so Fedora's `plymouth-plugin-script` and the theme
  went onto its `/usr` by hand. Plymouth was started by hand after the
  login screen was stopped, so it drew from the real root, with Pango,
  at 1280 by 800. The stones built in Sand. The prompt showed the first
  sentence whole, in Atkinson Hyperlegible Next, with about 260 px of
  ground at each end, and three Ink dots in the box. The same prompt
  again brought the second sentence. Caps Lock brought "Caps Lock is
  on." in Sand under the box. A question showed its typed answer in Ink.
  The ground was #1C333C on the boot screen and on the login screen
  after it.
- Not checked yet, and needed for acceptance: the boot screen on a real
  computer. Not checked either: a real laptop's screen at a high
  density.
- A later check, not a condition of acceptance: the prompt on a computer
  whose disk was encrypted at install, where the root itself waits for
  the password.
