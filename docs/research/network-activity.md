# Network activity: what the image contacts without being asked

**Date:** 2026-11-12
**For:** issue #36 ("Nothing phones home": inventory the image's network
activity) and ROADMAP P1-1
**Result:** A first pass, read from the image's files. The image switches
on several things that contact outside machines by themselves (listed
under "In short"), and Cairn's build changes none of them. Nothing has
been captured on the wire yet, so this list says what is switched on and
where its settings point, not what is sent.
The proposals at the end are for the maintainer to decide.

## What this is and how it was made

This is the inventory issue #36 asks for.
It is the first part of that issue, not the end of it.

The question asked of every item was the same.
Can it reach a machine outside the computer without a person asking for
it at that moment: on a schedule, at start-up, at login, or when a
network comes up?
If it can: what starts it, how often, where does it go, what does it
send that could tell one computer or family from another, and does
Cairn's image already change it?

**The image.**
Cairn built from main at 57bd89a, on Bazzite 45.20261104 (Fedora 45).
On the development computer it is tagged `cairn:dev-57bd89a`.

**How it was read.**
Every file was read inside a container made from that image, with no
network and no systemd running:

```sh
podman run --rm --network=none --cpus 2 localhost/cairn:dev-57bd89a <command>
```

The commands were plain ones: `cat`, `ls -la`, `grep`, `rpm -qf`,
`rpm -V`, `rpm -q --whatrequires`, `strings`, `ldd`,
`systemctl --root=/ list-unit-files`, `systemctl --root=/ is-enabled`,
the same two with `--global` for the units of each account, and
`uupd config-dump`.
Four passes were made: the system's units (694 unit files), what starts
at login, the settings that name addresses, and the project's own
documents.
Where two passes disagreed, the image was read again to settle it.
Three checkers then read the image against this document, and each of
their findings was read in the image once more before the document was
changed.

**What the test VM added.**
The test VM has shown some of these things running, on 2026-11-04,
2026-11-06, 2026-11-09 and 2026-11-12, and on a stock Bazzite install
from 2026-09-06.
What it showed comes from its journal, its process lists and its
screenshots.
The VM never ran main at 57bd89a itself.
On those dates it ran test images built from branches.
Nobody started the VM for this document.
What is quoted is from earlier runs, in the wording an audit of those
runs supports.

**How each fact is marked.**
The last column of every table says one of four things.

- *Image files*: read in the image as above.
- *Test VM, date*: seen on the test VM on that date, in its journal, a
  process list or a screenshot.
- *Upstream's page*: the project that makes the program says so; the
  addresses are listed at the end.
- *Not known*: nobody has looked, or looking at files cannot tell.

**What this method cannot show.**

- Nothing was captured on the wire. No request was read in full.
- A file's settings do not prove what is sent. Where this document says
  what a request carries, it is from the program's own text in the
  image, a journal line or an upstream page.
- A unit file that exists is not a unit that runs. Each row says how the
  unit is switched on and what condition can stop it.
- A container shows what an installed computer starts with. It does not
  show what a first start changes.
- A fresh install from the Cairn ISO has not been checked against this
  list.

**A note on "no package owns the link".**
All 140 links that switch system units on in this image belong to no
package.
Fedora images make them at build time from preset files.
So this document says whether a preset asks for the unit, which tells
more.
The image's `/etc/machine-id` is an empty file, so the presets are not
applied again at the first start.

## In short

Today the computer does these things by itself:

- Every few days a timer sends Fedora's and Terra's servers a count
  that names "Cairn Linux 45" and gives a rough age of the install.
  Whether every run sends it, or only one a week, is not known.
- While it is online, its settings ask it to fetch one small page from
  Fedora every five minutes.
- It asks public time servers for the time.
- Near 04:00 each day the updater runs. It looks for a new system,
  which it cannot get until a Cairn image is published. It tries
  Homebrew, a tool no Cairn screen uses. It updates apps from Flathub
  when there are any.
- Once a day a timer runs a key check for secure name look-ups; whether
  it reaches outside is not known.
- Steam starts by itself at each desktop login.
- At home it answers to `cairn.local`, and KDE Connect announces it.

Cairn has turned off Bazzite's first-login step, login notices and
terminal banner, and stopped the welcome window opening for new
accounts.
Older accounts still open it, and it is still in the menu.
Nothing here was measured on the wire yet.

## The inventory

The tables are grouped by when a thing happens.
Rows are numbered so the proposals further down can name them.
"The home's internet address" means the address every outside server
sees for any request from the home; it is not repeated in every row.
On a network with IPv6 that is not quite so: a request can leave from an
address of this one computer.
The base image makes that a temporary address that changes from time to
time (`use_tempaddr=2` in `/usr/lib/sysctl.d/75-networking.conf`, a file
no package owns).

### 1. At start-up and on a schedule

These start with the computer, before anyone logs in.
They are the same whoever uses the computer: a Guardian, a child at
level 3 or 4, or a child at level 1 or 2.

