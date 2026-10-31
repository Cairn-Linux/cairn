# ADR-0046: Steam's tools come with a child's games

**Status:** proposed. It becomes accepted when the maintainer has
confirmed the copy in place of each child's Steam fetching its own tools,
and the checks under "Checks in the VM" have run in the cairn-iso VM
without overturning a bullet of the Decision.
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
Arthur's Birthday (2471140), a Windows game, started after about two and
a half minutes for 688 MB of tools; on slow Wi-Fi that takes longer than
ADR-0036's 10-minute limit.

### What the maintainer chose

On 2026-10-30 Mason chose three fixes. Fixes 1 and 2, the launcher
starting the child's Steam again once and tapping again, and the
grown-up screen saying "Log out and back in, then try it again.", are
ADR-0045, proposed, on their own branch. Fix 3 is "each child's Steam
gets its tools ahead of time", with "Should provide the feedback that it
will take time, and inform the parent of progress." In #206 and #181 each
child's Steam downloads its own tools, each asked for by name.

### What the VM showed

On 2026-10-30, with `ada` logged out and her own tools set aside each
time and put back afterwards:

- **E9.** `steam://install/1391110`, `steam://run/1391110` and
  `steam://rungameid/1391110` each opened Steam's Install dialog (soldier,
  646 MB, Install and Cancel), which waits for a click, hidden behind the
  launcher or minimised. Nothing downloaded, and Steam fetched nothing by
  itself in 15 minutes. Steam's console and a simulated click, which no
  document offers for this, were not tried.
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

Mason first picked each child's Steam downloading its own tools, asked
for by name, with the parent told it takes a while and shown how far it
has got. E9 found no way to ask a hidden Steam: every documented link
opens a dialog that waits for a click nobody can give. E1 and E1b found
that a copy needs no asking: seconds, no disk, nothing downloaded, and
the games started. So the copy is the first-time path. With a copy
little waiting is left, and the window says the tools are being copied
and that this usually takes a few seconds. The long downloads that
remain, Steam's update of itself and newer tool builds, keep the warning
and the progress for #181's later piece. Mason confirms this change in
the pull request.

### Which tools

