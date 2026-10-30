# Cairn Linux — Roadmap and working plan

**Status:** living document
**Last updated:** 2026-10-21
(D1–D8, D11–D14 closed; D15 deferred)

`DESIGN.md` is the specification. This document is the plan for building it:
what has to be decided, in what order things get built, how we know a phase is
done, and what could sink it. Decisions that change the design get an ADR in
`docs/decisions/` and an edit to `DESIGN.md` in the same change.

---

## 0. Where things stand

- Design document drafted 2026-09-03. Brand guidelines v0.1 (mark,
  palette, type, voice) as a design canvas in `docs/brand-guide/`.
- GitHub organisation `Cairn-Linux` created 2026-09-03. This repository is its
  first, and has been public since the same day (ADR-0005).
- The `kidscan` scanner prototype is imported at `tools/kidscan/` with tests
  (2026-09-03).
- Brand tokens and the mark are generated in `brand/` and validated with
  `python3 brand/build.py --check`.
- The C++ toolchain, CMake presets, clang-format, clang-tidy and qmlformat
  configurations are in place.
- Three launcher slices landed 2026-09-04. The first: six tiles in a Qt Quick
  window, keyboard navigation, translatable strings, accessible names and
  tests for the tile model, the compiled brand tokens and QML navigation.
  The second: tiles launch the program a `kidscan` manifest names, a launch
  that fails or errors within five seconds shows "Something needs a
  grown-up", and the manifest reader and launcher have their own tests.
  The third: the launcher listens to the compositor's window list
  (`ext-foreign-toplevel-list-v1`) and a window nobody launched shows the
  grown-up screen until it closes. KWin does not offer that protocol, so the
  Wayland path is checked under nested labwc, not in CTest.
  The fourth: a labwc kiosk configuration in `session/labwc/` whose window
  rule makes the launcher fullscreen, with no default key or mouse bindings.
  It runs on the dev PC as a normal window under Plasma and fullscreen under
  nested labwc.
- The restricted shell's first two slices landed 2026-09-04 and 2026-09-05:
  the interpreter in `shell/` over a world of doors and a read-only `home`
  (ADR-0013, TERMINAL-DESIGN §3), six words including `exit`, suggestions
  instead of errors, completion, and the surface inside the launcher
  window from the Terminal tile, with `open` going through the launcher's
  own launch path.
- ADR-0015 (2026-09-05): nothing is bundled beyond the frame; a Guardian
  adds apps from a curated list kept in this repository, and the L1 grid
  stays flat and scrolls when a child has more tiles than fit. Came out of
  the maintainer's own run of the launcher: GCompris behind Build and
  Practice read as a native part of Cairn. The launcher's grid scrolls the
  same day (slice 7); the curated list and the Guardian tool are Phase 1.
- The shell is **Footpath** (ADR-0014, closing D9). Since 2026-09-05 it
  lives in its own repository, `Cairn-Linux/footpath`, with its design
  document, and arrives here as the submodule `external/footpath` pinned
  to v0.1.0. The launcher hands it the tiles as doors and binds its look to
  the brand tokens; the terminal's own tests run in its repository.