| # | What it is | What starts it, how often | Where it goes | What it could tell | How we know |
|---|---|---|---|---|---|
| 1.1 | The count of computers: `rpm-ostree-countme.timer`, `rpm-ostree-countme.service` | Fedora's preset asks for it (`90-default.preset`). 5 minutes after start plus a random wait of up to a day, then 3 days after its last run plus the same wait. Runs only where `/run/ostree-booted` exists, which is true on an installed computer. | The mirror lists of the five enabled package sources that ask to be counted: `mirrors.fedoraproject.org` (Fedora's two) and `tetsudou.fyralabs.com` (Terra's three) | The words "Cairn Linux 45", the kind of processor, and an age group from 1 to 4 | Image files; test VM, 2026-11-12; upstream's pages |
| 1.2 | The updater, system part: `uupd.timer`, `uupd.service` | A link in the image that no package owns and no preset asks for (the presets say disable). How the base image came to have it is not known. Daily at 04:00 plus up to 15 minutes; a missed run is made up after the next start. | The place the installed system came from. For an install from today's ISO that is `localhost/cairn:dev` as a registry (ADR-0038): the computer itself, so no outside machine. | Not known | Image files; ADR-0038; test VM, several dates |
| 1.3 | The updater, apps part (Flatpak) | The same run as 1.2 | Flathub, `https://dl.flathub.org/repo/` | Not known | Image files; test VM, 2026-11-04 and 2026-11-06 |
| 1.4 | The updater, Homebrew part | The same run as 1.2 | `github.com` (`Homebrew/brew`, by its git settings) and, by Homebrew's manual, `formulae.brew.sh` for its package list; perhaps a usage report | Not known. If a usage report goes: command names, processor type, system and Homebrew version, by upstream's page. | Image files; test VM, 2026-11-04 and 2026-11-06; upstream's pages and source |
| 1.5 | The clock: `chronyd.service` | Fedora's preset asks for it. Every start, then it keeps asking. | Time servers of `2.fedora.pool.ntp.org`, and any the router names | No line in its settings names an identifier | Image files |
| 1.6 | A daily key check: `unbound-anchor.timer`, `unbound-anchor.service` | Fedora's preset asks for it. Once each day, at a time systemd picks within that day (`AccuracySec=24h`); a missed run is made up; no condition stops it. | By upstream's page: the name servers the network gives, then the root name servers, then `data.iana.org` if those fail | No line in its settings names an identifier | Image files; upstream's page. What a run does here: not known |
| 1.7 | Flatpak set-up: `flatpak-add-fedora-repos.service`, `bazzite-flatpak-manager.service` | Both enabled. The first runs once. The second runs in full at the first start; after that, only when an image update changes its script or the image name in `image-info.json`. | Flathub, if these commands contact it at that moment | Not known | Image files. Whether they contact Flathub: not known |
| 1.8 | Homebrew set-up: `brew-setup.service` | A Bazzite preset asks for it (`01-homebrew.preset`). First start only. | Nowhere: it unpacks a file that is in the image | Nothing | Image files |

**1.1, the count.**
The unit calls itself "Weekly rpm-ostree Count Me reporting".
It runs `rpm-ostree countme` as a throw-away user.
Seen on the test VM on 2026-11-12: a system timer that nobody started
sent five count-me requests to two outside hosts, about 13 minutes after
the boot.
Its next run was set for 2026-11-15.
That VM ran a test image built from a branch, not main; the timer reads
the same in this image.

Read in the image's files: five enabled package sources in
`/usr/share/dnf5/repos.d` carry `countme=1`.
They are `fedora` and `updates`, whose mirror list is at
`mirrors.fedoraproject.org`, and `terra`, `terra-extras` and
`terra-mesa`, whose mirror list is at `tetsudou.fyralabs.com`.
A sixth, `updates-testing`, carries it too but is switched off.
Each of those addresses names the kind of processor (`arch=`).

The program builds the name it gives itself from `os-release`.
Its text holds "rpm-ostree (" and a message for when `NAME` or
`VERSION_ID` is missing.
Cairn's build writes `os-release` with `NAME="Cairn Linux"` and
`VERSION_ID=45`.
Very few computers in the world say that name.
So the count tells Fedora's and Terra's servers that a Cairn computer
exists at that internet address.

DNF's manual says the number is an age group worked out on the
computer: 1 for the first week, 2 for the first month, 3 for up to six
months, 4 for longer.
It says the age counts from when `/etc/machine-id` last changed.
That file is empty in the image and is filled at the first start.
The manual names no machine number being sent.
It also says only one request a week carries the mark.
The `rpm-ostree` program in the image holds the words "window already
counted", so a run in a week already counted may send nothing.
That was not seen: the VM showed one run.

`rpm-ostree`'s own page says a computer with no added packages fetches
no package lists at all.
That is why the count has a timer of its own.
The page calls the timer "bi-weekly"; the unit in the image waits 3
days, which is about twice a week.

The image's file `/etc/dnf/repos.override.d/99-config_manager.repo`
turns the Terra sources off for `dnf`.
Whether the count honours that is not known; the capture should show
it.

**1.2 to 1.4, the updater.**
The program is `uupd` 1.5.7, Universal Blue's updater.
The timer is `OnCalendar=*-*-* 04:00:00`, `RandomizedDelaySec=15m`,
`Persistent=true`.
The service runs `/usr/bin/uupd --log-level=debug --json`.
When a run fails, systemd starts it again twice more, 60 seconds apart
(no more than three starts in ten minutes).
The test VM showed exactly that on 2026-11-06.
Its settings file `/etc/uupd/config.json` switches off one part only,
distrobox.
`uupd config-dump` in the image shows the system, Flatpak and Homebrew
parts on.

It also shows checks before a timed run: battery at least 20 percent,
processor at most 50 percent busy, memory at most 90 percent full, and
the network not busier than 700,000 bytes a second.
By upstream's source, a timed run also asks NetworkManager to repeat
its internet check (row 3.1), up to six times, 3 seconds apart.
It goes on only if the answer is full connectivity; otherwise the run
fails.
On a network marked as metered it stops quietly: "Skipping updates,
network is metered".
The menu's updater (row 5.16) runs `uupd --hw-check=false`, without
these checks.
Whether those checks act as written was not seen.

What the test VM has shown, in the wording the audit of those runs
supports:

- On the stock install, four nights with an image update downloaded 2.2
  to 3.6 GB. Eleven automatic runs began between 04:04 and 04:14.
- On earlier Cairn test images the automatic run did reach outside:
  Homebrew reported two updated sources, and Flatpak app updates of
  12.4 MB (2026-11-04) and 42.3 MB (2026-11-06) were fetched.
- On 2026-11-06 the updater also ran a Flatpak update as the test child
  ada, who was logged in, and as the installer's account, bazzite.
- On 2026-11-06 the updater tried four times to send a desktop notice,
  three times to ada and once to the installer's account. Each try was
  logged as failed. Whether anything showed on the screen is not known.
- The updater did not restart the computer: the service does not use
  the restart option.
- The real updater has not run on the image the VM had on 2026-11-12.

That VM held 43 system apps; this image holds none.
Whether the Flatpak part contacts Flathub when no app is installed is
not known.

Homebrew is unpacked from a file in the image on the first start and
given to uid 1000, whoever that turns out to be (`chown -R 1000:1000`
on Homebrew's folder).
On the test VM that was the first account made.
On a fresh install from the Cairn ISO, which makes no account, it is
not known yet.
It has no packages installed.
By upstream's source the updater runs `brew update` and then
`brew upgrade -y` as the owner of Homebrew's folder.

`/etc/environment` sets `HOMEBREW_NO_AUTO_UPDATE=1`.
By Homebrew's manual that only stops the update before an install or
upgrade, not an explicit `brew update`.
No file in the image sets `HOMEBREW_NO_ANALYTICS`.
Homebrew's copy in the image is marked as having shown its usage-report
notice already (`analyticsmessage = true` in its git settings, and no
`analyticsdisabled`).
By upstream's source, its usage reports are then on from the first run,
and nobody will see the notice.
Upstream's page says a report holds no user identifier.
The nightly `brew update` and `brew upgrade` may each send one; not
seen.

A child can start the updater today.
The package's polkit file `/usr/share/polkit-1/rules.d/uupd.rules`
answers yes to any account for `uupd.service` and
`uupd-manual.service`, and Cairn's `10-cairn-levels.rules` has no line
for them at main.
Roadmap #265 already carries this.
Pull request #304, not merged yet, adds a rule that refuses a child.

`/usr/share/ublue-os/image-info.json` still names Bazzite's registry.
It is text other programs show.
The updater uses the installed system's own source, not this file.

**1.6, the key check.**
The tool keeps the root key for secure name look-ups (DNSSEC).
Unbound, the name server it is named for, is not installed.
GnuTLS's DANE library names the same key file, and the image's `wget`
(`wget2`) links that library and Unbound's library.
Whether a plain `wget` run uses the key is not known.
The service runs
`/usr/sbin/unbound-anchor -f /etc/resolv.conf -R` as user `unbound`.
The key file it would write is a chain of links,
`/var/lib/unbound/root.key` to `/etc/unbound/dnssec-root.key` to
`/usr/share/dns-root-data/root.key`.
The last is in the read-only part of the system, from the package
`dns-root-data`, so each image update brings the key anyway.
Whether a run can finish is not known.

**1.7, Flatpak set-up.**
The path of `flatpak-add-fedora-repos.service` belongs to the `flatpak`
package, but the base image replaced its content (`rpm -V` says so).
It adds Flathub from `/etc/flatpak/remotes.d/flathub.flatpakrepo` and
adds Fedora's two app sources switched off.
It runs while `/var/lib/flatpak/.ublue-initialized` does not exist.
`/usr/libexec/bazzite-flatpak-manager` belongs to no package.
It leaves at once if the version it wrote in `/etc/bazzite` matches its
own text and the image name matches `image-info.json` (`bazzite`).
The image has no `/etc/bazzite`, so it runs in full at the first start.
If it fails, its unit starts it again 30 seconds later, with no limit.
It switches Flathub on, blocks the Flatpak builds of Steam and Lutris,
removes two old leftovers, sets permissions, copies Firefox defaults
and removes unused runtimes.
It has no line that installs an app.

### 2. When somebody logs in

The login screen offers one session, `cairn-session`.
It reads the account's groups and starts one of two things.

- **A Guardian, or a child at level 3 or 4, gets the Plasma desktop.**
  Plasma runs every file in `/etc/xdg/autostart` and in the account's
  own `.config/autostart` folder, and every unit hung on
  `graphical-session.target`.
- **A child at level 1 or 2 gets the kiosk**: labwc with Cairn's
  launcher.
  The script starts `pipewire-pulse.service` and then
  `labwc -C /usr/share/cairn/labwc -S cairn-launcher`.
  That folder holds only `environment` and `rc.xml`: no `autostart`
  file.
  labwc's own sample in the image says a line in an `autostart` file is
  what would start `graphical-session.target`, and there is none.
  So by the files, no autostart file and no session unit starts rows
  2.1, 2.2 and 2.4 to 2.8 at level 1 or 2.
  A program in the kiosk could still wake 2.4, 2.6 or 2.8 through
  D-Bus.
  The full list of what runs in a kiosk session has not been read on a
  running system.

**At every login of both kinds.**
Each account's own systemd starts a few units at any login.
By the files these are `bazzite-dynamic-fixes.service` (it writes or
removes one settings file in the home), `mpris-proxy.service`,
`unity-gtk-module.service`, the sockets for D-Bus, PipeWire and
`podman`, systemd's own local sockets, and the timers
`grub-boot-success.timer` and `systemd-tmpfiles-clean.timer`.
No line in them names an address.
Only in Plasma: `dmemcg-booster-user`, `xdg-user-dirs`, `ntfs-nag`
(row 5.17), `kunifiedpush-distributor` (row 2.5), `spice-vdagent` and
`uresourced`.

Cairn's own programs at login (the launcher, the session scripts, the
login screen's module, the Guardian tool) link no Qt network part, and
no line in their sources names an address.

| # | What it is | What starts it, in which session | Where it goes | What it could tell | How we know |
|---|---|---|---|---|---|
| 2.1 | Steam, from the new-account template: `/etc/skel/.config/autostart/steam.desktop` | Every Plasma login of an account whose home was made from the template: a Guardian, a child at level 3 or 4. The file belongs to no package and runs `/usr/bin/bazzite-steam -silent %U`. | Valve's servers, by what Steam is. Which addresses: not known; not watched | Not known. Once signed in, the family's Steam account | Image files; test VM, 2026-11-12 |
| 2.2 | Bazzite's first-run check for Steam: `/usr/bin/bazzite-steam-firstrun` | Run by 2.1 at every Plasma login until the account has finished a Steam set-up | `https://store.steampowered.com/about`, with `curl` | The script adds nothing of its own to the request | Image files. The request itself was not watched |
| 2.3 | Steam for a child at level 1 or 2 | Cairn's launcher, only when the child's list has a Steam game: `systemd-run --user --unit=cairn-steam … steam -silent` | As 2.1 | As 2.1 | Cairn's source; test VM, 2026-11-12 |
| 2.4 | KDE Connect: `/etc/xdg/autostart/org.kde.kdeconnect.daemon.desktop` | Every Plasma login. D-Bus can also start it (`org.kde.kdeconnect.service`). | The network the computer is on, not the internet: a broadcast, and ports 1714 to 1764 | A device id and a device name, to anything on the same network | Image files; upstream's pages. Not watched |
| 2.5 | KDE's push-notice helper: `kunifiedpush-distributor.service` | Every Plasma login, through a link the package owns in `/usr/lib/systemd/user/graphical-session.target.wants/`. The package also has an autostart file, `org.kde.kunifiedpush-distributor.desktop`, which Plasma skips when it starts through systemd (`X-systemd-skip=true`). | Its settings name `https://unifiedpush.kde.org/` | Not known | Image files; upstream's source. Whether it connects: not known |
| 2.6 | Plasma's network part (module `networkmanagement`) | Every Plasma login | It asks NetworkManager to repeat its check (row 3.1) | As row 3.1 | Image files |
| 2.7 | Bazzite Portal, the welcome window (`bazzite-portal`, program yafti) | New accounts: nothing starts it. An account made before Cairn removed the template's file still opens it at every Plasma login. It is also in the start menu's favourites, and pinned to the task bar by the default panel template (`layout.js`, which the base image changed) and by `bazzite-pins.js`. Its search plug-in, `krunner-yafti`, is a package of its own. | No line in the program names an address. Its list links to Bazzite's documents, Discord, Discourse and Reddit, opened when pressed. | Not known | Image files; test VM, 2026-11-12 |
| 2.8 | Bazaar, the app store, through the start menu's search box | Typing in the search box of a Plasma session can start `/usr/bin/bazaar-daemon --no-window` through D-Bus. Bazzite's settings switch the search plug-in on (`bazaarrunnerEnabled=true`). Its per-account unit, `io.github.kolunmi.Bazaar.service`, is linked in no folder. Bazaar is also in the start menu's favourites. | Not known. Addresses in the program: Flathub's API, pictures and app source (`flathub.org/api/v2`, `imgproxy.flathub.org`, `dl.flathub.org`) and `arewelibadwaitayet.com` | Not known | Image files. Whether it starts, and what it fetches: not known |

**2.1 and 2.2, Steam at a desktop login.**
Seen on the test VM on 2026-11-12: the Guardian's Steam client started
silently at login from the account's own autostart entry.
The Guardian tool makes a child's home from the same template, so a
child at level 3 or 4 gets the file too.
At level 1 or 2 the copy is in the home but nothing reads it.
The Guardian tool also restarts the Guardian's own Steam with
`/usr/bin/bazzite-steam -silent` (`guardian/src/OwnSteam.h`), so row
2.2 can follow a Guardian pressing a button there too.

The first-run script runs this line while the home has no Steam yet:
`curl --http1.1 -Ls https://store.steampowered.com/about`.
If a page comes back it shows a box, "Steam is opening for the first
time".
That box was seen at the test child ben's level 3 login on 2026-11-12.
The request behind it was not watched.

**2.3, Steam at level 1 or 2.**
This one is Cairn's own doing (ADR-0027): the game then starts faster.
A child with no Steam game on the home screen has no Steam at login.
Seen on the test VM on 2026-11-12: in ada's level 1 session the Steam
client started by itself.
The launcher runs plain `steam`, not Bazzite's wrapper, so row 2.2 does
not happen at level 1 or 2.

**2.4, KDE Connect.**
Upstream's source says that at start, and at each change of network, it
sends a broadcast to find phones, listens for them, and announces itself
by mDNS.
On somebody else's Wi-Fi that reaches everybody there.
Removing the autostart file alone may not stop it: the system tray's
KDE Connect part can ask D-Bus to start it.
The image's firewall lets other machines reach these ports (row 4.4).

**2.5, the push-notice helper.**
`systemctl --global is-enabled` calls the unit "disabled", yet the link
under `/usr/lib` starts it with every Plasma session.
Upstream's source says it connects only when a program has signed up
for notices.
No installed program was found that signs up.
A program a person installs later could.

**2.7, Bazzite Portal.**
Seen on the test VM on 2026-11-12: the window, with entries for
Discord, Discourse and Reddit, opened by itself when ben logged in at
level 3; no such process ran in the level 1 and level 2 sessions.
It was started by an autostart file in the child's own home, as old as
the home itself (2026-10-01).
In the one sample it did not open at the Guardian's login.
The window has a "Launch at startup" switch that writes the autostart
file back.
Cairn has nothing that clears the file from older homes.

**2.8, Bazaar.**
The program's text holds the words "--run-refresh-worker" and
"automatic store data refresh is paused", so once started it may
refresh the store's data by itself.
Its automatic app updates are off by default (`auto-update` is false,
and no file changes it), and it skips its update check on a metered
network.
Whether it stays running after a search wakes it is not known.

**What a file in an account's own home can switch back on.**
The image locks none of this.
A file in `.config/autostart` starts any program at every Plasma login.
A unit in `.config/systemd/user`, or a line in `.bashrc` or
`.bash_profile`, runs at every login of both kinds.
Plasma's own settings can turn on Night Light (which asks for the
location), the automatic time zone and KDE's usage feedback.
Whether a child at level 1 or 2 can write such files from the kiosk was
not looked at here.

### 3. When a network comes up

| # | What it is | What starts it, how often | Where it goes | What it could tell | How we know |
|---|---|---|---|---|---|
| 3.1 | NetworkManager's check that the internet works | `NetworkManager.service` (Fedora's preset asks for it). Whenever a network is up, every 300 seconds, and again when Plasma or the updater asks (rows 2.6, 1.2). | `http://fedoraproject.org/static/hotspot.txt`, plain HTTP | No line in its settings names an identifier. What the request carries: not known | Image files; upstream's page. Not looked for in a journal |
| 3.2 | Asking the router for an address | NetworkManager, each time it joins a network | The router of that network | The computer's name, `cairn`, by NetworkManager's settings reference. On most Wi-Fi cards, a hardware address that stays the same for that one network and is not the real one. On Broadcom cards that use the `wl` driver, and on a wired connection, the real one. | Image files; upstream's pages |
| 3.3 | Time servers the router names | chrony's own dispatcher script `20-chrony-dhcp` (package `chrony`), run by `NetworkManager-dispatcher.service` (enabled) at each network change, writes them to `/run/chrony-dhcp`, which `chronyd` reads (row 1.5) | Whatever the router names | As row 1.5 | Image files |