- An app in one of the Guardian's libraries that kidscan's rule calls a
  tool (`is_steam_tool`: its appid, or a name starting "Proton", "Steam
  Linux Runtime" or "Steamworks") and whose folder holds a
  `toolmanifest.vdf`. Anything else is passed over without a word, so no
  game of the parent's is ever named.
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
  byte for byte and its `common/<installdir>`, without `var/`. It is made
  inside a folder only that run uses, with a name no one can guess, and
  renamed into place whole. The tool's other builds then go.
- A build is never replaced by an older one, since a second Guardian's
  Steam may be behind. After Valve rolls a tool back, the folder keeps the
  higher build until Valve ships one higher still, and a child's Steam
  that got it downloads Valve's current one at the first tap needing it.
- Any account in `cairn-guardian` can make entries in the folder. The
  step decides what a build is without following a link, removes anything
  else under a build's name, and writes only inside its own folder. That
  guards against mistakes and planted links, not one Guardian, an
  administrator, against another.
- Nothing is written in the parent's home, so their Steam can stay open.
  Under sudo there is no Guardian, and the folder stays as it is.

### Filling it, as the Guardian

- `provision/cairn-share-tools.py`, standard-library Python sharing
  kidscan's rules, runs as a system job: `systemd-run --uid=LOGIN
  --property=RuntimeMaxSec=300`, `python3 -I -B`.
- `systemd-run --uid`, not the `setpriv` the visit helper uses for a
  child, because systemd sets the account's own groups, `cairn-guardian`
  among them, and its time limit stops the whole unit, `cp` included.
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
  before the first copy; after any move once ADR-0042 lands. The family's
  setup compares it with the checkout.

### A child's copy of the tools

- The copy step, as the child and after the games, copies the newest
  build of each tool the child's library lacks, through the same
  `decide()` and `copy_game()` as a game. A tool the child has, in any
  state, is never touched: the child's Steam owns it and updates it.
- It takes from the folder only what kidscan's rule calls a tool, which
  catches a game put there by mistake.
- It reports a tool that the child's tools run in when neither the child
  nor the folder has it, read from the folder's `toolmanifest.vdf`, never
  the child's; E2 says that is a wait, not a stall.
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
copied, though one the parent's Steam lacks, or a newer version, still
downloads and can make that game wait several minutes; Ada's Steam may
not have every tool yet; or there were no tools to copy. The last two add
"If that game does not start, log Ada out and back in, then try it
again.", true with or without ADR-0045's restart. A child who had every
tool, or whose copy was held, gets none. A Save in which a copy ran but a
tool stayed out of the folder says so once. No sentence names a tool or
promises that a later Save mends it.

### What cairn-family says

New lines of `save-games`, fixed words, checked logins and appids, as
ADR-0041 requires:

| Line | Meaning |
|---|---|
| `tools made` or `tools kept` | The tools step finished, or the folder stays as it was |
| `tool-not-shared APPID REASON` | A tool stayed out of the folder, which has no build of it |
| `copying-tools LOGIN` | The first of the child's tools is being copied |
| `tool-copied`, `tool-have`, `tool-missing LOGIN APPID` | The child has the tool now, or lacks one their tools run in |
| `tool-skipped LOGIN APPID REASON` | A game's reason, `not-a-tool` or `no-reflink` |
| `no-tools LOGIN`, `tools-unreadable LOGIN` | The folder held no tool, or could not be read |

The step's own reasons are listed in `provision/cairn-share-tools.py`.
`journalctl -t cairn-family` gets its lines but `tool-had`, and
`journalctl -t cairn-steam-visit` each child's but `tool-have`.

### Where it applies

The image only, as ADR-0041. Provisioning makes no tools folder, and
`docs/TRY.md`, which describes provisioning, keeps step 7's visit. The
kids' laptop keeps the tools it was given by hand on 2026-10-07.

### Later (#181)

An "Update Ada's Steam" button runs the child's Steam out of sight while
they are logged out, after a warning that it can take a while, with a
card showing how far it has got; night-time updates later, off unless a
parent turns them on. It needs VM experiments first (a hidden Steam in a
headless cage, what Steam fetches when left alone) and gets its own ADR.
Nothing of it is built here.

## Consequences

- No tool install and no #206 stall at a first tap, for every tool the
  parent's Steam has, once check 1 holds.
- Limits: a copy is as new as the parent's Steam keeps it. Tools come
  only with new games, so a child with games from before gets them with
  their next new game, or with #181. A tool the parent's Steam lacks or
  could not give (an update waiting, a beta, private files, another
  drive) downloads at the first tap, as today. A child whose Steam never
  ran gets nothing. Fixes 1 and 2 stay the floor.
- Disk: 0 B until a Steam updates a copy; the folder's build then holds
  its own blocks, up to about 3 GB, until the next Save or Look. Each
  child's Steam keeps every copied tool up to date, Protons only the
  parent uses among them.
- Cairn copies tool manifests as it copies games' and writes none of
  Steam's files of its own. They carry the parent's SteamID, readable by
  every account, as the shared folder's game manifests do. If Steam stops
  taking a copied tool, the child's Steam fetches it at the first tap.
- A Save and a Look gain a job of a few seconds, at most five minutes.
- Once accepted it amends ADR-0033 (a child's first copy of each tool
  comes with their games, with no visit), ADR-0038 (the folder, and the
  setup's check), ADR-0041 (new lines and sentences) and ADR-0042 (the
  Save's order; its move step should drop to the Guardian the same way).
  DESIGN §8.3 gains a bullet.
- No new dependency, and nothing phones home: the copies use no network.

## Checks in the VM

In cairn-iso, with an image built from this change:

1. **A new child:** a visit signs their Steam in, then a Save gives a
   native and a Windows game, with the tools lines and sentence. The
   folder holds four whole builds, no `var/`, the Guardian's, group
   `cairn-guardian`, readable by all. At the first taps `content_log.txt`
   shows no tool download or verify (an update of an older copy is
   allowed, and timed), and both games start.
2. **A second Save, and a Look:** every tool `tool-had`, with the journal
   lines.
3. **`ada`, who has her own tools:** every tool `tool-have`, nothing of
   hers changed.
4. **A Save while the parent's Proton updates:** `tool-kept` or
   `tool-not-shared`, and the Save's sentence.
5. **Under sudo** the folder is unchanged; a wheel-only grown-up gets
   `refused folder` and `tools kept`.
6. **A newer build from Valve** than a child's copy: what the child's
   Steam does, and how long the tap waits.
7. Neither journal shows a path or a name.

## Alternatives considered

- **Each child's Steam downloads its own tools** (#181, #206): no
  documented way asks a hidden Steam (E9); about 1 GB and minutes per
  child with their login held; fresher builds.
- **The first fetch in a visit** (`docs/TRY.md` step 7): the parent stays
  at the screen, and it would probably stall on scout as in #206.
- **A hand-written "needs installing" manifest:** one Steam misreads
  breaks every game on that tool (steam-for-linux #13199, #13480).
- **steamcmd:** a new dependency, refused Steam Linux Runtime 4.0 when
  anonymous.
- **One shared copy of the tools:** slow or failing for all but its owner
  (ADR-0033), and `var/` is each account's own (steam-runtime #845).
- **Root copying from the parent's home, or the tools step as root:**
  root would follow a home's links.
- **Tools at every Save for every child named:** it takes the login lock
  for a Save that only takes games off.
- **Only the tools a child's games need:** Steam's app cache is
  undocumented.
- **Taking a tool whose update is waiting:** revisit with #181.
- **Fixes 1 and 2 alone:** no stall, but each child still waits for about
  1 GB at their first taps.
