# malcontent on Plasma: does it gate Flatpaks, and is its UI reachable?

**Date:** 2026-09-06
**For:** ROADMAP P0-7 (issue #6), DESIGN §14 Q7, issue #38
**Result:** Yes on both counts. malcontent 0.14 layered onto stock Bazzite
KDE blocks a blocklisted Flatpak at `flatpak run`, blocks user-repository
installs, and its control app runs under Plasma and asks for a parental
unlock. Nothing in §4.5 or §9.1 needs revisiting. malcontent also ships a
session-time limit and a web filter, which #38 raised; both are recorded
below.

## How it was run

In the `cairn-min` VM (stock Bazzite KDE `44.20260902`, provisioned by
`provision/cairn-provision.sh`, SDDM as the display manager per ADR-0016),
with `malcontent`, `malcontent-control`, `malcontent-pam` and
`malcontent-tools` layered by `rpm-ostree`. A test account `p07` in
`cairn-l3`, with a password, was restricted as root:

```sh
malcontent-client set-app-filter --disallow-user-installation \
  --disallow-system-installation p07 app/org.tuxpaint.Tuxpaint/x86_64/stable
```

`p07` then logged in at the SDDM greeter and, through `cairn-session`,
landed in Plasma 6.7.4. The commands below ran inside that session with its
Wayland display and session bus, and the screen was captured with `virsh
screenshot`. The account was removed afterwards.

## Rows

| Row | Result | What happened |
|---|---|---|
| A blocklisted Flatpak refuses to launch | **Pass** | `flatpak run org.tuxpaint.Tuxpaint` printed "Running app/org.tuxpaint.Tuxpaint/x86_64/stable is not allowed by the policy set by your administrator" and exited 1. `malcontent-client check-app-filter` says the same. |
| An allowed Flatpak still launches | **Pass** | `flatpak run org.kde.gcompris` started and drew its welcome screen. |
| Installing to the user repository is refused | **Pass** | After adding Flathub as a user remote, `flatpak install --user org.kde.kolourpaint` failed with the same policy message. Note that stock Bazzite has no user remote at all, so the first attempt failed for that reason; the filter is what refuses once a remote exists. |
| Installing to the system repository is refused | **Pass, by polkit** | `flatpak install --system` failed with "Flatpak system operation Deploy not allowed for user". That is polkit refusing a non-wheel user, not malcontent; the malcontent flag is a second lock behind it. |
| The restriction UI is reachable under Plasma | **Pass** | `malcontent-control` opened as a Plasma window showing "Parental Unlock Required" with an Unlock button, which is a polkit prompt. A Guardian in `wheel` gets through; a child does not. |

## What malcontent 0.14 also offers (issue #38)

- **Session limits:** `malcontent-client set-session-limits USER
  daily-schedule` (fixed start and end) or `daily-limit` (a fixed amount
  per day), with `request-extension` for a child to ask for more time. The
  pieces are there: `pam_malcontent.so`, `malcontent-timerd.service` and a
  timer extension agent, all installed and all disabled by default. Not
  exercised in this check. If they hold up, P1-9 shrinks to the calm
  end-of-time screen and the Guardian-side "extend" allowlist from
  DESIGN §3.2 on top of malcontent's timer, rather than a timer of its own.
- **Web filter:** `set-web-filter` with `malcontent-webd.service` and an
  update timer, also disabled by default and not exercised. D10 should
  look at it before choosing a DNS resolver, since it is local
  configuration and honours §9.3.

## Two things worth knowing

- The greeter's own buttons (Sleep, Restart, Shut Down) sit beside the
  avatar tiles and work for anyone at the keyboard, because the greeter
  session is the active one on the seat. That is the polkit row already
  owed by #35, not a malcontent matter.
- malcontent identifies apps by Flatpak ref. Layered RPMs such as ScummVM
  and Steam are outside its filter; those are gated by the launcher's
  manifest and, for Steam, by the Family allowlist (ADR-0004).
