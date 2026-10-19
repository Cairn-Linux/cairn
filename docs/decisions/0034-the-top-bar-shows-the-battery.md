# ADR-0034: The top bar shows the battery, and says when it is low

**Status:** proposed. It becomes accepted when the checks under
Consequences have run on the kids' laptop.
**Date:** 2026-10-18
**Closes:** (none) — addresses the battery item of #43

## Context

On 2026-10-18 a child played on the kids' laptop from 99% to an empty
battery in under three hours.
Nothing in the child's session said the battery was running down.
At 2% UPower put the laptop to sleep, and it died asleep.

The kiosk has no panel: labwc runs the launcher and nothing else
(ADR-0017), so nothing shows a battery unless the launcher does.
`docs/TRY.md` listed the gap: "nothing warns of a low battery".
Issue #43 asks for the battery's percentage and state in the launcher via
UPower, with a calm warning before sleep.

UPower already does the hard part.
It puts a computer's batteries together into one device, `DisplayDevice`,
and decides when it is low, critical, or at the level where it acts, from
`/etc/UPower/UPower.conf`.
On the dev PC and in the Bazzite VM that file says the same: by
percentage, low at 20%, critical at 5%, the action at 2%, and the action
`Auto`.
UPower's source (1.91.5, `src/up-daemon.c`) adds what the file does not:
the warning level is "none" unless the battery is discharging, and UPower
takes its action 20 s after reaching the action level.

The readings, on 2026-10-18:

- The dev PC, a laptop full and on the charger (UPower 1.91.4): present,
  type 2 (battery), state 4 (fully charged), 100%, warning level 1 (none).
- The VM, with no battery (UPower 1.91.5): not present, type 0 (unknown),
  state 0, 0%, warning level 1.
- The VM's L1 test child could read UPower's properties over the system
  bus, which UPower's D-Bus policy allows by default.
- A UPS, when one is present, takes the batteries' place in
  `DisplayDevice` (type 3), and its warning level 2 means "discharging".

[DESIGN §6.1](../DESIGN.md#61-the-launcher) asks for no reading beyond app
names, and puts Log out above the tiles, always in view (ADR-0021).
The launcher cannot draw over another program's window (ADR-0028).

## Decision

**The bar above the tiles shows the battery whenever UPower has one.**
The battery is at the left, level with the tiles' left edge, and Log out
stays at the right.
The bar is always in view at the tiles and never scrolls away.

**A drawn battery whose fill is the percentage, and a plug while it is on
the charger.**
The plug shows while the battery charges, while it is full, and while the
charger holds it below full, as a charge limit does.
No number shows on the screen; the battery's accessible name carries it:
"Battery, 42 percent", "Battery charging, 42 percent", "Battery on the
charger, 80 percent", "Battery full".

**One sentence when UPower says the battery is low.**
At UPower's low level: "The battery is low. Ask a grown-up for the
charger."
At its critical or action level: "The battery is almost empty. Ask a
grown-up for the charger."
The two levels share a sentence because what UPower does at the action
level is its configuration's to decide, so the words promise nothing
about sleeping.
There is never a sentence while the charger is in.
The sentence is as large as the room beside Log out allows, on two lines
if it must, and never below a child's smallest text (18 px).
No colour, motion, flashing or sound marks it.

**UPower decides, not Cairn.**
The launcher reads `IsPresent`, `Type`, `Percentage`, `State` and
`WarningLevel` from `org.freedesktop.UPower`'s
`/org/freedesktop/UPower/devices/DisplayDevice` on the system bus.
It follows `PropertiesChanged`, from UPower's own process only, and reads
everything again when UPower restarts.
It changes nothing in UPower: not its thresholds, not its critical
action.
Nothing leaves the computer.

**No battery, a UPS, or no UPower: nothing shows.**
A desktop, a virtual machine, or a desktop on a UPS has the bar it had
before.
When UPower cannot be read, the launcher says so in one sentence in the
journal, and the child sees no battery.

**Only at the tiles, not over a running game.**
The bar goes with the tiles, as Log out does.
A warning over a running game needs a surface the compositor draws above
other windows.
That is a bigger decision, about the kiosk and not only the launcher, and
it is left to the maintainer.

**`UPowerDevice` reads, `BatteryLook` decides, `Battery.qml` draws.**
`BatteryLook` is a plain class with the whole decision and is tested
without D-Bus.
`UPowerDevice` is tested against a stand-in UPower on a session bus of
its own.
Qt DBus is part of Qt, and `guardian/` already uses it.

## Consequences

- At the tiles, a child sees how full the battery is, and a child who
  reads, or a grown-up passing by, sees "low" from 20%.
  That is about a fifth of the play time, less on a worn battery: 20% of
  a 45-minute battery is about nine minutes.
- **A child in a game sees nothing, and that is most of the time on the
  laptop.** `docs/TRY.md` keeps "keep a laptop plugged in".
  The ways a later ADR could show a warning over a game:
  - a wlr-layer-shell surface: labwc offers the protocol, Qt has no
    support of its own, and KDE's LayerShellQt would be a new dependency;
  - a notification server in the kiosk;
  - labwc's own on-screen display;
  - raising the launcher at the critical level.
- **Not tried on real hardware running down.**
  It was tested against a stand-in UPower on a private bus, and read from
  the dev PC (full, on the charger) and the VM (no battery).
  No warning level above 1, and no state but 4 and 0, was seen live.
- Checks before acceptance, on the kids' laptop, unplugged and at the
  tiles:
  1. the battery shows, and its fill matches `upower -d`;
  2. plugging in shows the plug within seconds;
  3. at 20%, the low sentence;
  4. at 5%, "almost empty";
  5. a kiosk session reads UPower, and the journal has no line from the
     battery;
  6. the sentence fits at the laptop's resolution in Atkinson Hyperlegible
     Next (the dev PC has only a fallback font);
  7. a desktop, or the VM in the kiosk, shows no battery.
- A child who cannot read gets only the fill.
  There is no warning colour, because the palette has none: ochre means
  make and is for large fills only.
  A warning colour, and the battery's own icon, belong to brand v0.2
  (ROADMAP §6) and the next child test (P1-17).
- The battery is drawn from the brand's sizes (the heading size, the
  hairline, the small radius), not from an icon set.
  It is about as tall as a tile's title.
  Whether a five-year-old notices it at that size is a question for the
  next child test.
