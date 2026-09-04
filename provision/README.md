# provision

Phase 0 only. A script that turns a stock Bazzite KDE install (ADR-0006)
into a Cairn machine, so the experience can be tested on real hardware and
with a real child **before** any image, ISO or CI exists (DESIGN §13 Phase 0).

Nothing here yet. Phase 0 task **P0-2** builds `cairn-provision.sh`.

## What it must do

1. Create the level groups (`cairn-l1` … `cairn-guardian`).
2. Create one Guardian account (password) and one L1 child account (no
   password, avatar).
3. Install the kiosk compositor (labwc, ADR-0004) and the Phase 0 app set:
   Tux Paint, GCompris (Qt), ScummVM — via Flatpak where available,
   `rpm-ostree`/`bootc` layering otherwise. Steam is already on Bazzite.
4. Install the session entry, dispatcher, and greeter configuration from
   `../session/`.
5. Install the launcher and shell prototypes from `../launcher/` and
   `../shell/`.
6. Be idempotent. Running it twice on the same machine changes nothing the
   second time.

## What it must not become

The product. Everything it does by hand is what the Phase 1 image does by
construction and the first-boot wizard does with a GUI. When Phase 1 ships,
this directory is deleted.