**3.1, the check.**
The setting comes from the package
`NetworkManager-config-connectivity-fedora`, file
`/usr/lib/NetworkManager/conf.d/20-connectivity-fedora.conf`:
`enabled=true`, `uri=http://fedoraproject.org/static/hotspot.txt`,
`response=OK`, `interval=300`.
`/etc/NetworkManager/conf.d` is empty, so nothing changes it.
It is what makes a hotel's or cafe's login page appear.
Fedora's server sees the home's internet address every five minutes
while the computer is on.
Upstream's source for the updater says a timed update run stops unless
NetworkManager reports full connectivity.
What NetworkManager reports when its check is turned off is not known.
Read that before deciding, because it decides whether timed updates
would still run.

**3.2, the name and the hardware address.**
The installer writes `cairn` to `/etc/hostname`
(`disk_config/iso.toml`, ADR-0038).
`DEFAULT_HOSTNAME="cairn"` in Cairn's `os-release` gives the same name
if that file is missing.
Every Cairn computer gives the same name unless the family changes it.
A name of 20 characters or more is changed to `bazzite` by
`bazzite-hardware-setup.service` (enabled).
It runs at the first start, and again when its own version, the image's
name or branch, or the Fedora version changes.
The router and the home network would then see `bazzite`.

The Wi-Fi setting is `wifi.cloned-mac-address=stable-ssid` in
`22-wifi-mac-addr.conf`.
For cards with the `wl` driver, `90-broadcom-wl.conf` (package
`broadcom-wl`) sets `preserve`, which keeps the real address.
NetworkManager's page says a file read later wins, so for those cards
the real address goes out.
Nothing sets a wired address, and the settings reference says an unset
one means `preserve`.

