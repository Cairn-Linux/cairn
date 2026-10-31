# ADR-0019: A child's own way out of an app

**Status:** accepted
**Date:** 2026-09-29
**Closes:** (none) — addresses issue #77; amends ADR-0018
**Amended by:** the section "Tux Paint's Print button is off" below
(2026-10-31, #221)

## Context

At L1 and L2 there is no titlebar, no taskbar and no Alt-Tab (ADR-0017).
ADR-0018 gave a grown-up one way out of a stuck program, Ctrl-Alt-Home, and
nothing to the child: its context says a child cannot leave a window on
their own, and "That is the point." [DESIGN
§4.5](../DESIGN.md#45-sessions-per-level) said the way out never appears to
the child as a button to find.

The first pilot with a child, on 2026-09-25 (#8), showed what that costs. A
six-year-old in the VM could not leave an app without help, each time. Every
app has its own quit: Ctrl-Q in ScummVM, a button and a question in Tux
Paint, a power icon in GCompris. A pre-reader cannot find them, and the frame
gave no one answer. Phase 0's exit criterion asks a child to launch and
return from two apps unsupervised (ROADMAP §2), which cannot pass while
leaving needs an adult. Footpath's design already says that leaving is the
first thing a child should be able to do alone
(`external/footpath/DESIGN.md` §6).

On 2026-09-29 the maintainer decided that a child may close an app on their
own, and that a shortcut is enough.

## Decision

**Super+Q asks the window in front to close.** One keybind in
`session/labwc/rc.xml` runs labwc's `Close` action: a Wayland app gets
`xdg_toplevel.close` and an X11 app `WM_DELETE_WINDOW`, the same request a
titlebar's close button sends. It is `Close`, not `Kill`, so the app decides
what happens next, and an app that asks about unsaved work still asks. The
exception is an old X11 app that does not take `WM_DELETE_WINDOW`: it is
disconnected at once, as a titlebar's close button would do. Every app checked
here takes the request. The key acts when it is released, not when it is
pressed. labwc repeats a held binding 25 times a second, and a held key
would open and close Tux Paint's question over and over; on release it
closes once, however long the child holds it.

**In the launcher the same key is one step back, and never closes it.** The
launcher is the session: labwc runs it with `-S` and ends when it does
(ADR-0012), so if it closed, the child would be logged out. When the launcher
is in front, the request reaches its window, and `CloseRequest`
(`launcher/src/`) answers it the way Escape answers on the screen that is
showing. The Terminal goes back to the tiles, and `exit` still works there. A
failed launch's grown-up screen goes back to where the child was. At the
tiles nothing happens. On the grown-up screen for a window that opened on
its own nothing happens either: the launcher never dismisses that screen
itself, and only the window closing ends it. Only the kiosk
makes the launcher fullscreen, so a windowed launcher is a grown-up running
it under Plasma, and there it closes like any other window.

**Tux Paint runs with `--autosave --saveovernew`.** Closing it asks "Do you
really want to quit?" with a tick and a cross, which a pre-reader can
answer. With these options it then saves the picture without asking, as a
new file when it changed an old one. A child who leaves never loses a
drawing and is never asked whether to save it; the one question left is
answered with a picture.

**Ctrl-Alt-Home stays as ADR-0018 decided**, for a frozen app, which cannot
answer a polite close. This ADR amends ADR-0018's premise that the child has
no way out; its mechanism and the power button are unchanged.

Super+Q is the maintainer's choice. Super on its own and every other Super
combination still do nothing. Q is "quit" in most apps (Ctrl-Q) and on a
Mac (Cmd-Q), so the letter carries over to a real computer later. The chord
itself does not: Plasma, the L3 session, binds Meta+Q to its activity
switcher by default and gives every window a close button, so whether L3
keeps Super+Q is for the L3 session work.

## Consequences

- One key leaves every app. How a child learns it is the first-run guide's
  first lesson (#78, #79); until that exists, a grown-up shows it once.
- Apps keep their own behaviour when asked to close, because the frame does
  not reskin them (CLAUDE.md non-negotiable 3). Tux Paint asks first, and a
  second Super+Q at its question means "No, take me back". Holding the key
  does nothing until it is let go, then acts once. GCompris and ScummVM
  close at once. A game's unsaved progress is the game's business,
  as it is with any close button.
- The key closes whichever window is in front, including one the child did
  not open. Closing such a window also ends the grown-up screen that named
  it, as closing it any other way always has. Steam's own windows are
  iconified by the frame and never in front (ADR-0017), so the key does
  not reach them, and their grown-up screen still waits for a grown-up.
- The launcher now decides what a close request means. That decision is a
  small class with its own tests, because a wrong answer would end the
  child's session.
- The test laptop needs a Super key. Nearly every PC keyboard has one.
- Changing the key later is one line in `rc.xml`, as for the grown-up's
  combination (#72).
- Checked on 2026-09-29 in the VM, under a real seat, against Tux Paint,
  GCompris, a Steam game, the Terminal, the grown-up screen and a frozen
  app (`docs/research/kiosk-containment.md`).

## Tux Paint's Print button is off (2026-10-31, #221)

In a user test on the cairn-iso VM on 2026-10-31, ada tapped Print in
Draw, then "Yes, print it!". Tux Paint answered "Sorry! Your picture could
not be printed!", and the journal had `lpr: Error - No default
destination.` Tux Paint prints by running `lpr` inside its Flatpak
sandbox. The sandbox has no `cups` socket and no network, so no picture
can be printed from it, whether the computer has a printer or not.
Printers are the Guardian's
([DESIGN §3.2](../DESIGN.md#32-the-guardian-role)), and the Guardian tool
does not link to Plasma's printer settings yet (ADR-0040).

**Draw starts Tux Paint with `--noprint`.** Mason chose it on 2026-10-31.
It is Tux Paint's own option, like `--autosave`, not a reskin. The Print
button stays in its place, grey like Undo on a new picture, and a tap on
it does nothing. Ctrl+P does nothing either.

The option goes on the command line beside `--autosave --saveovernew`,
not in a configuration file. The system-wide file the Flatpak reads,
`/app/etc/tuxpaint/tuxpaint.conf`, is part of Flathub's build, so changing
it would mean building Tux Paint ourselves. The user's own file,
`~/.tuxpaintrc`, is outside what the sandbox can see: only `~/.tuxpaint`
is kept for it, in `~/.var/app/org.tuxpaint.Tuxpaint/`. Using it would
take a Flatpak override and a file in every child's home. The command
line is read at every start and wins over both files.

- A child never meets a Print button that can only say "Sorry!".
- The Draw tile and the Terminal's `open` both start Tux Paint from the
  manifest, so both have Print off.
- A grown-up who opens Tux Paint from Plasma's menu still has Print, and
  gets the same "Sorry!": the Flatpak cannot print for anyone.
- Tux Paint quits at once on an option it does not know, so a misspelt
  option would put every Draw launch on the grown-up screen. A launcher
  test reads the manifest the image installs and pins Draw's whole
  command line.
- Printing is looked at again when the Guardian tool links to Plasma's
  printer settings (ADR-0040). Then Tux Paint could get the `cups` socket
  from a system Flatpak override and print where a Guardian has set up a
  printer, with `--printdelay` to limit how often a child can print.
- Checked in the source of Tux Paint 0.9.35, the version Flathub builds:
  the option marks the button as not available, and neither a tap nor
  Ctrl+P prints. Seen nested on the dev PC with Fedora's 0.9.35: the
  button is grey where it was blue. Not yet seen in the kiosk on the VM.
