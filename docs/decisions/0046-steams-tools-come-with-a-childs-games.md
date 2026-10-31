# ADR-0046: Steam's tools come with a child's games

**Status:** proposed. The maintainer confirmed the copy in place of
each child's Steam fetching its own tools on 2026-10-30. It becomes
accepted when the checks under "Checks in the VM" have run in the
cairn-iso VM without overturning a bullet of the Decision.
**Date:** 2026-10-30
**Closes:** (none) — the third of issue #206's three fixes, and the first
part of #181; part of ROADMAP P1-5 and P1-13. Once accepted, amends
ADR-0033, ADR-0038, ADR-0041 and ADR-0042.

## Context

### A child's first tap

Each account's Steam keeps its own tools (ADR-0033): the Steam Linux
Runtimes that native games run in, and Proton for Windows games. Steam
downloads a runtime "if you install a game or a version of Proton that
requires it" (Valve's steam-runtime README). A child's games are reflink
copies made at a Save (ADR-0033, ADR-0041), so the child's Steam never
installs them, and first meets their tools at the child's first tap.

The tools on the cairn-iso VM, as the test child `ada`'s Steam fetched
them ("Runs in" is each tool's `require_tool_appid`):

| Appid | Tool | Runs in | Download | On disk |
|---|---|---|---|---|
| 1070560 | Steam Linux Runtime 1.0 ("scout") | 1391110 | 75 MB | 223 MB |
| 1391110 | Steam Linux Runtime 2.0 ("soldier") | — | 192 MB | 677 MB |
| 1493710 | Proton Experimental | 4183110 | 437 MB | 1,537 MB |
| 4183110 | Steam Linux Runtime 4.0 | — | 251 MB | 711 MB |

On 2026-10-30 `ada`'s first tap on Pajama Sam 2 (292780), a native game,
fetched scout and never soldier. She saw "Getting it ready" for 10
minutes, then "Something needs a grown-up", twice out of two (#206).
Arthur's Birthday (2471140), a Windows game, started 2 minutes 39
seconds after the tap, once her Steam had downloaded 688 MB of tools; on
slow Wi-Fi that takes longer than ADR-0036's 10-minute limit.

### What the maintainer chose

On 2026-10-30 Mason chose three fixes. Fix 1, the launcher starting the
child's Steam again once and tapping again, is a dated section of
ADR-0036 ("Steam stops part-way on a new child's first Linux game").
Fix 2, the grown-up screen saying "Log out and back in, then try it
again.", is ADR-0045. Both came with pull request #207. Fix 3 is "each
child's Steam gets its tools ahead of time", with "Should provide the
feedback that it will take time, and inform the parent of progress."
After the VM runs below he chose the copy instead ("Copying for sure"),
and said yes to three more choices: tools come only with a child's new
games, every tool goes to every child, and a tool his own Steam is still
updating is not taken.

### What the VM showed

On 2026-10-30, with `ada` logged out and her own tools set aside each
time and put back afterwards:

- **E9.** `steam://install/1391110`, `steam://run/1391110` and
  `steam://rungameid/1391110` each opened Steam's Install dialog (soldier,
  646 MB, Install and Cancel), which waits for a click, hidden behind the
  launcher or minimised. Nothing downloaded, and Steam fetched nothing by
  itself in 15 minutes. `steam://validate`, `steam://launch`,
  `-applaunch` with the tool's appid, Steam's console and a simulated
  click were not tried.
- **E1.** Reflink copies of the Guardian's four tools, each
  `common/<installdir>` without `var/`, manifest last, owned by `ada`,
  took about 3 s and 0 B (`btrfs filesystem du`). Her Steam loaded all
  four, downloaded and verified nothing, and never rewrote the copies,
  which keep the Guardian's SteamID as `LastOwner`. Pajama Sam 2 reached
  `CreatingProcess` 2 s after the tap, Arthur's Birthday 20 s: 18 s went
  on a one-time update of a Wine prefix her own Proton had made, which
  only replacing a child's own Proton causes, and Cairn never does.
- **E1b.** The same with `ada`'s depot cache for the tools moved aside:
  nothing fetched, games started 3 s and 5 s after the tap. So no
  depot-cache file needs copying, for builds that match Valve's current
  ones. Her Steam had installed all four tools that day, so its app cache,
  config and licences knew them; only a new child (check 1) shows this
  for a Steam that never had them.
- **E2.** A copied scout without soldier: at the tap `ada`'s Steam marked
  soldier "Update Required" by itself, downloaded it in 54 s and started
  the game at 59 s. No stall, in one run.
- Only Steam Linux Runtime 4.0 had a `var/`: 631 MB of pressure-vessel's
  temporary folders, some private.
- Not tested: a copy older than Valve's newest build. On the kids' laptop,
  tools copied by hand were updated at a tap in 40 s to 2 minutes, with
  "Getting it ready".

## Decision

**At every Save and every Look for new games, Cairn copies the tools in
the saving Guardian's own Steam, its Steam Linux Runtimes and Protons,
into a tools folder, by reflink and as that Guardian. When a Save copies
a child's new games, the child's own Steam library also gets a copy of
each tool from there that it lacks, as the child and by the copy step's
own rules; a tool the child's Steam has is never touched. It takes
seconds and downloads nothing. The Save's window says while the tools
are taken and copied and, afterwards, whether each child's Steam got
them or still has to download one.**

### This is not the mechanism first picked

Mason first picked each child's Steam fetching its own tools ahead of
time, with the parent told it takes a while and shown how far it has
got. A Steam nobody sees would have to be asked for each tool by its
appid: E9 tried three documented links that install or run an app that
way, and each opened a dialog that waits for a click nobody can give.
E1 and E1b found that a copy needs no asking: seconds, no disk, nothing
downloaded, and the games started. So the copy is the first-time path,
and the window says it usually takes a few seconds. The long downloads
that remain, Steam's update of itself and newer tool builds, keep the
warning and the progress for #181's later piece. Mason confirmed this
change on 2026-10-30.

### Which tools

- An app in one of the Guardian's libraries that kidscan's rule calls a
  tool (`is_steam_tool`: its appid, or a name starting "Proton", "Steam
  Linux Runtime" or "Steamworks"), whose folder, one plain folder name,
  holds a `toolmanifest.vdf` (`tool_installdir`, which the copy step asks
  too). Anything else is passed over without a word, so no game of the
  parent's is ever named.
- Every tool goes to every child: which tool Steam picks for a game sits
  in its binary app cache, which Valve does not document.
- Steamworks Common Redistributables (228980) has no `toolmanifest.vdf`,
  and Legacy Steam Runtime (4690330) is not in kidscan's rule.

### The tools folder

- `/var/lib/cairn/steam-tools`, made by tmpfiles: root and
  `cairn-guardian`, mode 2775, the shared folder's default ACL. Only
  Guardians change it; every account reads it. It is no Steam library,
  and nothing runs from it.
- One folder per tool build, `<appid>-<buildid>`: the tool's manifest
  byte for byte and its `common/<installdir>`, without `var/`, made
  inside a folder only that run uses, with a name no one can guess, and
  renamed into place whole. The tool's other builds then go.
- A build is never replaced by an older one, since a second Guardian's
  Steam may be behind, and it stays until a newer build replaces it, so
  the folder can hold a tool the saving Guardian's Steam no longer has.
  After Valve rolls a tool back, the folder keeps the higher build until
  Valve ships one higher still; a child's Steam that got it should then
  download Valve's current one at the first tap needing it (not tested).
- Any Guardian can make entries in the folder, so the step decides what a
  build is without following a link, removes anything else under a
  build's name, and writes only inside its own folder: a guard against
  mistakes and planted links, not against another Guardian, who is an
  administrator.
- Nothing is written in the parent's home, so their Steam can stay open.
  Under sudo there is no Guardian, and the folder stays as it is.

### Filling it, as the Guardian

- `provision/cairn-share-tools.py`, standard-library Python sharing
  kidscan's rules, is installed as `/usr/libexec/cairn/cairn-share-tools`,
  mode 644, beside `kidscan.py`, and runs as a system job: `systemd-run
  --uid=LOGIN --property=RuntimeMaxSec=300`, `python3 -I -B`.
- `systemd-run --uid` starts the step as the Guardian directly, with the
  account's own groups, as `setpriv --init-groups` gives the visit
  helper's copy, so no process of the job runs as root and no root-side
  wrapper is needed; `RuntimeMaxSec` then stops the whole unit, `cp`
  included.
- As the Guardian, not as root like the picture step, because it follows
  the Guardian's library list, whose paths can name any folder, and copies
  whole trees: a planted link reaches nothing the Guardian could not
  already, and the copies are owned as Steam's own installs are.
- It takes a tool only when its `StateFlags` is exactly 4, it is on no
  beta branch, `downloading/` has nothing for it, its manifest names a
  build, every account can read every file but those in its `var/`, `cp`
  can reflink from its library, and its manifest is unchanged after the
  copy.
- A Save runs it after every child's list has lost the games taken off,
  so a step that takes its whole five minutes leaves those gone, and
  before the first copy; after any move once ADR-0042 lands. Whether
  ADR-0042's move step drops to the Guardian the same way is for that
  step to decide. The family's setup compares this step with the
  checkout.

### A child's copy of the tools

- The copy step, as the child and after the games, copies the newest
  build of each tool the child's library lacks, through the same
  `decide()` and `copy_game()` as a game. A tool the child has, in any
  state, is never touched: the child's Steam owns it and updates it.
- It takes from the folder only what kidscan's rule calls a tool, which
  catches a game put there by mistake.
- It reports a tool that one the child now has runs in when the child's
  library still lacks it after the copy (the folder had none, or its copy
  was left out), read from the folder's `toolmanifest.vdf`, never the
  child's. For scout without soldier, E2 saw a wait, not a stall, in one
  run.
- Each tool stands alone and changes nothing said about a game. With no
  tools folder, as after provisioning, it says nothing about tools. The
  child's Steam loads the copies at the next login (ADR-0027).

### What the parent sees

While the tools are taken, for a Save and a Look: "Getting the tools
Steam runs games with from your own Steam. This usually takes a few
seconds." While a child's are copied: "Copying the tools Steam runs
games with into Ada's Steam. This usually takes a few seconds, and Ada
cannot log in until it ends."

Last in each child's result, one of three sentences: the tools were
copied, though one Cairn has no copy of, or a newer version, still
downloads and can make that game wait several minutes;
Ada's Steam may not have every tool yet; or Cairn had no tools to copy.
The last two add "If that game does not start, log Ada out and back in,
then try it again.", true with or without ADR-0036's restart. A child who
had every tool, or whose copy was held, gets none. A Save in which a copy
ran but a tool stayed out of the folder says so once. No sentence names
a tool or promises that a later Save mends it.

### What cairn-family says

New lines of `save-games`, fixed words, checked logins and appids, as
ADR-0041 requires:

| Line | Meaning |
|---|---|
| `tools made` or `tools kept` | The tools step went to its end, or it did not (refused, failed, never started or stopped at its time limit); every build in the folder is whole either way, though a step stopped part-way may already have changed some |
| `tool-not-shared APPID REASON` | A tool stayed out of the folder, which has no build of it |
| `copying-tools LOGIN` | The first of the child's tools is being copied |
| `tool-copied`, `tool-have`, `tool-missing LOGIN APPID` | The child has the tool now, or still lacks one their tools run in |
| `tool-skipped LOGIN APPID REASON` | A game's reason, `not-a-tool` or `no-reflink` |
| `no-tools LOGIN`, `tools-unreadable LOGIN` | The folder held no tool, or could not be read |

The step's own reasons are listed in `provision/cairn-share-tools.py`.
`journalctl -t cairn-family` gets `tools made` or `tools kept`, then the
step's lines but `tool-had` and `finished`, each after `tools step:`. A
step that did not finish and did not say why adds `no-answer` (no line
at all: it could not start, or its time limit came before its first
line) or `stopped` (some lines, then nothing more). `journalctl -t
cairn-steam-visit` gets each child's lines but `tool-have`.

### Where it applies

The image only, as ADR-0041. Provisioning makes no tools folder, so
`docs/TRY.md`, which describes provisioning, keeps step 7's visit, and
the kids' laptop keeps the tools it was given by hand on 2026-10-07.

### Later (#181)

An "Update Ada's Steam" button runs the child's Steam out of sight while
they are logged out, after a warning that it can take a while, with a
card showing how far it has got; night-time updates later, off unless a
parent turns them on. It needs VM experiments first (a hidden Steam in a
headless cage, what Steam fetches when left alone) and gets its own ADR.
Nothing of it is built here.

## Consequences

- No tool install and no #206 stall at a first tap, for every tool Cairn
  has a copy of, once check 1 holds.
- Limits: a copy is as new as the parent's Steam keeps it. Tools come
  only with new games, so a child with games from before gets them with
  their next new game, or with #181. A tool Cairn has no copy of, because
  no Guardian's Steam had it or could give it (an update waiting, a beta,
  private files, another drive), downloads at the first tap, as today. A
  child whose Steam never ran gets nothing. Fixes 1 and 2 stay the floor.
- Disk: 0 B until a Steam updates a copy; the folder's build then holds
  its own blocks, up to about 3 GB, until the next Save or Look. Each
  child's Steam keeps every copied tool up to date, Protons only the
  parent uses among them.
- Cairn copies tool manifests as it copies games' and writes none of
  Steam's files of its own. They carry the parent's SteamID, readable by
  every account, as the shared folder's game manifests do. If Steam stops
  taking a copied tool, the child's Steam is expected to fetch it at the
  first tap, as before ADR-0046, with #206's restart and grown-up line as
  the floor (not tested).
- A Save and a Look gain a job of a few seconds, at most five minutes.
- Once accepted it amends ADR-0033 (a child's first copy of each tool
  comes with their games, with no visit), ADR-0038 (the tools folder, the
  tools step in `/usr/libexec/cairn`, and the setup's check), ADR-0041
  (new lines and sentences) and ADR-0042 (the Save's order). DESIGN §8.3
  gains a bullet.
- No new dependency, and nothing phones home: the copies use no network.

## Checks in the VM

In cairn-iso, with an image built from this change:

1. **A new child:** a visit signs their Steam in, then a Save gives a
   native and a Windows game, with the tools lines and sentence. The
   folder holds four whole builds, no `var/`, the Guardian's, group
   `cairn-guardian`, readable by all. At the first taps `content_log.txt`
   shows no tool download or verify (an update of an older copy is
   allowed, and timed), and both games start.
2. **A second Save, and a Look:** `tools made`, no `tools step:
   tool-shared` line in `journalctl -t cairn-family` for that Save, and
   the folder's builds unchanged (or run
   `/usr/libexec/cairn/cairn-share-tools` by hand as the Guardian, with
   `python3 -I -B`, and see `tool-had` for each tool).
3. **`ada`, who has her own tools:** every tool `tool-have`, nothing of
   hers changed.
4. **A Save while the parent's Proton updates:** `tool-kept` in the
   journal or `tool-not-shared`, and the Save's sentence.
5. **Under sudo** the folder is unchanged; a wheel-only grown-up gets
   `tools kept`, with `tools step: refused folder` in the journal.
6. **A newer build from Valve** than a child's copy, and a copy Steam
   does not take: what the child's Steam does, and how long the tap
   waits.
7. Neither journal shows a path or a name.

### First run, 2026-10-30

In cairn-iso, on a build of this branch put over the image's `/usr`, with
the VM's clock in EDT:

- tmpfiles made the tools folder, mode 2775, `root:cairn-guardian`, with
  the shared folder's default ACL.
- A Save that only took two games off `ada`'s list said `tools made` at
  23:54:37. The step ran as the Guardian for 1.3 s; systemd gave it
  `cairn-guardian`, so it could write the folder. The folder held four
  builds, `1070560-26115262`, `1391110-26115965`, `1493710-26306234` and
  `4183110-26116036`: the Guardian's, group `cairn-guardian`, no `var/`,
  2.96 GiB with 0 B of their own.
- With `ada`'s own four tools and their five depot-cache files set aside,
  a Save gave her the two games back at 23:55:04: the games `have`, then
  `copying-tools ada` and four `tool-copied`, in 1.2 s for the whole Save.
  The copies were hers, without `var/`, with the SELinux types Steam gave
  her own install, manifests byte for byte the folder's, 0 B of their own.
- At her login her Steam loaded all four tools. Pajama Sam 2 started 1 s
  after the tap (23:59:34) and Arthur's Birthday 5 s after it (00:00:28),
  both in the copies, with no download, no verify and no depot-cache file
  fetched. An earlier tap at 23:56:36 never reached the launcher (the
  VM's pointer; no line in any log).
- Check 2 held: a second Save said `tools made` and shared nothing, and
  the step run by hand as the Guardian said `tool-had` four times.
- Check 3 held: a Save that gave her Pajama Sam 2 again, with every tool,
  said `tool-have` four times and left her manifests as they were.
- Check 5 held under `sudo`: `tools kept`, the folder unchanged. A
  wheel-only grown-up was not tried.
- Check 7 held: the journals name appids and the child's login only.
- Her Steam had installed these tools that day, so check 1 still needs a
  child whose Steam never had them; checks 4 and 6 are still to run.
  `ada`'s own tools were put back afterwards.

## Alternatives considered

- **Each child's Steam downloads its own tools** (#181, #206): none of
  the three links tried asks a hidden Steam without a click (E9); about
  1 GB and minutes per child with their login held; fresher builds.
- **The first fetch in a visit** (`docs/TRY.md` step 7): the parent stays
  at the screen, and it would probably stall on scout as in #206.
- **A hand-written "needs installing" manifest:** one Steam misreads
  breaks every game on that tool (steam-for-linux #13199, #13480).
- **steamcmd:** a new dependency; it refused Steam Linux Runtime 4.0 when
  anonymous.
- **One shared copy of the tools:** slow or failing for all but its owner
  (ADR-0033), and `var/` is each account's own (steam-runtime #845).
- **Tools at every Save for every child named:** it takes the login lock
  for a Save that only takes games off.
- **Fixes 1 and 2 alone:** no stall, but each child still waits for about
  1 GB at their first taps.