On a network with IPv6, outside servers can see an address of this one
computer rather than the home's (see the start of the inventory).

### 4. Things that listen or announce on the home network only

None of these goes to the internet by its settings.
"Home network" means whatever network the computer has joined; on a
school's or cafe's Wi-Fi it is that one.

| # | What it is | What starts it | What it does there | What it could tell | How we know |
|---|---|---|---|---|---|
| 4.1 | `avahi-daemon.service` and `.socket` | A preset asks for them. Every start. | Announces and answers for the computer's name, `cairn.local` (or `bazzite.local`, see 3.2), and its addresses | The name. `publish-hinfo=no` and `publish-workstation=no` keep hardware details out | Image files |
| 4.2 | Short-name look-ups in `systemd-resolved.service`, and `.local` names | A preset asks for it. Every start; acts when a program asks for a name. | Asks the network for short names (`LLMNR=resolve`). It does not answer others. Names ending in `.local` are asked by multicast too (`mdns4_minimal` in `/etc/nsswitch.conf`, from `nss-mdns`). | The name being looked for | Image files |
| 4.3 | Printing: `cups.socket`, `cups.path` | Both enabled. `cups.service` starts only through them. | Listens on this computer only (`Listen localhost:631`). Once started it can announce printers this computer shares, by DNS-SD (`Browsing Yes`, `BrowseLocalProtocols dnssd`). Finding other machines' printers is `cups-browsed`'s job, and it is off (row 5.15). | Not known | Image files, with the `cupsd.conf` manual page in the image. Whether it announces anything while it listens on this computer only: not known |
| 4.4 | The firewall: `firewalld.service`, zone `FedoraWorkstation` | A preset asks for it. Every start. | A filter: no line in its settings names an outside address. It lets other machines reach ssh, samba-client, dhcpv6-client, and every port from 1025 to 65535. | Nothing by itself | Image files |
| 4.5 | KDE Connect | Row 2.4 | Row 2.4 | Row 2.4 | Row 2.4 |
| 4.6 | `ModemManager.service` | A preset asks for it. Every start. | Talks to a mobile-data modem if the computer has one | Not known | Image files |
| 4.7 | `rpc-statd-notify.service` | Static; `nfs-client.target` is enabled and wants it | Tells file servers the computer restarted. By its manual page in the image, it tells only the machines on its list in `/var/lib/nfs/statd/sm`; the image has no `/var/lib/nfs` at all. | Not known | Image files. What it sends on a running system: not known |
| 4.8 | Bluetooth: `bluetooth.service` | A preset asks for it (`enable bluetooth.*`). Every start. `FastConnectable = true`; the radio's power-on setting is left at its default. | Devices already paired can connect. By BlueZ's defaults the computer is not visible to new devices unless a person opens pairing. | The computer's name, to devices nearby when it is visible | Image files. Whether it becomes visible while the Bluetooth settings page is open: not known |

**Names are looked up through the router.**
`systemd-resolved` has no settings of the image's own.
No line names an outside name server, and no fall-back servers are set.
It does nothing by itself.