- The Terminal (Footpath fills the window), the starting screen, the
  grown-up screen and the question before logging out do not show the
  battery.
- A launcher run without `--log-out` on a laptop, as a grown-up's run
  under Plasma, now has a bar with the battery alone.
  If UPower's answer comes after the first frame, the tiles move down
  once at start.
  The kiosk always has Log out, so there the bar is up from the first
  frame.
- A laptop with a UPS plugged in shows no battery, because UPower then
  reports the UPS. That is rare, and was not tried.
- UPower's own action at 2% is unchanged.
  Whether `Auto` is the right action on Bazzite, given that the laptop
  died asleep, is a separate question for #43.
- Six new strings, translatable, with accessible names from the start
  (ADR-0007, ADR-0008).
  The battery's role is `Indicator`, the nearest Qt has to a level; the
  screen-reader work in Phase 3 revisits it.
- The launcher links Qt DBus.
  `libQt6Gui` already loads it, so nothing new is loaded at run time.
  Its tests need `dbus-run-session`, which `guardian/`'s tests already
  require.

## Alternatives considered

- **Cairn's own thresholds.** There would be two answers to "is it low",
  against inheriting from upstream (CLAUDE.md, non-negotiable 2).
- **A percentage number on the screen.** It needs reading.
- **A lightning bolt for charging.** It says "charging" while the laptop
  is often full, or held at a charge limit. The plug is true in all three
  cases. A child test may show children read a bolt faster; it would need
  QtQuick.Shapes, which is part of Qt.
- **Ochre or red for low.** There is no token for it, and ochre means
  make.
- **A fill that moves while charging.** It would say something true
  (ADR-0028), but nothing needs it yet.
- **Raising the launcher over a game at critical.** It pulls a child out
  of play unasked.
- **A notification daemon in the kiosk.** A new dependency, and a
  decision about what may draw over a game.
- **PowerDevil in the kiosk.** It would bring Plasma into the kiosk.
- **Reading `/sys/class/power_supply`.** It duplicates what UPower puts
  together and decides.
- **Waiting for the L2 dock that #43 names.** The dock does not exist
  yet, and the bar above the tiles is always in view at every level that
  has tiles.