- `docs/DEVELOPMENT.md` matches the real dev PC as checked on 2026-09-04.
- D5 (level groups, ADR-0011) and D7 (session dispatch, ADR-0012) are closed.
- P0-4 ran nested on the dev PC on 2026-09-06
  (`docs/research/kiosk-containment.md`). Two rows failed and were fixed in
  `session/labwc/`: X11 windows had titlebars, and a window that asked for
  focus was given it over the launcher. VT switching turns out to be code in
  labwc, not a binding, so the keymap drops the keysyms instead. Left for
  the VM: the shortcut rows by keypress, the VT row on a real console, and
  `pkcheck` as a child account. A hung app still has no way out (#41).
- The Bazzite KDE VM exists since 2026-09-06 (`cairn-min`, Minimum tier,
  snapshot `fresh`), and P0-2's `provision/cairn-provision.sh` ran clean
  twice on it. Stock Bazzite ships Plasma Login Manager, not SDDM (D6).
- P0-3 and P0-7 done in the VM the same day. D6 closed with ADR-0016
  (SDDM, swapped in by provisioning); a child lands in the kiosk with no
  password and a Guardian in Plasma with one. malcontent gates Flatpaks
  under Plasma and its control app runs there
  (`docs/research/malcontent-plasma.md`).
- P0-10 ran in the VM on 2026-09-08 with a signed-out client
  (`docs/research/steam-containment.md`). One labwc rule keeps Steam's
  windows off the screen from their first frame, and it has to match the
  WM_CLASS instance `steamwebhelper`, not the class; the launcher shows
  "Something needs a grown-up" naming the window, under a real seat. The
  client is about 800 MB proportional whether signed in or out. Settings ›
  Family offers Steam Families only: Family View and its PIN are gone from
  the client, so DESIGN §8.3 no longer names them. The three P0-4 rows
  that waited for the VM ran too: shortcuts and VT switching hold;
  `pkcheck` as the child said #35's polkit rule is needed. The signed-in
  rows wait for the maintainer's account.
- The polkit rule for the level groups landed the same day
  (`session/polkit/`, #35): NO at every child level for network, package
  and parental-control changes, NO at L1 and L2 for power and removable
  media, never YES. Checked from inside the child's session in the VM;
  logind now answers "no" to power-off, reboot and suspend there. The one
  surprise was logind's `set-reboot-*` actions implying `reboot`.
- The launcher's footprint is measured (`docs/research/launcher-footprint.md`,
  2026-09-08, in the VM): about 145 MB proportional idle and 235 MB for the
  whole L1 session, 2.9 GB of the 4 GB floor free at the tiles and 2.7 GB
  with an app up; first frame 0.2 s after exec, tiles 1.75 s after Enter
  at the greeter, an app back to the tiles in 0.2 s. Tux Paint takes six
  seconds from tile to a usable screen under llvmpipe, with nothing on
  screen meanwhile; a child-test question, not a launcher change yet.
- The signed-in P0-10 rows ran the same evening after the maintainer
  signed in as ada: a silent client maps no window in the kiosk, so the
  tiles show at login; a forced main window hides and the grown-up screen
  names it; a tile running `steam -applaunch` brings the game up and the
  tiles are back within two seconds of it quitting; the login survives a
  reboot. **D3 closed with ADR-0017: labwc.** The client idles at 1.4 GB
  proportional signed in, not 800 MB, which is P1-13's budget to carry.
  Left on P0-10: the child-role rows (no child in the family yet), Steam's
  own toast, the laptop run.
- The launcher tracks a launched app by its window, not its process (#42):
  `steam -applaunch` and `flatpak run` return within a second while the app
  runs, so a window-based lifecycle keeps the launcher Running until the
  app's window closes. Verified 2026-09-08 in the VM: a Steam-routed
  Putt-Putt tile brought the game up fullscreen with no grown-up screen and
  the tiles returned on quit.
- The hung-app row of P0-4 (#41) is answered by ADR-0018: Ctrl-Alt-Home
  runs `session/bin/cairn-give-up`, which ends whatever a child launched,
  however frozen, and leaves the frame and the Steam client standing. The
  power button is a tap to ignore and a five-second hold to power off
  (`session/logind.conf.d/`). Both verified in the VM 2026-09-08: a frozen
  Tux Paint, live GCompris and a Steam game all end within two seconds and
  the tiles return. No launcher change: for a sandboxed app the launcher is
  already behind the window.
- A first pilot with a child ran on 2026-09-25 (#8): a six-year-old, about
  twenty minutes, in the VM as `ada` with the maintainer present. It is not
  the exit test, which needs real hardware. VM lag was the biggest problem.
  The child could not leave an app without help (#77); the login screen
  looked unlike the launcher (#81); nothing said whose profile was whose
  (#82); the Terminal was too advanced for a child who cannot read yet
  (#83). The first-run guide ideas are #78 to #80. The retest is on real
  hardware.
- **ADR-0019 (2026-09-29, #77): a child's own way out of an app.** Super+Q
  asks the app in front to close, as a titlebar's close button would, once
  when the keys are let go however long they are held. In
  the launcher the same key is one step back (the Terminal to the tiles, a
  failed launch to where the child was) and never ends the session; the
  decision is `launcher/src/CloseRequest`. Tux Paint runs with `--autosave
  --saveovernew`, so leaving never loses a drawing. It amends ADR-0018,
  whose Ctrl-Alt-Home stays for a frozen app. Checked in the VM against
  Tux Paint, GCompris, a Steam game, the Terminal, the grown-up screen and
  a frozen app (`docs/research/kiosk-containment.md`).
- **ADR-0020 (2026-09-30, #81, #82): the whole family at the login
  screen.** The Cairn SDDM theme moves into Phase 0 (P1-16 is now P0-11)
  so the next child test sees it: "Who's playing today?" from the brand
  guide, a tile for every child and every Guardian, a password field only
  on tiles that need one. `HideUsers` goes. The Guardian account gets
  `authselect`'s lockout after wrong passwords, and a security key as an
  option. A child account is optional at first boot.
- **ADR-0021 (2026-09-30, #44): a child logs themselves out.** A Log out
  button above the tiles asks once, with the focus on Back, then runs
  `session/bin/cairn-log-out`: Steam gets a moment to shut down, then
  logind ends the child's account, so no app or Steam client is left for
  the next person. It leaves the Guardian's quick actions (P1-7). Checked
  in the VM: the login screen back in about a second, nothing of `ada`'s
  left running.
- The first app after a child logs in hung until PipeWire was up (#87):
  nothing in the kiosk session started it, so the child's first app did.
  `cairn-session` starts it before the kiosk since 2026-09-30, checked in
  the VM with Draw pressed as soon as the tiles showed.
- Provisioning layers the brand's typefaces since 2026-09-30:
  `atkinson-hyperlegible-next-fonts` and `-mono-fonts` are in Fedora but not
  in the Bazzite image, so every Cairn screen had been in a fallback font.
  Checked in the VM on the login screen, the tiles and the Terminal. The
  same run fixed two places where a second provisioning run was not
  quiet: a package still waiting for its reboot, and the SELinux rule for
  `/var/lib/faillock`.
- Provisioning no longer prints the Guardian's password (#103, 2026-09-30):
  the script traces every command, and the password step was traced too.
  At a terminal it now runs `passwd` for the new Guardian instead. It also
  refuses a child account that is a system, `wheel` or Guardian
  account, uses `mktemp` instead of a fixed path in `/tmp`, refuses a file
  from the checkout that links outside it, and checks the signature of
  the package it downloads. Tested with stand-ins
  (`provision/tests/test_provision.py`) and in the VM.
- **ADR-0023 (2026-09-30, #101, #100): one order for an account's role.**
  PAM, `cairn-session`, the polkit rule and the login screen now read the
  groups in the same order: an administrator (`cairn-guardian` or `wheel`)
  first, then L1 and L2, then L3 and L4, then accounts Cairn did not make.
  A Guardian in a level group had been let in by the greeter with no
  password; checked in the VM, before and after. Provisioning adds a new
  level before it drops the old one, so an interrupted change never leaves
  a child in no group, and refuses to make an existing child the Guardian.
  Accounts made in System Settings still get Plasma until the Guardian
  tool owns family accounts (#113).
- **Ready for a first test on a laptop (2026-10-01).** Two cousins, 9 and
  10, are to try it on 2026-10-02 (P0-9, `docs/research/child-test-01.md`).
  The evening before: empty tiles say "coming soon" instead of asking for
  a grown-up, a laptop's volume keys work in the kiosk (#43), provisioning
  makes more than one child at L1 or L2, and a fresh install from stock
  Bazzite was rehearsed in the VM with all of it. The cousins left before
  the run could start; two children at home took its place on 2026-10-10
  and 2026-10-11 (ADR-0031).
- **The kiosk's notes reach the journal (2026-10-01, #122).** Everything
  the kiosk printed, the launcher's notes for a grown-up included, went to
  the child's own `~/.cache/wayland-errors`, which SDDM replaces at every
  login. `cairn-session` now starts labwc under `systemd-cat` as
  `cairn-kiosk` and the launcher's wrapper starts it as `cairn-launcher`,
  so a Guardian reads them with `journalctl -t cairn-launcher` after the
  child has logged in again. Checked in the VM.
- **ADR-0027 (2026-10-01, #119): Steam starts at login for a child with a
  Steam tile.** The launcher starts the client in the background as its own
  service when a tile runs `steam -applaunch`, so the first game starts in
  4.5 s instead of 14.9 s, Steam's own windows come before play, and the
  give-up key leaves the client alone. A child with no Steam tile never
  starts it: the idle client costs about a gigabyte of the 4 GB floor.
  Checked in the VM, with the memory in `launcher-footprint.md`.
- **ADR-0028 (2026-10-07, #129): motion says something true, and a
  starting tile builds the mark.** On the first laptop a Steam game took 7
  to 20 s from the press to its window, with nothing on the screen, and the
  press looked broken. The brand's token note had allowed only the rows to
  move. Now motion may show that something is happening, never decorate or
  hold a child's attention, and a pressed tile shows the Cairn mark building
  itself stone by stone over "… is starting." until the app's window is
  up. Tested in the QML suite. On 2026-10-08 the screen also waits until
  the game's window has stayed up for 3 s, so a game's splash does not
  take it away.
- **ADR-0029 (2026-10-08, #132): a Steam game is waited for while Steam
  works on it.** Pressed 3 s after login, a game on the laptop came up
  21.5 s after the tap, after the launcher's 15 s wait had ended, so its
  window counted as one that opened on its own. A Steam tile's launch now
  waits past the 15 s while the client started at login is younger than a
  minute or Steam's `reaper` for the game runs, up to two minutes. Other
  tiles keep the 15 s. Tested with a stand-in `/proc`.
- **ADR-0030 (2026-10-11, #139): a launch that shows nothing needs a
  grown-up.** On 2026-10-10, offline, Steam held twelve of thirteen game
  presses behind its "Cloud sync failed" dialog, which the kiosk hides. Each
  time the tiles came back after 15 s without a word, and the children
  pressed again. A launch that has put nothing on the screen when its wait
  ends now shows "Something needs a grown-up. … did not start." with Back,
  for every tile; a window that comes before Back still takes over. Tested
  in C++ and QML.
- **Phase 0 ends (2026-10-12, ADR-0031).** Two children at L1 used the
  kids' laptop on 2026-10-10, offline, and the younger one alone for about
  45 minutes on 2026-10-11, online. They logged in from their tiles and
  stayed in the kiosk, as far as the journal shows. Only the younger one
  got a Steam game to start and came back to the tiles: one offline, two
  online. No child has yet opened something from the Terminal or seen the
  grown-up screen, and the laptop is above the 4 GB floor: those rows move
  to P1-17 and P1-1. Image work starts with P1-1 to P1-3 on the dev PC.
- **ADR-0034 (2026-10-18, #43): the top bar shows the battery.** On the
  kids' laptop a child played from 99% to an empty battery with nothing
  on the screen saying so, and the laptop died asleep. The bar above the
  tiles now shows a drawn battery whose fill is the percentage, a plug
  while the charger is in, and one sentence when UPower's warning level
  says it is low: "The battery is low. Ask a grown-up for the charger."
  UPower decides what low is; the launcher changes nothing in it. Nothing
  shows over a running game, which needs a surface the compositor draws
  and is the maintainer's decision. Tested against a stand-in UPower, and
  read from the dev PC and the VM; not yet run down on the laptop.
- **ADR-0035 (2026-10-19, #138): a game's tile shows its picture from
  Steam.** On 2026-10-10 the younger child could not tell one Putt-Putt
  game from another, because every tile showed only a title, and she chose
  by guessing. Provisioning now writes the game tiles from the shared
  folder to `/var/lib/cairn/games.json`, which the launcher reads with
  `--games`, so a re-run keeps them (ADR-0033). It also copies each game's
  header picture from the Guardian's own Steam into
  `/var/lib/cairn/game-pictures/`; nothing is downloaded. A game's tile
  shows that picture inside its Paper card in place of the title, and a
  game with no picture shows its name. Tested in C++, QML and Python with
  stand-in pictures, and against a made-up machine. Installed by hand on
  the kids' laptop on 2026-10-20. There 9 of the 25 games had their
  picture only as `library_header.jpg`, which the step did not read, so
  their tiles showed names; it reads that name too now.
- **ADR-0036 (2026-10-20): wait while Steam's log says it is getting a
  game ready.** On the kids' laptop a tap on Arthur's Birthday made two
  held-back Steam updates due; Steam downloaded them for 40 s with no
  reaper, so the grown-up screen said the game did not start, and the game
  came 30 s later. The launcher now reads the lines Steam adds to its own
  log after the tap. While they say Steam is working on the game, the
  launch waits up to 10 minutes and the starting screen adds "Getting it
  ready. This can take a few minutes." A question on a hidden window or a
  refusal does not keep it waiting, and the journal says which it was.
  Tested in C++ and QML with lines from the laptop's logs; not yet run
  there.
- **ADR-0037 (2026-10-20): wait while Steam updates itself, and tap again
  if it lost the tap.** On the kids' laptop a tap 18 s after login came
  while Steam installed an update of its own client. Steam restarted
  without it, so the grown-up screen came and no game followed. The
  laptop's logs held 5 such taps, all lost, while every tap made as Steam
  only started was kept. The launcher now reads Steam's update log: while
  Steam starts or updates itself the launch waits, saying the game is
  getting ready, and if the restarted client has not taken the tap 3 s
  after its own start command, the launcher sends it again, once. Tested
  in C++ with lines from the laptop's logs; not yet run there.
- **ADR-0038 (2026-10-21, #164): the first Cairn image.** The image
  compiles Cairn in a build stage it throws away and copies the tree
  `cmake --install` makes. On Bazzite `/usr/local` belongs to the machine,
  not the image, so every file moves to its standard place under `/usr`,
  and only PAM, faillock and one SDDM file stay in `/etc`. Nothing that
  names a person goes in the image. The first slice adds the install
  rules, the wrappers as templates, and a test that installs the tree and
  checks every file and its mode. Provisioning on the kids' laptop is
  unchanged. The second slice builds the image on the dev PC: a build
  stage compiles Cairn against the base's own Qt, stops on any failing
  test, and the image copies the installed tree. `bootc container lint`
  passes with no warnings. The third slice sets the image up to boot
  into Cairn: labwc, ScummVM, SDDM and the fonts layered with `dnf5`,
  SDDM as the login screen, the visit's cage, the groups with fixed
  numbers from `sysusers.d`, the folders in `/var` from `tmpfiles.d`,
  faillock and the SELinux labels. It booted in the VM the same day: the
  Cairn login screen, a child at the tiles, a Guardian in Plasma.
  The fourth slice leaves the family's own steps outside the image:
  `provision/cairn-family-setup.sh`, run from a checkout of the image's
  commit, makes the accounts, the "Steam for <child>" entries in
  `/usr/local/share/applications`, the game tiles, each child's copies
  and the Flatpaks, from provisioning's own steps.
  `provision/cairn-renumber-guardians.sh` gives a hand-provisioned
  computer's `cairn-guardian` the image's number before it moves. Built
  and tested on the dev PC 2026-10-21, and run in the VM the same day.
  The family's setup made a second child (ben, level 2), the "Steam for
  <child>" entries, `games.json` and the shared game folder. The entries
  appeared in the Guardian's menu without logging in again, the children
  reached their tiles, and SELinux denied nothing. The renumbering's
  first try refused the ssh login that ran it; fixed to count only a
  desktop session, it then ran over ssh with a console login open and
  moved every file and ACL entry from 945 to 465.
  The fifth slice gives the image Cairn's name and mark: `os-release`
  says Cairn Linux with `ID=cairn`, which bootc-image-builder resolves to
  Fedora 44 through `ID_LIKE` as it did Bazzite. `generic-logos` replaces
  `fedora-logos`, the Cairn mark fills Bazzite's logo places and the
  Guardian's splash, and Bazzite's terminal banner, announcements and
  documentation entry are gone. A check step stops the build if any of
  it comes back. Built on the dev PC 2026-10-21 and booted in the VM the
  same day: System Settings' About page, `hostnamectl` and the terminal
  name Cairn Linux, the Guardian's start button and splash show the mark,
  the shutdown screen lost Bazzite's watermark, `rpm -V` is clean, and
  updates, Flatpak, Steam and `ujust` still work. Two things stay
  Bazzite's until #165 rebuilds the initramfs: the hostname, which the
  initramfs's own `initrd-release` sets, and the boot menu titles, which
  libostree rewrites only when the kernel or initramfs changes. The ADR
  had claimed both changed; it now says what the VM showed. A fresh
  install's boot titles are a slice 6 check.
  The sixth slice prepares the installer ISO: `disk_config/iso.toml` and
  `tools/build-iso.sh`, which the maintainer runs once with `sudo`. The
  installer asks before it touches a disk, offers no account, and names
  the computer `cairn`. Without root, the pinned builder planned it, and
  a test ISO with a test Guardian from a private config. Planning found
  that the builder cannot read Bazzite's `terra-mesa` signing key, so
  the script lends it the image's keys. A test ISO was built and
  installed in a new VM on 2026-10-22: the installer stopped for the language,
  disk, keyboard and time and finished, and the computer was called `cairn`,
  with "Cairn Linux" boot titles, the groups at their numbers and
  Cairn's login screen. A slow first build failed when Fedora replaced
  six packages mid-download; the next run took about six minutes. An
  install with no account is still untested.
- **ADR-0039 (2026-10-22, #167): one root command changes the family's
  accounts.** `cairn-family create-child` and `cairn-family set-level`,
  in C++ with Qt Core only, make a child's account and change a child's
  level, and write the child's "Steam for <child>" entry. The Guardian
  tool's window runs it through pkexec (ADR-0040), behind
  `com.cairnlinux.family`, which asks for a password for every change;
  the family's setup runs it with sudo. It checks the caller again
  itself, gives levels 1 and 2 only, adds the new level group before
  dropping the old one, and refuses any account Cairn did not make
  rather than adopt it. It answers `done` or `refused` and a word. The
  rule for who an account is moves into `accounts/`, which the login
  screen shares. Accepted 2026-10-23 after the setup and the window
  ran it in the VM.
- **ADR-0040 (2026-10-23, #167): the Guardian tool is its own app.**
  `cairn-guardian`, "Family" in a Guardian's menu, is a window of its own beside the Steam window. Its one
  page so far, Children, lists each child with their level, adds a child
  at level 1 or 2 with a login name made from the name the parent types,
  and moves a child to another level with the same words up or down.
  Each change is one run of `cairn-family` through pkexec, one at a
  time, with a plain sentence after each answer. Accepted 2026-10-23,
  after a Guardian used it in the VM with the real password prompt.
- **ADR-0041 (2026-10-27, #167, proposed): each child has their own
  list of games.** Children of different ages need different games, so
  on the image a child gets copies and tiles only for the games a
  Guardian picks for them, on the Guardian tool's Games page, with one
  password per Save. A new child starts with none. A game taken off the
  list loses its tile at the next login, and its copy and saves stay.
  The kids' laptop keeps every game for every child until it is
  reinstalled from the image. Before levels 3 and 4 ship (#107), a child
  must be kept from games off their list in the shared folder. Step 1 is
  in: `cairn-copy-games --only` and `cairn-steam-visit --copy-games
  CHILD [APPID...]` copy only the listed games, an empty list copies
  nothing, and provisioning asks for every game with `--all`.
  Step 2 is in: `cairn-family save-games`, the root side of Save.
  Step 3 is in: the Guardian tool has a switch between its Children and
  Games pages, and the Games page shows, for the child picked, every
  shared game with the games on their own list highlighted, and opens
  that child's own Steam.
  Step 4 is in (2026-10-28): a parent taps games on and off for one
  child or several, and Save gives each changed child their whole new
  list with one password, saying whose games are being copied and then
  what happened for each child. The image's launcher reads each child's
  own list, and the family's setup no longer copies every game, so a
  child has no games until a Guardian saves their list.
  Step 5 is in (2026-10-29), as Mason reshaped it that day: a logged-in
  child's games are greyed and blurred on the Games page, with **Log Ada
  out**, which warns first and then only logs the child out, with its
  own password (`cairn-family log-out`, `cairn-steam-visit --log-out`).
  The page follows logind and shows the games again once the child is
  out. Save opens a progress window that names each copied game and says
  not to log out or close the lid, and the copy holds a `systemd-inhibit`
  lock on sleep and the lid. The first version's "Later" (a waiting list
  copied at the child's logout by a systemd unit) and "Log them out now"
  (a Save that logs a child out) are gone. Next: the checks in the VM,
  among them whether Plasma's lid handling honours the lock.
- **Steam updating itself at login keeps the tiles (2026-10-30, #183,
  amends ADR-0037).** On 2026-10-27 the kids' laptop showed the grown-up
  screen for 13 minutes at a login, with Steam's updater box and its
  Cancel button on top, while Steam installed one update and downloaded
  another. While Steam's updater log says it is updating itself, or has
  just restarted after an update, its windows are now Steam's: they bring
  up no grown-up screen, and the launcher asks labwc to minimise them
  through the window protocol it already uses. Tested in C++ with the dev
  PC's log of the same two updates, and by hand under nested labwc with a
  made-up log; not yet seen on a real Steam update in the kiosk. A labwc
  window rule could hide them sooner once their X11 instance is known.
- **A Steam that cannot sign in needs a grown-up soon (2026-10-29, #182,
  amends ADR-0036).** On the kids' laptop a tap came while the tester
  account's Steam had its saved sign-in turned down. The starting screen
  said "Getting it ready" for 10 minutes and no game came. The launcher
  now reads Steam's `LogonFailure` lines while Steam's log says nothing
  about the game: a refused sign-in ends the wait at the next grace, and no
  connection gets a minute. Both end on the grown-up screen, without
  "Getting it ready", and the journal says which it was. Tested in C++
  with lines from both laptops; not yet run on the kids' laptop. A line
  for the grown-up on the grown-up screen is still open.
- **Give-up spares Steam's interface again (2026-10-01, #117).** A Steam
  update moved Steam's own interface into a sandbox called `srt-bwrap`,
  which the give-up key's `bwrap` match caught. Flatpak's sandbox is now
  matched by its whole name, and a Steam game's container is found as the
  child of its `reaper`. Checked in the VM: a frozen Steam game and a
  frozen Tux Paint end, and Steam's interface stays up. A new test runs the
  real helper in a PID namespace of its own, so it can never touch a real
  app on the machine running it.
- **ADR-0026 (2026-10-01, #99): what counts as the app's window.** In the
  VM a hidden Steam window could stop the tiles working after a game, or
  hold a grown-up screen nothing could clear. The launcher now reads the
  compositor's other window list, which says whether a window is hidden
  and which window it belongs to: a hidden window never counts, the first
  window of a launch and its dialogs are the app, and any other window gets
  the grown-up screen. A program that has run and exits ends its launch,
  each launch has its own process, and Ctrl-Alt-Home always brings the
  tiles back. Checked in the VM. Builds now need `wlr-protocols-devel`.
- **ADR-0025 (2026-09-30, #98): every launch runs in a scope of its own.**
  The grown-up's Ctrl-Alt-Home matched only the roots of Flatpak apps and
  Steam games, so a frozen native ScummVM survived it. The launcher now
  starts each program in `cairn-app-*.scope` in the kiosk, and
  `cairn-give-up` ends those scopes as well. Checked in the VM: a frozen
  Putt-Putt and the speech-dispatcher it had started ended in 0.7 s, and
  Flatpak apps still end as before. A behavioural test freezes a stand-in
  in a scope and runs the real helper.
- **ADR-0024 (2026-09-30, #97, #76): games are a fourth kind.** kidscan's
  manifest now loads in the launcher as it stands: its titles are `games`
  tiles, drawn as the brand sketch draws a game (a Paper card with a
  hairline edge, no new colour), and Footpath 0.2.0 lists them under
  `games` in the Terminal with an outlined chip. kidscan no longer passes
  the Windows-only `--no-console` to ScummVM, and one bad manifest entry no
  longer refuses the rest. A round-trip test runs kidscan's output through
  the launcher's reader. Checked in the VM with Putt-Putt Joins the Parade
  in native ScummVM, from the tile and from `open`.
- **Phase 0 begins.** Its whole purpose is to put a launcher in front of a
  real child before any image tooling exists.

---

## 1. Decisions needed

Each has a recommendation. None is final until an ADR lands in
`docs/decisions/`. Ordered by when they bite.

| ID | Decision | Recommendation | Bites when |
|---|---|---|---|
| **D1** | Project licence (DESIGN §12.3) | **Closed 2026-09-03, ADR-0005.** Apache-2.0 for code; CC BY-SA 4.0 for docs and brand; name and mark are trademarks with a policy in `TRADEMARKS.md`; DCO sign-off; SPDX headers. Repository public from this date. | Closed |
| **D2** | First-party language and UI toolkit (DESIGN §14 Q1) | **Closed 2026-09-03, ADR-0002: C++20 + Qt 6 + QML** for every first-party surface, including the restricted shell. Chosen by the maintainer to learn C++; the technical recommendation had been Python + PySide6 with the same QML. Fallback if a component proves unworkable is exactly that, and the QML carries over. | Closed |
| **D3** | Kiosk compositor for L1/L2 (DESIGN §4.5) | **Closed 2026-09-08, ADR-0017: labwc** in kiosk configuration. ADR-0004 makes Steam at L1/L2 a v1 requirement, and a kiosk has no tray, so the compositor must hide Steam's forced windows with window rules and tell the launcher about them via a foreign-toplevel protocol. cage has neither and stays a measurement baseline. Both halves held in the VM with a signed-out and a signed-in client (`docs/research/steam-containment.md`, `kiosk-containment.md`). | Closed |
| **D4** | Base image (DESIGN §4.1 says Fedora/bootc, not which) | **Closed 2026-09-03, ADR-0006: Bazzite (KDE), `ghcr.io/ublue-os/bazzite:stable`.** One Intel/AMD image for v1; Nvidia unsupported until later. Policy review found no blocker (`docs/research/base-image-policy.md`); compliance is P1-14. Fallback `kinoite-main` + layered Steam. | Closed |
| **D5** | How "level" is stored and enforced (DESIGN §3, §4.3) | **Closed 2026-09-04, ADR-0011.** Supplementary groups: `cairn-l1` … `cairn-l4`, `cairn-guardian`; exactly one per account. Level change = group change. PAM, polkit, the session dispatcher and malcontent policy all key off groups, which is Unix-native, inspectable and legible (Principle 2). Per-child name/avatar/allowlists live in a small config the Guardian tool owns. | Closed |
| **D6** | Display manager and greeter (DESIGN §4.3 describes the login screen, not the component) | **Closed 2026-09-06, ADR-0016: SDDM with a Cairn QML theme.** Stock Bazzite KDE and Fedora 44 KDE ship Plasma Login Manager, whose configuration cannot hide users, limit sessions or take a theme, so provisioning and the Phase 1 image swap it for SDDM explicitly, as Bazzite's own Deck stage does. `SessionDir` offers only `cairn.desktop`, `HideUsers` lists the Guardians, and `/etc/pam.d/sddm` lets `cairn-l1`/`cairn-l2` in without a password. Breeze theme until the Cairn theme lands (P0-11); ADR-0020 un-hides the Guardians. | Closed |
| **D7** | Session dispatch | **Closed 2026-09-04, ADR-0012.** One Wayland session entry (`cairn.desktop` → `cairn-session`) that reads the account's level group and execs either the kiosk compositor + launcher (L1/L2) or `startplasma-wayland` (L3/L4, Guardian). The greeter offers only this session, so a child cannot pick another. | Closed |
| **D8** | Localisation and offline stance (DESIGN §14 Q4, Q5) | **Closed 2026-09-03, ADR-0007.** English-only v1 with all strings externalised from the first commit; offline-capable, not offline-first. | Closed |
| **D9** | Name and architecture of the restricted shell | **Closed 2026-09-05, ADR-0014: Footpath.** A Qt Core interpreter behind a QML surface hosted inside the launcher window, **not** a PTY program; its own repository `Cairn-Linux/footpath` with `docs/TERMINAL-DESIGN.md` as its design document, consumed by Cairn as a pinned submodule and packaged as the `footpath` RPM in P1-2. The tile stays "Terminal". | Closed |
| **D10** | Content filtering (DESIGN §9.2 "TBD") | Two options, both local-config-only to honour §9.3: a **filtered upstream DNS resolver** (simple, but sends every query to a third party) or a **local resolver with blocklists** (private, needs list updates via the OS image). Decide in Phase 2 when the browser first appears at L2. | Phase 2 |
| **D11** | Hardware floor (DESIGN §7 had a 2 GB minimum that contradicted its own 2013–2018 target) | **Closed 2026-09-03, ADR-0003.** Minimum: about 2013 or newer, 4 GB, Intel HD 4000+, UEFI, 64 GB. Recommended: 8 GB, discrete or modern integrated GPU, 128 GB. Published as a game-box Minimum/Recommended panel everywhere it appears. | Closed |
| **D12** | Is Steam at L1/L2 a v1 requirement? (DESIGN §3 table said L3+, §8.3 said all levels) | **Closed 2026-09-03, ADR-0004: yes, all levels.** The maintainer's own kid-friendly Steam library is the use case. Steam integration moves from Phase 3 to Phase 1; containment is proven in Phase 0 (P0-10). | Closed |
| **D13** | Accessibility scope for v1 (DESIGN §14 Q6) | **Closed 2026-09-03, ADR-0008.** High-contrast and large-text modes in v1 as token-set variants; screen reader, dwell click and specialist review in Phase 3. `Accessible.name` on every QML control from the start. | Closed |
| **D14** | Which first-party app carries the aesthetic (DESIGN §6.3) | **Closed 2026-09-03, ADR-0009: none in v1.** The frame, terminal and the launcher's creations gallery carry the identity; survey existing maintained kid software instead; revisit after Phase 2 (story maker leading, then music). | Closed |
| **D15** | ScratchJr on Linux: now, later, never (DESIGN §5.2) | **Deferred 2026-09-03, ADR-0010.** Desired feature; the implementation question (port, contribute upstream, adopt a package) is taken after Phase 0 with child-test evidence. Meanwhile "Build" uses existing software via the survey. | After Phase 0 |

---

## 2. Phase 0 — validate the experience (no ISO)

**Goal.** One child aged 5–8, on a Minimum-tier laptop (ADR-0003), uses the L1
session unsupervised for twenty minutes.

**Exit criterion.** The child: logs in from an avatar tile; launches and
returns from at least two apps, one of them a Steam-routed title; uses the
shell to open something; hits an error and recovers without help; never leaves
the kiosk. Observations are
written up in `docs/research/`. Then, and only then, Phase 1.

**Exit, 2026-10-12 (ADR-0031).** Phase 0 ends on the children's evenings of
2026-10-10 and 2026-10-11 on the kids' laptop, short of this criterion as
written. Seen, by children in the 5–8 band: logging in from a tile,
launching apps including Steam games, and twenty minutes alone. Seen as far
as the journal shows: coming back to the tiles, and staying in the kiosk,
though Steam's overlay was on (#74). Partly seen: an error. On 2026-10-10
twelve of thirteen Steam presses started nothing and the children went on to
other tiles, but the launcher said nothing then. Its grown-up screen for
this (ADR-0030) came after, and no child has seen it. Not seen: using the
Terminal, and a Minimum-tier laptop. Not met: #8's prerequisites for Steam
(#25, #74, #88). What was not seen moves to P1-17 (child test 2), P1-1 (the
4 GB floor) and P1-13 (Steam). Row by row: `docs/research/child-test-01.md`.

**What Phase 0 deliberately skips.** Guardian GUI (the provisioning script and
a CLI stand in), first-boot wizard, quick-actions overlay, ISO, CI, signing.

**P0-4 and P0-5:** nested labwc on the dev PC; the test laptop only for RSS
and the VT-switch row.

| ID | Task | Depends on | Done when |
|---|---|---|---|
| **P0-1** | Repository, planning, brand tokens and mark as code | — | This commit. |
| **P0-2** | **Provisioning script** `provision/` that turns a stock Bazzite KDE install (ADR-0006) into a Cairn machine: creates the level groups from ADR-0011, one Guardian, one L1 child; installs the kiosk compositor and the Phase 0 app set (Tux Paint, GCompris, ScummVM via Flatpak/dnf); installs the session entry and the display manager and greeter configuration (D6). Idempotent; safe to re-run. `cairn-provision.sh` ran clean twice on the fresh VM on 2026-09-06 (groups, accounts, labwc and ScummVM layered, Tux Paint and GCompris Flatpaks, kiosk config, launcher). The session entry and dispatcher (P0-3) and the level-group polkit rule (#35) were added as they landed. | — | Runs clean twice on a fresh VM. |
| **P0-3** | **Session dispatch and greeter** in `session/`: `cairn-session` dispatcher (ADR-0012), `cairn.desktop`, display manager and greeter configuration (D6), PAM rule for passwordless L1/L2. Resolves D6 only. **Done 2026-09-06 in the VM** (ADR-0016): stock Bazzite KDE ships Plasma Login Manager; provisioning swaps in SDDM, offers only `cairn.desktop`, hides Guardians, and lets `cairn-l1`/`cairn-l2` in without a password from the greeter only. `ada` landed in the kiosk with Enter on an empty field; a `cairn-l3` account and the hidden Guardian (through the Breeze "Other" button) landed in Plasma with a password; `su - ada` and `ssh ada@` fail. Left: the Cairn theme (P0-11, ADR-0020). The level-group polkit rule (#35) landed 2026-09-08. | P0-2 | Record which display manager stock Bazzite KDE ships (`rpm -q sddm plasma-login-manager` and `readlink -f /etc/systemd/system/display-manager.service` in the VM); child account lands in the kiosk with no password prompt; Guardian account lands in stock Plasma with one. |
| **P0-4** | **Kiosk containment test** with labwc running a placeholder client: a launched Tux Paint or ScummVM window appears on top and closing it returns to the launcher; Alt-Tab, Super, and Ctrl-Alt-Fn VT switching are unreachable; a focus-stealing X11 client cannot steal focus under XWayland. Steam-specific rows live in P0-10. Nested rows ran 2026-09-06: Tux Paint on top and back, focus requests refused and X11 titlebars removed by a new window rule, VT keysyms removed by `session/labwc/environment`. The VM rows ran 2026-09-08: shortcuts and VT switching hold at the real console; `pkcheck` as the child said polkit lets a child power off, mount and change Wi-Fi; the rule in `session/polkit/` (#35) landed the same day and the row passes from inside the child's session. The hung-app row (#41) is answered by ADR-0018: Ctrl-Alt-Home runs `cairn-give-up`, verified against a frozen app in the VM. The child's own way out (#77), found missing in the first pilot, is ADR-0019: Super+Q closes the app in front, verified in the VM 2026-09-29. | — | Written checklist in `docs/research/kiosk-containment.md`, all rows pass or have a named mitigation. |
| **P0-5** | **Launcher v0** in `launcher/`: a C++/QML app. Six tiles from `Cairn.Brand.Tokens`, colour by kind; keyboard and mouse navigation. Slices 1 to 3 landed 2026-09-04 (six tiles, navigation, `QProcess` launch from a `kidscan` manifest, the "Something needs a grown-up" screen on failure and on a window nobody launched, via `ext-foreign-toplevel-list-v1`, tests). Slice 4 added the labwc kiosk configuration in `session/labwc/`, whose window rule makes the launcher fullscreen; the launcher itself has no fullscreen option. The window listener and the fullscreen rule were checked by hand under nested labwc 0.9.6 on 2026-09-04. The grid scrolls since 2026-09-05 (slice 7, ADR-0015). Memory and latency recorded 2026-09-08 in the VM (`docs/research/launcher-footprint.md`): about 145 MB proportional idle, 235 MB for the whole session, first frame 0.2 s after exec, tiles 1.75 s after Enter at the greeter; the laptop run moves to P1-1 (ADR-0031). The launcher tracks a launched app by its window, not its process, so `steam -applaunch` returning at once no longer looks like a stray window (#42, verified in the VM with a Steam-routed title). A close request is one step back and never closes the launcher in the kiosk (ADR-0019, 2026-09-29). A child logs themselves out from a button above the tiles (ADR-0021, 2026-09-30). A pressed tile answers at once: the Cairn mark builds itself over "… is starting." until the app's window has stayed up for 3 s, so a game's splash does not take it away (ADR-0028, #129, 2026-10-07 and 2026-10-08). A launch that shows nothing before its wait ends says it did not start, on the grown-up screen, instead of going back to the tiles (ADR-0030, #139, 2026-10-11). A Steam game Steam is still getting ready keeps the starting screen, which says so (ADR-0036, proposed, 2026-10-20). A tap Steam lost while it updated itself is sent again (ADR-0037, proposed, 2026-10-20). Game tiles show the game's picture from the Guardian's Steam (ADR-0035, proposed, 2026-10-19). Remaining: pictures for the other tiles (DESIGN §14 Q3). | — | Runs as a window under Plasma and fullscreen under nested labwc; RSS recorded in `docs/research/launcher-footprint.md`. |
| **P0-6** | **Restricted shell v0** in `shell/`: C++ interpreter behind a QML text surface. L1 vocabulary candidate of six real commands (`ls`, `cd`, `open`, `cat`, `help`, `exit`), large type, aggressive completion, suggestion-style errors ("I don't know \"opn\" — did you mean open?"), icon-augmented `ls`. Nothing destructive reachable. The interpreter is a pure C++ class with no Qt GUI dependency so it is unit-testable in isolation. Slices 1 and 2 landed 2026-09-04 and 2026-09-05: the interpreter over doors and `home`, six words, suggestions, completion, and the surface inside the launcher with `open` through `AppLauncher`. Split into `Cairn-Linux/footpath` on 2026-09-05 (ADR-0014). Remaining, in that repository: a `--home` layout file, icons on `ls` beyond colour chips (DESIGN §14 Q3); here: a child opening something from it, which Phase 0 did not see (P1-17, #83). | P0-5 | Child can open an app from it. Vocabulary written up for testing (DESIGN §14 Q2). |
| **P0-7** | **malcontent-on-Plasma check** (DESIGN §14 Q7). In a VM: install malcontent, restrict a test user, log into Plasma, confirm restricted Flatpaks refuse to launch and the restriction UI is reachable. **Done 2026-09-06:** yes on both counts, recorded in `docs/research/malcontent-plasma.md`; §4.5 and §9.1 stand. malcontent 0.14's own session timer and web filter are recorded there for P1-9, D10 and #38. | — (parallel) | Result recorded in `docs/research/malcontent-plasma.md` and, if it changes §4.5 or §9.1, an ADR. |
| **P0-8** | **Import `kidscan`** into `tools/kidscan/` with its README; no GUI yet. | — (parallel) | **Done 2026-09-03.** Imported with a fixture-based test suite. Ran against a real library in the VM on 2026-09-08 (four Humongous titles, all detected as native ScummVM), which found and fixed the headless-detect bug; the rest of #24 stands. |
| **P0-9** | **Child test.** Real hardware, real child, twenty minutes, observer takes notes and does not intervene unless asked. Iterate on P0-5/P0-6 and repeat at least once. A pilot ran 2026-09-25 in the VM (#8), not the exit test; its findings are #77–#83, and #77 is closed by ADR-0019. Two children at L1 used the kids' laptop offline on 2026-10-10; its findings are #138–#141, the first being that a child who cannot read cannot tell the games apart. The younger child used it alone and online on 2026-10-11. **Done 2026-10-12 (ADR-0031), short of the exit criterion as written:** Phase 0 exits on those two evenings; the Terminal, the grown-up screen and the 4 GB floor were not seen and move to P1-17 and P1-1. | P0-2…P0-6, P0-10 and P0-11 | Write-up in `docs/research/child-test-01.md`. Phase 0 exit criterion met. |
| **P0-10** | **Steam containment spike** (ADR-0004; the riskiest item in the design, so it runs early). **In the VM**, under labwc: start the client with `-silent` at session start; launch a game with `steam -applaunch`; force the client window open (`steam steam://open/main`, a pending client update, a signed-out client) and confirm window rules keep it off the screen; confirm the launcher learns of the foreign window via `ext-foreign-toplevel-list-v1` (P0-5) and can show "Something needs a grown-up"; confirm Family View PIN locks store/community/friends when a window does slip through. **On the Minimum-tier laptop:** record idle RSS of the client against the 4 GB floor; confirm Ctrl-Alt-Fn VT switching is unreachable. Note that the Steam client is an X11 app under XWayland, so rules match its `WM_CLASS`. **Ran 2026-09-08 in the VM with a signed-out client** (`docs/research/steam-containment.md`): the rule in `session/labwc/rc.xml` hides the sign-in window from its first map and the launcher names it on the grown-up screen; `steam://open/main` and `-applaunch` change nothing signed out; the client is about 800 MB proportional; Settings › Family has no Family View. Bazzite's `bazzite-steam` wrapper puts a KDE dialog on the child's screen on first run, so the session must start `/usr/bin/steam -silent` itself (P1-13). The signed-in rows ran the same evening: no window from a silent client, the forced main window hidden and named, `-applaunch` from a tile up and back, the login through a reboot; the client is 1.4 GB proportional signed in. D3 closed with ADR-0017. Left, now under P1-13 and P1-1 (ADR-0031): the child-role check (needs a child in the family), Steam's toast over the game, the laptop memory run, and whatever Valve forces (update, Steam Guard, expiry) when it does. | P0-3 | `docs/research/steam-containment.md` with every row passing or named as an accepted gap; D3 closes with an ADR. |
| **P0-11** | **The Cairn login screen** (ADR-0020, moved from P1-16): an SDDM theme after the brand guide's "Who's playing today?" sketch, every value from `Cairn.Brand.Tokens`. A tile per family member, children first, then Guardians; a circle in a brand colour with the first letter, the name beneath. An L1 or L2 child logs in with one press; any other tile asks for the password on the same screen. A small C++ QML module reads which kind an account is from its groups (ADR-0011), with Qt Test coverage. An account outside the family gets a password tile after the Guardians. Provisioning drops `HideUsers`, installs the theme and its module, enables `authselect`'s `with-faillock` and keeps its count in `/var/lib/faillock` so a reboot does not reset it. **Done 2026-09-30 in the VM:** the theme loads under the real SDDM with `ada`, `guardian` and the installer's `bazzite` (shown as "cairn") in that order; `ada` reached the kiosk with one Enter; three wrong passwords for `guardian` wrote three entries to `/var/lib/faillock` (labelled `faillog_t`), the right password was refused while locked and still after a reboot, and after `faillock --reset` the Guardian reached Plasma. The run found that an empty Enter counted as a wrong password; the theme no longer sends one. The children logged in from it on the kids' laptop (P0-9). Left: the Atkinson fonts, which the VM does not have. | P0-3 | Checked in the VM: `ada` logs in with one press, `guardian` with a password, repeated wrong passwords lock `guardian` out for a while, still after a reboot, and the screen says so plainly; `qsTr()` and `Accessible.name` everywhere; 18px minimum. |

---

## 3. Phase 1 — MVP image (L1 only)

Ships: launcher, shell, core educational apps via Flatpak, ScummVM and Steam
integration, Guardian role with account creation and level management,
first-boot wizard, quick-actions overlay, signed image, CI, ISO.

| ID | Task |
|---|---|
| P1-1 | Measure image size and first-boot RAM on both hardware tiers with the Steam client resident. On the Minimum tier this includes the laptop runs Phase 0 left (ADR-0031): the launcher's memory and Tux Paint's start (`docs/research/launcher-footprint.md`), and the Steam client's memory (`docs/research/steam-containment.md`). Until the project has a Minimum-tier laptop, the VM gives upper bounds, not floor numbers. Disable Bazzite's first-boot portal, Game Mode session and Waydroid. |
| P1-2 | Package launcher, shell and session as RPMs (in-tree spec files, built in a Containerfile stage or via a COPR). Footpath is packaged from its own repository (ADR-0014). Until then the first image compiles Cairn in a build stage and copies the tree `cmake --install` makes, with every file where an RPM would put it (ADR-0038, accepted, 2026-10-21); the install rules and their test landed that day. |
| P1-3 | Build the first image on the dev PC and boot it in the VM (#164, ADR-0038): the build stage and the steps provisioning does by hand for the whole machine (packages, SDDM, groups, folders, faillock; both built on the dev PC and booted in the VM 2026-10-21), then what is the family's own (slice 4: `provision/cairn-family-setup.sh` and the renumbering of `cairn-guardian`, built 2026-10-21 and run in the VM that day, renumbering included), then name and logos (slice 5: `os-release`, `generic-logos` for `fedora-logos`, the Cairn mark in Bazzite's logo places, Bazzite's welcome and support links removed; built 2026-10-21 and booted in the VM 2026-10-21). The build stage's tests pass again from 2026-10-30: two Guardian tests now say their own locale, since a container build has none, one waits for its buttons to be laid out before it clicks, and the stage has the one file from `system_files/` a provisioning test reads. Adopt `image-template`'s `Justfile` and `build.yml`; pin the Bazzite base to a digest (ADR-0006); generate cosign keys; add `SIGNING_SECRET`; push to `ghcr.io/cairn-linux/cairn` as a private package, made public only after P1-14, #86 and P1-17 (ADR-0031). |
| P1-4 | ISO via `bootc-image-builder` (`disk_config/iso.toml`); `build-disk.yml`. Test install on both tiers. The image ships no user accounts and `sshd` disabled, and the ISO configuration does not let the installer create a user; the first account is the Guardian from the first-boot wizard (ADR-0020). Prepared 2026-10-21 (#164, ADR-0038 slice 6): `disk_config/iso.toml` (Anaconda's Users module off, a kickstart so the installer asks before it touches a disk, hostname `cairn`), `tools/build-iso.sh` run once with `sudo`, the builder pinned in `cairn.env`. Built on the dev PC and test-installed in a new VM `cairn-iso` 2026-10-22, from a private test config with a test Guardian: a 6.0 GB ISO, an installer that stops for the language, disk, keyboard and time, and a computer called `cairn` with Cairn Linux boot titles, the groups at their numbers and Cairn's login screen. Left: an install from `iso.toml` itself, with no account (untested), Secure Boot (the base's kernel needs Universal Blue's key enrolled), the installer's Fedora artwork (P1-14), the boot splash, still Bazzite's (#165), whether a Guardian's tile belongs on the login screen (maintainer's question), and `build-disk.yml` with P1-3. |
| P1-5 | Guardian tooling: a CLI first (`create-child`, `set-level`, `set-pin`, `allow-app`, and `install-app` / `remove-app` from the curated list at `catalogue/`, ADR-0015), then a minimal GUI on it. Level changes symmetric (DESIGN §3). Planned first: a Guardian opens a chosen child's own Steam from their own login, to sign in, switch account or change Steam's settings, without ssh or `rc.xml` (ADR-0032, proposed; `docs/research/steam-visit.md`; #131). Its root helper and the Guardian's window are in: "Steam for <child>" in a Guardian's menu opens `guardian/`'s window, which runs `session/bin/cairn-steam-visit` with `pkexec`, closes the Guardian's own Steam for the visit, and closes the visit on Done, the screen lock or its close button. A child who tries to log in meanwhile is refused by PAM, and the login screen says a grown-up is setting something up and to try again soon. ADR-0032 is accepted once the rest of the checks under its Consequences pass: those that need a signed-in Steam, and a fingerprint where a reader exists. Games are installed once, from the Guardian's own Steam, into a folder only Guardians can write, and each child gets their own copy, so saves stay in the child's home (#130); ADR-0033 (proposed) and `docs/research/shared-steam-library.md` say how. The kids' laptop moved over on 2026-10-13, and seven of the sixteen checks ADR-0033 needs have passed there, and the first half of check 11. Provisioning now makes that folder and copies its games into each child's own library, as the child, with the visit helper's lock. It also makes the game tiles from that folder, in `/var/lib/cairn/games.json`, which a re-run keeps and the launcher reads with `--games`, each with the game's picture from the Guardian's own Steam (ADR-0035, proposed, 2026-10-19); not yet run on the laptop. The command under the Guardian's tool landed 2026-10-22 (#167 slice 1, ADR-0039, accepted 2026-10-23): `cairn-family create-child` and `set-level`, a root command in C++ behind its own polkit action that asks for a password for every change. It gives levels 1 and 2 only, refuses any account Cairn did not make, and writes the child's "Steam for <child>" entry. The family's setup on the image makes its children with it. The login screen and the command share one account rule in `accounts/`. The window on it followed 2026-10-23 (#167 slice 2, ADR-0040, accepted 2026-10-23): the Guardian tool, "Family" in a Guardian's menu, a program of its own whose first page, Children, lists each child with their level, adds a child at level 1 or 2 and moves a child up or down with the same button. It makes the login name from the child's name and runs `cairn-family` through pkexec, one change at a time, with one plain sentence for every answer. Both ran in the VM on 2026-10-23, at the Guardian's desktop with the real password prompt. Each child gets their own list of games, chosen on a Games page in the Guardian tool (ADR-0041, proposed 2026-10-27), in six steps; the first, a copy that follows a list, is in, and so is the second, `cairn-family save-games`, the root side of the page's Save, which copies each child's newly ticked games as a system job and writes their own list in `/var/lib/cairn/games/`, the third, a switch between the Guardian tool's pages and a Games page that shows each child's list over a grid of the shared games' pictures and opens the child's own Steam, and the fourth (2026-10-28), highlighting games and Save, with the image's launcher reading each child's own list and the family's setup no longer copying every game. Step 5 followed on 2026-10-29: a logged-in child's games are greyed and blurred with **Log Ada out**, which only logs them out (`cairn-family log-out`, its own password), and Save opens a progress window with a warning not to log out or close the lid, while a `systemd-inhibit` lock keeps the computer from sleeping mid-copy. It replaced the first version's "Later" and "Log them out now". Next: ADR-0041's VM checks (step 6), `reset-child` (P1-8), `set-pin` (#35), and adopting an existing account (#113). |
| P1-6 | First-boot wizard (DESIGN §4.3.1): Guardian account (the only required step), PIN with its warning, an optional security key for the Guardian (ADR-0020), an optional first child, the starter set from the curated list with Tux Paint pre-ticked (ADR-0015), game scan offer, where creations live. Add Wi-Fi setup, which the design doc's list omits and the starter set needs. No secret (a password, the PIN) is ever traced, logged or passed as a command-line argument (#103). |
| P1-7 | Quick-actions PIN overlay with the fixed allowlist (DESIGN §14 Q8): extend time, unlock one app this session. Logging out is the child's own since ADR-0021. Nothing persistent. |
| P1-8 | Child account reset and creations export (DESIGN §11): skeleton home, one protected creations directory, sync to a Guardian-readable location. |
| P1-9 | Time limits and schedules. malcontent does not do this; needs a session-side timer with a calm end-of-time screen and logind integration. (Gap in the design doc; add a section.) |
| P1-10 | Update policy: `bootc upgrade` timer, apply on reboot; rollback documented as a one-command Guardian operation. |
| P1-11 | Documentation site skeleton: install, hardware tiers stated plainly, "what this does not do" (no remote management), rollback, getting creations out. |
| P1-12 | Guardian-loss recovery path (DESIGN §14 Q9) decided and documented. |
| P1-13 | Steam integration (moved from Phase 3 by ADR-0004): client `-silent` at login (done for a child with a Steam tile, ADR-0027), `steam -applaunch` tiles from the `kidscan` manifest (game tiles from the shared folder, with pictures, ADR-0033 and ADR-0035), launch watchdog with the "needs a grown-up" screen (a game Steam is getting ready waits and says so, ADR-0036, proposed; a tap lost to Steam's own update is sent again, ADR-0037, proposed; a Steam that cannot sign in ends the wait soon, #182; Steam's updater windows are hidden while it updates itself, #183), the child role in a Steam Family, Steam Families documented as the family's relationship with Valve, Flatpak-vs-RPM client decision. Builds on P0-10. From P0-10 (ADR-0031): what a child-role account's client locks, once the family has a child member; Steam's toast over a running game; and what a client update, a Steam Guard prompt or a sign-in expiry puts on the child's screen (`docs/research/steam-containment.md`). |
| P1-14 | Base image compliance. Checklist in `docs/research/base-image-policy.md`. Done in the image on 2026-10-21 (ADR-0038, slice 5): `os-release` rewritten, `fedora-logos` swapped for `generic-logos` (a `cairn-logos` package can follow with P1-2), no `fedora-release-notes`, Bazzite's terminal banner and support links removed, Bazzite, Universal Blue and Fedora in `NOTICE`. Left: the hostname and the boot menu titles on a computer that updated from Bazzite, which say Bazzite until #165 rebuilds the initramfs (a fresh install from the ISO has "Cairn Linux" boot titles and is called `cairn` through the installer's kickstart, checked 2026-10-22), the Fedora "not provided or supported" notice on the Guardian tool's About page (#167) and the website, the installer's artwork (bootc-image-builder installs `fedora-logos` there, slice 6), Bazzite's updater and portal menu entries (P1-1), and Cairn's own version in the image labels (P1-3). Blocks publishing any image. |
| P1-15 | High-contrast and large-text modes as per-child Guardian settings, driven by token-set variants from `brand/build.py` (ADR-0008). |
| P1-17 | **Child test 2** (ADR-0031): what Phase 0 did not see. A child opens something from the Terminal, once #83 decides how a child who cannot read meets it; a child leaves an app on their own (#77); a child meets the grown-up screen and presses Back (ADR-0030); Steam games with the overlay off (#74) and a Guardian's way to step in (#88), or no Steam in that run, as #8 asked. Also #8's script rows, none of which an evening ran: a native ScummVM game scanned, launched, quit and launched again; ScummVM frozen and ended with Ctrl-Alt-Home; every help button and outside link in the allowed apps; a stale or unrelated window that cannot trap the child; a sibling at the login screen and the Guardian tile; a level change interrupted part-way, which fails closed; no path from a child's session to Plasma. A child at the tiles with the battery low, and whether a child who cannot read notices the battery's fill (ADR-0034). Whether a game's picture tells the younger child which game it is, and whether the pictures pull the children away from Draw (ADR-0035). On a Minimum-tier laptop if the project has one by then. Blocks publishing any image, like P1-14 and #86. |

---

## 4. Phase 2 — the ladder (L2, L3)

- L2: launcher grows; dock gains the files view, which is the **creations
  gallery** (ADR-0009): everything the child made, from any app, with export
  to a Guardian-readable place (DESIGN §11); ~12-command vocabulary.
- L3: Plasma session with a KDE Kiosk profile; malcontent integration (only if
  P0-7 passed); browser present and filtered (D10).
- Level up and level down both routine from the Guardian tool.
- Expanded shell: real shell at L3, no sudo; decide how the restricted shell
  hands off to bash without a vocabulary cliff.

## 5. Phase 3 — games and polish

- Steam polish on top of P1-13: per-title launch options, cloud-save conflict
  handling, Proton version pinning where a title needs it.
- Minecraft Java as a first-class integration, not a tile.
- `kidscan` GUI for category assignment; pictures for tiles other than games
  decided (DESIGN §14 Q3; games have theirs, ADR-0035).
- Catalogue additions from the software survey (ADR-0009); revisit the
  first-party app question with child-test findings.
- Accessibility, second tranche (ADR-0008): screen reader via AT-SPI/Orca,
  dwell click, review with a specialist.

---

## 6. Parallel workstreams (start any time)

| Workstream | Where | Notes |
|---|---|---|
| **ScratchJr on Linux** | Wish list (issue #23) | Desired feature, deferred by ADR-0010. No repository, no work until the image is stable and a child test has shown whether a young child misses block coding at L1 (ADR-0010); then decide how: port, contribute a Linux patch to the community port, or adopt a package if one exists. |
| **Linux Mark Institute sublicence** | Legal | Free and routine but has lead time. Start now. |
| **Trademark reviews** | Legal | Universal Blue / Fedora / Bazzite derivative policies; Scratch Foundation branding; "Cairn" search in software classes. |
| **Website** `cairnlinux.com` | Separate repo, later | Hold until Phase 1 has something a parent can install. |
| **Brand v0.2** | `docs/brand-guide/` | Iconography, motion (the rule is ADR-0028), avatars, spacing, sound policy, high-contrast and large-text variants. See the review notes in issue tracker. |
| **Guardian boundary audits** | Issues #85 and #86, then `docs/research/` | Can an older child get into a Guardian account or around their level (ADR-0020)? Twice: #85 early, against what Phase 0 built; #86 before the first public image, against the installed image, and it blocks publishing like P1-14. |
| **Software catalogue survey** | Issue tracker, then DESIGN §5.2 | Find existing, maintained, kid-oriented apps for music composing, writing/story, camera and coding so the project does not build or maintain its own (ADR-0009). Criteria: actively maintained; Flatpak or Fedora package; offline; no accounts, ads or telemetry; free licence; Wayland/XWayland; age fit with mouse and keyboard; fits the Minimum tier. |

---

## 7. Risks

| Risk | Signal | Mitigation |
|---|---|---|
| C++ is new to the maintainer and most code is AI-written | Bugs that can't be read; crashes without messages | Conventions in `CLAUDE.md`; sanitizers on in Debug; tiny classes; tests from the first commit; the trust boundaries get the most tests and the plainest code. |
| malcontent doesn't work cleanly on Plasma | P0-7 | Revisit §4.5/§9.1 before Phase 2; KDE Kiosk alone may suffice for app gating. |
| Kiosk escape via VT switch or forced windows | P0-4, P0-10 | logind `HandleXXX` and compositor config; labwc window rules plus foreign-toplevel detection (D3, ADR-0004). |
| Steam client in every child's session | P0-10 RSS on the 4 GB floor; client UI changes under us | Measured 2026-09-08: about 800 MB proportional idle signed out and 1.4 GB signed in, in the VM. The child role in a Steam Family as defence in depth (Family View is gone); the launcher's grown-up screen is the universal fallback. |
| Steam changes client behaviour under us | Phase 1 onward | Detection + graceful failure, never rely on a quiet client. |
| Steam changes the files Cairn reads, which Valve does not document: its manifests and the `librarycache` layout, which already has two forms and two names for a game's picture (ADR-0033, ADR-0035) | After a Steam update, provisioning leaves games out, or notes that new games have no picture | A game with no picture shows its name, and a launch that fails ends on the grown-up screen (ADR-0030). |
| Phase 0 exited on thin evidence | One family, one laptop above the 4 GB floor, no Terminal use seen (ADR-0031) | P1-17 tests what was not seen before an image leaves the family; P1-1 measures the floor. |
| Single maintainer | Always | Inherit upstream; keep first-party code small; document every mechanism. |
| Base image trademark constraints | Reviewed 2026-09-03, no blocker | Compliance checklist in `docs/research/base-image-policy.md`; P1-14 executes it before any image is published. |
| Atkinson Hyperlegible Mono licence | **Closed 2026-09-04** | Atkinson Hyperlegible Next and Atkinson Hyperlegible Mono are both OFL-1.1, verified against the Fedora packages on 2026-09-04 (`docs/DEVELOPMENT.md`). |

---

## 8. How work is tracked

- **GitHub milestones** per phase: Phase 0, Phase 1, Phase 2, Phase 3,
  Pre-release legal.
- **Labels**: `area:launcher`, `area:shell`, `area:session`, `area:image`,
  `area:guardian`, `area:games`, `area:brand`, `area:docs`, `area:provision`,
  `accessibility`, `decision`, `research`, `upstream`, `legal`.
- **ADRs** in `docs/decisions/` for every D-number when it closes.
- `DESIGN.md` changes go through a pull request, even from the maintainer, so
  the reasoning is recorded.