**4.4, the firewall.**
Nothing listens for ssh: `sshd.service` and `sshd.socket` read disabled.
But with this zone, any program that listens on a high port can be
reached from the same network.
That is a question for the Guardian audit (#86) as much as for this
list.

### 5. Installed but switched off, or waiting for a person

By the files, nothing in this group contacts an outside machine by
itself.
Some of it does start at login and then waits (5.4, 5.8, 5.17, 5.18,
5.19).
Each row says what keeps it off or what it waits for.

**Switched off by a setting or a condition.**

| # | What it is | What keeps it off | Where it would go | How we know |
|---|---|---|---|---|
| 5.1 | `dnf-makecache.timer` (refreshes package lists) | Its link is there, but timer and service carry `ConditionPathExists=!/run/ostree-booted`, so neither runs on an installed computer | Fedora's mirrors | Image files |
| 5.2 | Greenboot's two network checks, in `greenboot-healthcheck.service` | Both are named in `DISABLED_HEALTHCHECKS` in `/etc/greenboot/greenboot.conf` | Package sources and update servers | Image files |
| 5.3 | Firmware lists: `fwupd-refresh.timer`, `fwupd.service`, `passim.service` | The timer reads disabled. The two services are static: they start only when a program asks. No autostart file or account unit names them. | `https://cdn.fwupd.org/downloads/firmware.xml.zst`. `AutomaticReports=false` and `AutomaticSecurityReports=false`. | Image files. Whether a Plasma page asks: not known |
| 5.4 | The location service: `geoclue.service` | Not off: at every Plasma login the helper in `/etc/xdg/autostart` registers with it, which starts it through D-Bus. By upstream, it looks up a location only when a program asks for one. | `https://api.beacondb.net/v1/geolocate`, with the Wi-Fi access points nearby, or with the internet address alone (`[ip] enable=true`), by upstream's pages. `submit-data=false`. | Image files; upstream's pages. Whether it looks anything up with no program asking: not known |
| 5.5 | Automatic time zone (Plasma module `geotimezoned`) | Not loaded unless a person turns it on | `https://geoip.kde.org/v1/timezone` | Image files; upstream's source |
| 5.6 | KDE's usage feedback | The default level is "NoTelemetry" and no file in `/etc/xdg` sets another | KDE, when a person turns it on | Image files; upstream's page |
| 5.7 | Weather, comic strip and picture-of-the-day widgets; online accounts | None is on the default desktop or panel; no account exists | Each service's own servers, when a person adds one | Image files |
| 5.8 | Automatic proxy search (Plasma module `wpad_detector`) | Loads at login, but acts only when the proxy setting is "detect automatically". The image sets no proxy. | The local network | Image files; upstream's source |
| 5.9 | Remote desktop: `krdpserver`, `krfb` | `krdpserver`: its per-account unit is off by a per-account preset (`00-krdp.preset`). `krfb`: no unit and no preset; nothing starts it. Neither has an autostart file. | Would listen on the network | Image files |
| 5.10 | `tailscaled.service` | Reads disabled. Its package source is switched off for `dnf` by the override file, though the source's own file says enabled. It carries no `countme` line. | Tailscale | Image files; issue #36's comment |
| 5.11 | `sshd.service`, `sshd.socket` | Read disabled | Would listen on the network | Image files |
| 5.12 | `waydroid-container.service`, `brew-update.timer`, `brew-upgrade.timer` | A preset says enable, but no link exists and presets are not applied again at first start | Homebrew's servers for the two timers | Image files. A fresh ISO install: not checked |
| 5.13 | `rpm-ostreed-automatic.timer`, `bootc-fetch-apply-updates.timer`, `podman-auto-update.timer`, `systemd-sysupdate-update.timer` (also called `systemd-sysupdate.timer`), `systemd-sysupdate-reboot.timer` | Read disabled. `/etc/rpm-ostreed.conf` has `AutomaticUpdatePolicy=none`. The image has no `sysupdate.d` files, so `systemd-sysupdate` would have nothing to fetch. | The image's source | Image files; test VM, 2026-11-12 |
| 5.14 | `fedora-atomic-desktop-appstream-cache-refresh.service` | Masked by the base image | Fedora | Image files |
| 5.15 | File sharing and servers: `wsdd`, `smb`, `nmb`, `rpcbind`, `nfs-server`, `cockpit`, `mariadb`, `cups-browsed`; `podman.socket` for the system. Also a container unit for Cockpit's web console, `/usr/share/containers/systemd/cockpit-container.container`, which no package owns. | Read disabled. The Cockpit container has no install line, so nothing starts it. | Would listen on the network. The Cockpit container would fetch `quay.io/cockpit/ws:latest` and run it with the whole disk. | Image files |

**Waits for a person to press something.**

| # | What it is | What it does when pressed | How we know |
|---|---|---|---|
| 5.16 | "Bazzite Updater" in the menu (`bazzite-updater`), and `ujust update` | Starts `uupd-manual.service`, the updater without its checks before a timed run. The window also reads a news feed from `universal-blue.discourse.group` and offers to switch the computer to a Bazzite image. | Image files |
| 5.17 | `ntfs-nag.service` (every Plasma login) | Watches for drives. Opens a page of Bazzite's documents only when "Learn more" is pressed. The notice is signed "Bazzite". | Image files |
| 5.18 | KDE's yearly donation notice (Plasma module `donationmessage`) | Opens a kde.org page when pressed. A child at level 3 or 4 would see it. | Image files |
| 5.19 | `/etc/profile.d/debuginfod.sh` (every login, both kinds) | Sets an address at `debuginfod.fedoraproject.org` that only a debugging tool uses, when a person runs one | Image files |
| 5.20 | `podman.socket` for each account | A local socket; no network address | Image files |
| 5.21 | The start menu's unit converter (search plug-in `unitconverter`) | When a person types an amount of money in the search box, it can fetch exchange rates from `www.ecb.europa.eu`. Nothing in `krunnerrc` turns it off. | Image files |

**5.3, firmware.**
Upstream's page says a firmware download is logged with the internet
address, the time, the file's name and the program's name.
`passim.service` would share firmware lists with other computers on the
home network; it starts only when `fwupd` asks.

**5.4, the location service.**
`/etc/geoclue/conf.d/99-beacondb.conf` belongs to no package; the base
image added it.
Upstream's page says a look-up is logged with the internet address, the
time and the program's name for 28 days.
`/etc/xdg/autostart/geoclue-demo-agent.desktop` starts at every Plasma
login.
It is the helper that says yes or no to a program asking for the
location, and no line in it names an address.
When it starts it registers with the location service, and the
service's D-Bus file starts `geoclue.service` for that.
A person can turn Night Light on in System Settings, and by upstream's
source it then asks for the location.
Nothing in the image stops a child at level 3 or 4 from doing that.

**Not in the image at all.**
Discover and its update notifier, Plasma's welcome window, KDE's crash
reporter window, PackageKit, Firefox and abrt are not installed.
Issue #36 named Discover's notifier; it is not here.
`/etc/xdg/PlasmaDiscoverUpdates` asks for unattended updates, but no
program that reads it is installed.
It would start to matter if Discover were ever added.

**Already taken out by Cairn's build.**
Each has a check script that stops the build if it comes back.

| What | Build file | Check |
|---|---|---|
| Bazzite's first-login step, `bazzite-user-setup.service`, with its root script and polkit files. It made each account Tailscale's operator. | `build_files/bazzite-extras.sh` | `build_files/check-bazzite-extras.sh` |
| The autostart file that showed Bazzite's notices at login | `build_files/name-and-logos.sh` | `build_files/check-name-and-logos.sh` |
| Bazzite Portal's autostart file in the template for new accounts | the same | the same |
| The terminal banner and the menu's link to Bazzite's documents | the same | the same |

**Looked at and found local.**
`fstrim.timer`, `logrotate.timer`, `raid-check.timer` and
`systemd-tmpfiles-clean.timer` keep disks and logs tidy.
`bazzite-hardware-setup.service` and the other Bazzite and bootc
start-up units run local commands only; no line in them fetches
anything.
The one exception is the long-name change in row 3.2.
The enabled sockets other than avahi's and cups's listen on a path
under `/run`, not on a network address.
`sssd.service` is enabled but has no settings file, so it does not
start.
The tools for running inside a virtual machine (`qemu-guest-agent`,
`vboxservice`, `vmtoolsd`, `vgauthd`, and the `spice-vdagent`,
VirtualBox and VMware autostart files) talk to the host machine, not
the network.
`99-nvme-nbft-connect.sh` (package `nvme-cli`), run when a network
comes up, acts only on a link named `nbft…`, which only a computer set
up to start from network disks has.
`python3-sentry-sdk` is installed; no package needs it, and no file in
the image outside the package itself uses it.
A program a person installs later could.

## Proposed, for the maintainer to decide

Each row that runs gets one proposal: keep and tell parents, turn off
in the image, or remove from the image.
Where the choice hangs on something not yet decided, the proposal is
only "decide with that".
None of this is done.
No build file was written for this document.

**How the build would do any of these.**
The build already has the pattern.
A script in `build_files/` makes the change, as `bazzite-extras.sh`
does.
A check script beside it, run as its own step of the `Containerfile`
after `build.sh`, reads the image and stops the build if a base update
brings the thing back, as `check-bazzite-extras.sh` does.
Each proposal below says what the change is and what the check would
read.

| # | What | Proposal |
|---|---|---|
| 1.1 | The count of computers | Turn off in the image |
| 1.2 | The updater, system part | Decide with the update decision (P1-10, P1-3) |
| 1.3 | The updater, apps part | Decide with the update decision |
| 1.4 | The updater, Homebrew part | Turn off in the image |
| 1.8 | Homebrew set-up | Keep for now; taking Homebrew out is for the maintainer to weigh |
| 1.5, 3.3 | The clock | Keep and tell parents |
| 1.6 | The daily key check | Turn off in the image |
| 1.7 | Flatpak set-up | Keep and tell parents |
| 2.1, 2.2 | Steam at a desktop login, and its first-run check | Turn off in the image, for new accounts |
| 2.3 | Steam for a child at level 1 or 2 with a Steam game | Keep and tell parents |
| 2.4, 4.5 | KDE Connect | Remove from the image |
| 2.5 | KDE's push-notice helper | Remove from the image |
| 2.7 | Bazzite Portal | Remove from the image |
| 2.8 | Bazaar's search plug-in | Turn off in the image |
| 3.1, 2.6 | NetworkManager's check | Keep and tell parents; look again with the update decision |
| 3.2 | The name `cairn` given to the router | Keep and tell parents |
| 4.1 | `cairn.local` on the home network | Keep and tell parents |
| 4.2 | Short-name look-ups | Keep and tell parents |
| 4.3 | Printing | Keep and tell parents |
| 4.4 | The firewall's open ports | Decide with the Guardian audit (#86) |
| 4.6 | `ModemManager.service` | Keep and tell parents |
| 4.7 | `rpc-statd-notify.service` | Decide after the capture |
| 4.8 | Bluetooth | Keep and tell parents |
| 5.4 | The location service | Decide after the capture |
| 5.1 to 5.15, except 5.4 | Everything switched off | Keep off, and add a check that it stays off |
| 5.16 | "Bazzite Updater" in the menu | Decide with the update decision |
| 5.17, 5.18 | The drive notice and KDE's donation notice | Decide with P1-14's list of what shows Bazzite's or KDE's name |
| 5.19, 5.20, 5.21 | The debugging address, the account's `podman` socket, the unit converter | Keep; none does anything on the network by itself |

### Turn off or remove

**1.1, the count: turn off in the image.**
It serves Fedora's and Terra's statistics, not the family.
A count of computers is not a call a parent asked for, and the request
names Cairn.
Cost to a family: none.
Cost to others: Fedora and Terra count fewer computers, and those
counts are how they learn who uses their work.
How: mask `rpm-ostree-countme.timer`, which is upstream's own way to
stop it.
Check: `systemctl --root=/ is-enabled rpm-ostree-countme.timer` reads
`masked`, and no link to it is left in `timers.target.wants`.

**1.4, Homebrew in the updater: turn off in the image.**
No Cairn screen uses Homebrew, nothing is installed with it, and it
belongs to whichever account has uid 1000.
Each night the updater contacts GitHub for it all the same, and its
usage reports count as accepted.
Cost: a Guardian who installs a program with Homebrew must update it by
hand.
How: add Homebrew to the parts switched off in `/etc/uupd/config.json`,
the way the base image switches distrobox off.
Also set `HOMEBREW_NO_ANALYTICS=1` in `/etc/environment`, for a
Guardian who runs it by hand.
The updater's unit file does not read `/etc/environment`, so that line
may not reach the nightly run.
So also set `analyticsdisabled = true` in the git settings of
Homebrew's copy when the image is built.
Check: `uupd config-dump` shows the Homebrew part disabled, the line is
in `/etc/environment`, and Homebrew's git settings say
`analyticsdisabled = true`.

**1.8, Homebrew set-up: keep for now.**
Its unit only unpacks a file; no line in it contacts anything.
Taking Homebrew out of the image whole (a 158 MB file and
`brew-setup.service`) is a larger step, left for the maintainer to
weigh.

**1.6, the daily key check: turn off in the image.**
The image already brings the key with each update, and the only reader
found is the secure-look-up part of `wget`, when a person runs it.
The name server the tool is named for is not installed.
Cost: none found.
How: mask `unbound-anchor.timer`.
Upstream's own switch is the other way: `DISABLE_UNBOUND_ANCHOR=yes`
in `/etc/sysconfig/unbound`.
No package requires `unbound-anchor`, so removing it is a third way.
Check: `is-enabled` reads `masked`.

**2.1 and 2.2, Steam at a desktop login: turn off in the image, for new
accounts.**
A family that has not chosen Steam gets a start of Steam, and a request
to Valve's shop, at every desktop login.
Steam is the family's choice, so it should start when the family has
made that choice.
Cost: a Guardian who uses Steam opens it by hand, and its downloads
start then, not at login.
Whether the Guardian's windows for a child's Steam (ADR-0032, ADR-0048)
lean on the Guardian's own Steam already running was not checked here.
The Guardian tool itself starts that Steam with Bazzite's wrapper,
which can make the first-run request (2.2).
How: remove `/etc/skel/.config/autostart/steam.desktop`, as the build
already removes Bazzite Portal's file beside it.
Check: the template's `autostart` folder holds no file.
Homes that already exist keep their copy.
For a child at level 3 or 4 this is better decided with the older
child's session, which is not designed yet.

**2.4 and 4.5, KDE Connect: remove from the image.**
It announces a device id and a name to every device on whatever network
the computer joins, in an older child's session as well as a
Guardian's.
The firewall leaves its ports open.
A parent may well want it, to link a phone; that is a reason to let a
Guardian add it, not to run it for everyone.
Cost: no phone link until a Guardian installs it.
How: remove the packages `kde-connect`, `kdeconnectd` and
`kde-connect-libs`; nothing else in the image requires them.
Removing only the autostart file is not enough, because D-Bus can start
the program.
Check: `rpm -q` says none is installed, and neither the autostart file
nor `org.kde.kdeconnect.service` is left.

**2.5, the push-notice helper: remove from the image.**
It runs in every desktop session with a KDE server's address in its
settings, and nothing installed uses it.
It has two ways to start: the unit and an autostart file.
Cost: a program a person installs later that uses KDE's push notices
would not get them.
How: remove the package `kunifiedpush`, which nothing requires; that
takes the unit, the autostart file and its System Settings page.
The other way is to mask the unit for every account and hide the
autostart file as well.
Check: `rpm -q` says it is not installed, or both the mask in
`/etc/systemd/user` and the hidden autostart file are there.
`is-enabled` called this unit "disabled" while it ran, so that word is
not a check.

**2.7, Bazzite Portal: remove from the image.**
A welcome window with links to Discord and Reddit is not for a child's
computer, and P1-1 already says to disable it.
Its "install this" entries are Bazzite's choices, not Cairn's.
Cost: none for a Cairn family.
How: remove the packages `bazzite-portal` and `krunner-yafti`, which
nothing requires.
Take its entry out of the start menu's favourites
(`kicker-extra-favoritesrc` in the KDE settings profile), out of the
default panel template's `layout.js` and out of `bazzite-pins.js`.
Check: `rpm -q` says neither is installed, and none of the three files
names `yafti`.
Older homes keep their autostart file.
With the program gone it should start nothing; check that in the VM
with ben's account.

**2.8, Bazaar's search plug-in: turn off in the image.**
Typing in a search box should not wake an app store.
What the store's background program fetches then is not known, which is
a reason to stop it until someone has looked.
Cost: apps from the store no longer show in the start menu's search.
The store itself stays for the Guardian.
How: set `bazaarrunnerEnabled=false` in the KDE settings profile's
`krunnerrc`, or remove `krunner-bazaar`, which nothing requires.
Check: the line reads false, or the package is not installed.

### Keep and tell parents

"Tell parents" means a line in the parents' documents (P1-11) that says
what it is, where it goes and why it stays.

**1.5 and 3.3, the clock.**
A wrong clock breaks logins to web sites and breaks updates.
A parent would want the time kept.
Tell parents: the computer asks public time servers, under Fedora's
name, for the time.
Turning it off would cost a correct clock.

**1.7, Flatpak set-up.**
It is what gives the family Flathub at all, and the apps a Guardian
chooses come from there.
Tell parents: the computer knows Flathub as its app source.
The capture should show whether these two units contact Flathub at
start.

**2.3, Steam for a child at level 1 or 2.**
It runs only for a child whose Guardian put a Steam game on the home
screen, so the family has asked for Steam.
Tell parents: that child's Steam starts at login, out of sight, so the
game opens faster (ADR-0027).

**3.1 and 2.6, NetworkManager's check.**
It is what makes a hotel's or cafe's login page appear, and the
updater's own check leans on it.
Tell parents plainly: while it is online the computer fetches one small
page from Fedora every five minutes, and Fedora's server sees the
home's internet address each time.
Turning it off would cost the login page opening by itself, and
perhaps timed updates.
If the maintainer decides to turn it off: a file of the same name in
`/etc/NetworkManager/conf.d` shadows the packaged one, or the package
`NetworkManager-config-connectivity-fedora`, which nothing requires,
comes out.
The check would read NetworkManager's settings for `enabled=false`.
Look at this again once the update decision is made.

**3.2, the name given to the router.**
Every Cairn computer says `cairn` unless the family changes it.
Tell parents: the router and the home network see that name, and a
name of 20 characters or more turns into `bazzite`.
P1-14's list of Bazzite's names could carry that last part.

**4.1, 4.2 and 4.3: `cairn.local`, short names, printing.**
These are how a printer is found and how the computer is found by name
at home.
Hardware details are already kept out of the announcement.
Tell parents: on the home network the computer answers to
`cairn.local`.
Turning them off would cost finding a printer without typing its
address.

**4.6, `ModemManager.service`.**
It does nothing unless the computer has a mobile-data modem.

**4.8, Bluetooth.**
A family will want a pair of headphones or a game controller to work.
Tell parents: Bluetooth is on, and paired devices can connect.
By BlueZ's defaults the computer is not visible to new devices; whether
it becomes visible while the settings page is open is for the capture.

**5.19, 5.20 and 5.21.**
None of these does anything on the network by itself.
The unit converter fetches exchange rates only when someone types an
amount of money; tell parents that the search box can do that.

### Decide with something else

**1.2 and 1.3, updates of the system and of apps.**
Updates are something a parent would want.
How they reach a family's computer is not decided: P1-10 is the update
policy, and P1-3 puts the image where a computer can fetch it.
Until then this document proposes nothing for the updater beyond
"decide with that".
Three facts for that decision.
The updater is `uupd` at 04:00 with catch-up, not a `bootc upgrade`
timer.
It updates a logged-in child's own apps as that child, and has tried to
send notices to a logged-in child.
A child can start it today (#265); pull request #304, not merged yet,
refuses a child.

**5.16, "Bazzite Updater" in the menu.**
A Guardian needs some way to update by hand, so this goes with the same
decision.
The entry carries Bazzite's name and offers to switch the computer to a
Bazzite image, which P1-14 already lists.

**5.17 and 5.18, the drive notice and the donation notice.**
Neither goes online unless a button is pressed, but both put text in
front of a Guardian or an older child: one signed "Bazzite", one asking
for money for KDE once a year.
Decide them with P1-14, beside the other places that show another
project's name.

**4.4, the firewall.**
Which ports other machines may reach is a question of who can get in,
not of what goes out.
Decide it in the Guardian audit (#86), with this row as its input.

**4.7, `rpc-statd-notify.service`.**
By its manual it has nobody to tell on a computer that never mounted a
file server.
Look in the capture first.

**5.4, the location service.**
It starts at every desktop login, and Night Light needs it.
What it does then with no program asking is not known.
Look in the capture first.

**5.1 to 5.15, except 5.4: everything switched off.**
Keep it off.
One check script could read each of these, so a base update that turns
one on stops the build.
It would read the link folders themselves (system and per-account, in
`/etc` and `/usr/lib`), the mask files, the two condition lines for
`dnf-makecache`, the greenboot line, the `fwupd` report settings, and
`/usr/share/containers/systemd` for the Cockpit container.
`is-enabled` alone is not enough.
It says `static` for services D-Bus starts (`fwupd`, `passim`), and it
called a per-account unit "disabled" while it ran (2.5).
For 5.12, whether a fresh ISO install applies the presets at its first
start is a check on an installed computer.

## Sentences in the project's documents this changes

Line numbers are main's at 57bd89a.
The sentences quoted here were not changed.
ROADMAP's P1-1 gains one sentence that points at this note.
"Contradicted" means the inventory shows otherwise.
"Too narrow" means true of part and silent on the rest.
"Unproven" means nothing read or seen shows it yet.

**About "Nothing phones home".**

- `CLAUDE.md` 21-23: "Nothing phones home. No telemetry, analytics,
  accounts, or network calls the user didn't ask for."
  Contradicted for the whole image: the count (1.1) and the updater's
  calls for Homebrew and Flathub (1.3, 1.4) are calls nobody asked for.
  It holds as a rule for what Cairn itself builds.
  Would be true: "Nothing Cairn builds phones home. The base image
  still makes calls of its own; `docs/research/network-activity.md`
  lists them."
- `README.md` 66-68: "Nothing phones home. Cairn's design excludes
  project accounts, telemetry and analytics. Optional services such as
  Steam have their own accounts and network requirements."
  Too narrow: the second sentence is true, the heading says more, and
  the interim wording of issue #36 is not there.
  Would be true: issue #36's two sentences, quoted under "Still to do",
  in place of the heading's claim.
- `docs/DESIGN.md` 97: "Nothing phones home. No telemetry, no accounts,
  no analytics. See §9.3."
  Contradicted as a description of the image today; it stands as a
  principle.
  Would be true: "Nothing Cairn builds phones home, and the image turns
  off what it inherits that does. Until issue #36 closes, the inherited
  system still contacts upstream services."
- `docs/DESIGN.md` 770: "The system collects nothing and requires no
  account to function."
  Too narrow: Cairn collects nothing, but the system sends a count that
  names it.
  Would be true: "Cairn collects nothing and requires no account to
  function. What the inherited system sends is listed in
  `docs/research/network-activity.md`."
- `docs/decisions/0032` 352-353: "Nothing phones home. The only network
  traffic is Steam's own, for an account the family chose."
  `docs/decisions/0048` 670-674: "The only network traffic is Steam's
  own".
  Too narrow, and true of what each decision adds.
  Would be true: "This decision adds no network traffic but Steam's
  own".

**About updates.**

- `docs/DESIGN.md` 792-793: "Update cadence inherited from the base
  image. `bootc upgrade` on a schedule, applied at reboot."
  `docs/ROADMAP.md` 998 (P1-10): "Update policy: `bootc upgrade` timer,
  apply on reboot".
  Too narrow: the image's updater is `uupd`, daily at 04:00 with a
  catch-up after a missed run; it also updates Flatpak apps and
  Homebrew; `bootc-fetch-apply-updates.timer` is off; the service does
  not restart the computer.
  Would be true: "Today the base image's updater, `uupd`, runs daily
  and after a missed run, for the system, Flatpak apps and Homebrew. A
  new system is used from the next restart, and the updater does not
  restart the computer."
- `docs/decisions/0038` 675-678: "An installed computer updates the way
  the VM did in slices 3 to 5, from an archive, until P1-3 publishes
  the image. Its automatic image check fails until then, and reaches
  nothing outside the computer."
  Too narrow, and unproven for an ISO install.
  The same nightly run reached Homebrew's and Flathub's servers on the
  test VM on 2026-11-04 and 2026-11-06.
  Would be true: "Its automatic image check fails until then. The same
  run still updates Flatpak apps and Homebrew from the internet."
- `docs/decisions/0038` 864-865: "A computer installed from the ISO
  cannot update itself until the image is in a registry (P1-3)".
  Too narrow: the system cannot; apps and Homebrew still update by
  themselves.
  Would be true: "cannot update its system by itself".
- `docs/decisions/0038` 375: "`bootc` and `uupd` read nothing from
  `os-release` but text to show."
  Too narrow: `rpm-ostree`'s count puts the name from `os-release` into
  its request.
  Would be true: the same sentence, and "`rpm-ostree`'s count sends the
  name and version to Fedora's and Terra's mirror lists."
- `README.md` 70-71: "Upstream keeps shipping updates whether or not
  this project is active that month."
  `docs/DESIGN.md` 243-245: "The base image keeps receiving updates
  whether or not this project is actively maintained that month."
  Unproven for a Cairn install: upstream ships them, but a Cairn
  computer gets system updates only when a Cairn image is built and put
  where it can be fetched.
  Would be true: add "A Cairn computer receives them when a new Cairn
  image is built from that base."
- `docs/decisions/0007` 24-29: updates fail calmly when offline, "never
  with a blocking dialog in a child's session".
  `docs/DESIGN.md` 921 says they "fail calmly".
  Unproven offline: nothing was watched offline.
  Separately, an online run on 2026-11-06 tried to send desktop notices
  to a logged-in child, and whether anything showed is not known.
  Would be true: mark it as a goal still to be checked in a child's
  session, offline and online.

**About what the image holds.**

- `docs/ROADMAP.md` 989 (P1-1): "Disable Bazzite's first-boot portal,
  Game Mode session and Waydroid."
  `docs/decisions/0006` 50-51 lists the same.
  Too narrow: the inventory adds the count, Homebrew, the key check,
  KDE Connect, the push-notice helper, the Steam autostart file and the
  search plug-in.
  Waydroid's unit already reads off.
  Would be true: the same sentence, pointing at the proposals in this
  note for the rest.
- `docs/DEVELOPMENT.md` 93-95: "The image ships no Flatpaks, because
  they are the family's choice".
  Holds for files.
  It leaves out that two start-up units add Flathub as a source and the
  nightly updater then updates whatever apps exist.
  Would be true: add "The image does add Flathub as the app source at
  first start."
- `docs/decisions/0038` 343: `HOME_URL` is "A link a person clicks;
  nothing fetches it".
  Unproven either way; nothing read contradicts it.

**Named in no document at main.**
The count; Homebrew; the updater running a Flatpak update as logged-in
accounts and its notices; NetworkManager's check (only ADR-0050 points
at it); the time servers; the daily key check; Bazzite's first-run
request for Steam; the push-notice helper.
KDE Connect is named only as a place that shows the computer's name
(`docs/decisions/0038` 411 and 526), not as something that announces
itself on the network.
`docs/decisions/0038` 526 does say the name goes out in DHCP and mDNS,
as rows 3.2 and 4.1 find.

**Checked and still true.**
`sshd` is disabled (`docs/ROADMAP.md` 992).
Bazzite's first-login step is out and `tailscaled` is installed and off
(`docs/ROADMAP.md` 460-469; issue #36's comment).
The updater's rule lets any account start it (`docs/ROADMAP.md` 1042).
Bazzite's updater and portal menu entries are still in the image
(`docs/research/base-image-policy.md` 97-99).

## Still to do

**The interim public wording stays in force.**
Issue #36 says public text keeps Cairn's own code apart from the
inherited system until the inventory closes:

> Cairn's own components do not include telemetry or network reporting.
> The complete upstream image is still being audited and may contact
> upstream update or service endpoints.

And: "Do not publish the broader 'nothing phones home' claim as a
verified property of the whole image until the capture and inventory in
this issue support it."
This note is the inventory's first pass.
The capture is not done.

**The capture on a running system.**
Use a computer or VM installed fresh from the ISO of the image being
judged, with a new Guardian and new test children: ada at level 1 with
a Steam game, cy at level 1 without one, ben at level 3.
Record every packet that leaves it, from outside the computer (on the
VM's host or on the router), so nothing on the computer can hide from
the recording.
On the computer, keep the journal and read
`systemctl list-timers --all` at the end of each part.
Record whether each request leaves over IPv4 or IPv6.

| Part | How long | What to look for |
|---|---|---|
| First start, nobody logged in | 1 hour | Names asked of DNS; time servers; the connectivity check every 300 s and what its request carries; whether the Flatpak set-up contacts Flathub; mDNS and LLMNR; what the computer tells the router (name, hardware address) |
| The count | From 5 minutes before the time `systemctl list-timers rpm-ostree-countme.timer` gives, to 30 minutes after | Which hosts, and what each request carries; whether the Terra requests go though the sources are off for `dnf` |
| One timed updater run, with the system's source reachable, and one without | The whole run | Which hosts, how much data; whether `formulae.brew.sh`, `analytics.brew.sh` or another Homebrew report host is contacted; whether the Flatpak part contacts Flathub with no app installed; how many connectivity checks it asks for; whether it skips on battery and on a metered connection; whether anything shows on a logged-in child's screen |
| The key check | From the time `systemctl list-timers unbound-anchor.timer` gives, or a whole day | Whether `unbound-anchor.service` runs to the end, and whether it reaches the root name servers or `data.iana.org` |
| A second count, a week later | One run | Whether a run in a week already counted sends anything |
| Guardian's first Plasma login | From before the password to 10 minutes after | Steam and the request to `store.steampowered.com`; broadcast and ports 1714 to 1764 (KDE Connect); any connection to `unifiedpush.kde.org`, `api.beacondb.net`, `geoip.kde.org`; `systemctl status geoclue fwupd passim` right after the login; whether Bluetooth becomes visible while its settings page is open |
| ben's level 3 login, home made new on this image | The same | The same; and that no welcome window opens |
| ada's level 1 login, then cy's | The same | Steam for ada and none for cy; `systemctl --user list-units --state=running` and `ps -u ada -o pid,cmd`, to confirm nothing from `graphical-session.target` or D-Bus runs in the kiosk that should not |
| In a Plasma session: type a word in the start menu's search box; type an amount of money; then turn Night Light on | 5 minutes each | Whether Bazaar's background program starts, what it fetches (Flathub, `arewelibadwaitayet.com`) and whether it stays; whether `www.ecb.europa.eu` is contacted; whether the location service starts and what goes to `api.beacondb.net` |
| The first start after an image update that changes `/usr/libexec/bazzite-flatpak-manager` | 15 minutes | The Flatpak set-up's second kind of run |

After each proposal that is accepted and built, run the same parts
again and confirm the thing is gone from the recording.

**Not known today.**

1. What any request carries on the wire.
2. Whether Homebrew sends a usage report in the updater's run.
3. Whether `unbound-anchor.service` finishes, fails, or reaches
   outside.
4. Whether the two Flatpak set-up units contact Flathub.
5. Whether the push-notice helper connects when no program has signed
   up.
6. What the location service does after the login helper wakes it,
   with no program asking.
7. Whether a Plasma page wakes `fwupd` and fetches its list.
8. Whether Bazaar's background program starts from a search, what it
   actually fetches, and how often.
9. What Steam contacts, and whether Bazzite's first-run request
   happens as its script reads.
10. Whether the count sends on every run or once a week, and whether it
    honours the `dnf` override that turns Terra's sources off.
11. What the updater does once a Cairn image has a place to be fetched
    from, and whether its checks before a timed run act as upstream's
    source says.
12. What a fresh install from the Cairn ISO switches on, and which
    account gets Homebrew.
13. What NetworkManager tells the router beyond what its settings show.
14. What `rpc-statd-notify.service` sends on a running system; by its
    manual, nothing while it has no list.
15. Whether the updater's Flatpak part contacts Flathub when no app is
    installed.
16. How the base image came to have the link that switches
    `uupd.timer` on.
17. What runs in a level 1 or 2 session beyond the units the files
    show (section 2), including anything D-Bus starts.
18. What NetworkManager reports to the updater when its internet check
    is turned off.

## Upstream pages relied on

- The count: <https://coreos.github.io/rpm-ostree/countme/>,
  <https://dnf5.readthedocs.io/en/latest/dnf5.conf.5.html>,
  <https://dnf.readthedocs.io/en/latest/conf_ref.html>
- The updater: <https://github.com/ublue-os/uupd>,
  <https://raw.githubusercontent.com/ublue-os/uupd/main/README.md>, and
  its source on the same site at tag v1.5.7: `cmd/update.go`,
  `checks/hardware.go` and `drv/brew/brew.go`
- NetworkManager's check and the order of its settings files:
  <https://networkmanager.dev/docs/api/latest/NetworkManager.conf.html>
- The name sent to the router and the Wi-Fi hardware address:
  <https://networkmanager.dev/docs/api/latest/nm-settings-nmcli.html>
- The key check:
  <https://nlnetlabs.nl/documentation/unbound/unbound-anchor/>
- Homebrew's usage report: <https://docs.brew.sh/Analytics>,
  <https://raw.githubusercontent.com/Homebrew/brew/main/Library/Homebrew/utils/analytics.sh>;
  its package list and `HOMEBREW_NO_AUTO_UPDATE`:
  <https://docs.brew.sh/Manpage>
- Firmware: <https://lvfs.readthedocs.io/en/latest/privacy.html>,
  <https://github.com/hughsie/passim>
- Location: <https://beacondb.net/>, <https://beacondb.net/privacy/>,
  <https://ichnaea.readthedocs.io/en/latest/api/geolocate.html>
- KDE Connect: <https://userbase.kde.org/KDEConnect>,
  <https://invent.kde.org/network/kdeconnect-kde/-/raw/master/core/backends/lan/lanlinkprovider.cpp>
- The push-notice helper:
  <https://invent.kde.org/libraries/kunifiedpush/-/raw/master/src/distributor/distributor.cpp>
- Automatic time zone:
  <https://invent.kde.org/plasma/plasma-workspace/-/raw/master/geotimezoned/geotimezonemodule.cpp>
- Night Light and the location:
  <https://invent.kde.org/plasma/knighttime/-/raw/master/src/daemon/kdarklightmanager.cpp>
- Automatic proxy search:
  <https://invent.kde.org/network/kio-extras/-/raw/master/kcms/proxy/wpad-detector/module.cpp>
- KDE's usage feedback:
  <https://community.kde.org/Policies/Telemetry_Policy>
- Bazaar: <https://gitlab.gnome.org/World/bazaar/-/raw/main/README.md>
- labwc: <https://raw.githubusercontent.com/labwc/labwc/master/docs/labwc.1.scd>
- Read in the image itself, not on the web: the manual pages
  `cupsd.conf(5)` and `sm-notify(8)`, BlueZ's `/etc/bluetooth/main.conf`
  and labwc's sample `autostart` file.
