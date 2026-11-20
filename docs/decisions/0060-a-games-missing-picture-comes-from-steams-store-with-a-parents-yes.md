# ADR-0060: A game's missing picture comes from Steam's store with a parent's yes

**Status:** proposed. The maintainer decides whether it is accepted,
once the checks under "Checks in the VM" have run in the cairn-min VM,
on an image built from slices 2 to 5, with none of them overturning a
bullet of the Decision, and the checks under "Checks on the kids'
laptop" have run there. The laptop has run the image, with Family,
since 2026-11-04.
**Date:** 2026-11-20
**Closes:** (none) — the maintainer's request of 2026-11-20, recorded in
ADR-0047 under "Later"; part of ROADMAP P1-5 and P1-13. Once accepted,
amends ADR-0035 (a picture is "never downloaded", and fetching from
Steam's servers was turned down), ADR-0047 (pictures come only from the
Guardian's Steam, and the Save window's sentences) and ADR-0007 (what
needs a network). Adds two subcommands and an option to ADR-0039's
`cairn-family`, and a question, a button and a line to ADR-0041's Games
page.

## Context

A child who cannot read finds a game by its picture (#138, ADR-0035).
A tile with only the game's name is the fallback, not the aim
(ADR-0047).

Today each picture is copied on the computer from the Guardian's own
Steam, from `~/.local/share/Steam/appcache/librarycache/`, and never
downloaded (ADR-0035, Decision).
Steam keeps a picture there only for a game its library has shown.
So a game installed from another grown-up's Steam, or not yet looked at,
has none.
Family then names the game and says: open Steam in your own login, then
press Look for new games (`guardian/qml/SaveOutcome.qml:84-85`,
`guardian/qml/GameCard.qml:48`).
That works, but it is a chore, and it ends at a tile with a name if
Steam changes its undocumented cache (ROADMAP risk row on Steam's
files).

On 2026-11-20 the maintainer asked for more: "Maybe we have a
notification when setting up games for a child the first time asking if
they would like us to download pictures for the game tiles? That way we
do not reach out to the internet without permission? Steams store
pictures sounds best. We could also have an "update game icons" button
on the games page in the parent's app. That would allow them to fetch
them at any time."

This ADR reads "for a child the first time" as once for the computer,
at the first Save of any child's games.
Pictures go by game, not by child: one picture serves every child who
has the game (ADR-0047), so one answer covers every child.

The rules this has to keep:

- CLAUDE.md non-negotiable 1: no network calls the user didn't ask for.
- DESIGN §2 principle 6 and §9.3: nothing phones home, and the system
  collects nothing and needs no account.
- `docs/research/network-activity.md` (#309) lists what the image
  contacts without being asked. No Cairn program on the image connects
  to the internet itself today. Family's About page names two addresses
  (`guardian/src/AboutPage.cpp:69`, `:74`, from #308), which it hands to
  the grown-up's own web browser only when a button is pressed. The
  inventory's sentence that no Cairn source names an address was written
  before that page; this change corrects it. #310 and #313 have since
  turned off reports the image sent by itself.

### What Steam's store gives

Checked from the development computer on 2026-11-20 with `curl`, mostly
with HEAD requests, for the family's 11 games and a few others:

- Each of the 13 store headers downloaded is a 460x215 baseline JPEG of
  26 to 118 KB, the same picture Steam keeps on the computer. For five
  games the copy in the development computer's own Steam cache was byte
  for byte the store's header.
- The fixed address, `.../steam/apps/<appid>/header.jpg`, answered 200
  for the 6 older games and 404 for the 5 newest, on three of Valve's
  hosts. `shared.steamstatic.com` answered the same for one of each.
  The newest keep the header one folder down, named by a 40-digit hash.
- Only Valve's store API names that folder. `IStoreBrowseService/GetItems`
  on `api.steampowered.com` takes every app id in one request, needs no
  key and no account, and set no cookie. It answered with no User-Agent.
  Without a country it answers `{"response":{}}`, so a country must be
  sent.
- The other API, `store.steampowered.com/api/appdetails`, takes one game
  per request and sets a `browserid` cookie that lasts a year.
- Both hosts also answer plain `http` with 200 and no redirect.
- Rate limits are not documented. None was met.

### Who carries the requests

Seen from the development computer on 2026-11-20 (`getent`,
`resolvectl query`):

- `api.steampowered.com` resolved to 23.43.51.164 and 23.43.51.165,
  whose names are `a23-43-51-16x.deploy.static.akamaitechnologies.com`.
  So the request that names the app ids and the country very likely
  passes through Akamai as well as Valve. Whether the encrypted
  connection ends at Akamai is not confirmed.
- `shared.steamstatic.com` resolved through `shared.valve.map.fastly.net`,
  so the pictures come through Fastly.
- Other places, or other days, may get other companies (not confirmed).

### Valve's terms

- The Steam Subscriber Agreement
  (https://store.steampowered.com/subscriber_agreement/, §2.A) gives a
  subscriber a licence for personal, non-commercial use. Nothing found
  forbids showing a game's own store header on the owner's computer,
  fetched when the owner asks. The art belongs to each publisher.
- The Steam Web API Terms of Use (https://steamcommunity.com/dev/apiterms,
  last updated July 2010) cover `api.steampowered.com`. They are written
  for an application that signs up for an API key. They require a
  privacy policy, allow 100,000 calls a day, ask that Steam data about a
  user be fetched only when that user asks, and forbid presenting the
  data so that the application seems endorsed by Valve.
- GetItems answers without a key, so Cairn signs up for none. Whether
  those terms govern a call with no key is not clear. Cairn's use would
  sit far below their daily cap, fetch only on the owner's press, and
  suggest no endorsement. Cairn has no privacy policy beyond the
  question's words and the network inventory.
- None of this is legal advice.

### What a request tells Valve, and the companies that carry it

- The computer's internet address and the time.
- The app ids asked for, so which of the family's games need a picture.
- The country Cairn sends, which the API needs.
- No account, no Steam ID, no key and no cookie.

The Guardian's own Steam already signs in from the same address, with
the family's account, and owns these games. The new requests add that
something on that address asked for these pictures outside the Steam
client.

### What Cairn has today

- Nothing is stored for the whole computer: no `/etc/cairn`, no
  `QSettings` in Family, no settings file.
- Every `cairn-family` run through pkexec asks for the password again:
  its actions are `auth_admin`, not `auth_admin_keep`
  (`session/polkit/actions/com.cairnlinux.family.policy`, such as
  `save-games` at `:75-85`).
- A Save and a Look run the picture step as root, then make the games
  list (`guardian/src/CatalogueRefresh.cpp:94-111`), then the tools
  step, then each child's copies (`guardian/src/GamesSave.cpp:75-124`).
  The tools step already runs as a system job with a time limit, as the
  Guardian (`guardian/src/ToolsRefresh.cpp:12-31`, ADR-0046).
- No unit in the repository limits a program's network or files with
  systemd's sandbox settings.

## Decision

**Family asks once, at the first Save of a child's games on this
computer, whether Cairn may get missing game pictures from Steam's
store, and says what is sent and to whom. The answer is the computer's
and can be changed on the Games page. With a yes, a Save or a Look also
gets a picture from the store for each shared game that has none, after
every child has their list. A new button on the Games page, Get
pictures from Steam's store, gets them when pressed; unless the answer
is yes, it first shows what is sent. These presses are the only times
Cairn reaches the internet for pictures. The Guardian's own Steam's
picture always comes first, and a store picture only fills a gap. The
download runs as a throwaway account that systemd makes for the run, in
a sandbox that sees no home and no other program. Root takes each
downloaded file only through the picture step's checks and a check of
the picture's size.**

### When Family asks

- The question comes when a parent presses Save on the Games page and
  the computer has no answer yet. It comes before the password box, so
  one password covers the answer and the Save.
- It covers the Family window, as the Save's progress window does, with
  the heading "Pictures for the game tiles" and these words:

  > A child who cannot read yet finds a game by its picture. Cairn
  > copies each picture from your own Steam, but Steam keeps a picture
  > only for games you have looked at in its Library. So some games may
  > have no picture.
  >
  > Cairn can get most missing pictures from Steam's store over the
  > internet. Valve, which runs Steam, and the companies that deliver
  > Steam's store over the internet then see four things: this
  > computer's internet address, the time it asked, which of your games
  > need a picture, and the country in this computer's language setting,
  > such as United Kingdom for English (UK). Nothing else is sent: no
  > account, no one's name, nothing about the children. Your own Steam
  > already signs in to Valve from this address and tells it which games
  > you own.
  >
  > With yes, Cairn asks Steam's store when you save games or look for
  > new games, and only for games that have no picture. With no, Cairn
  > never asks the store by itself. Either way, you can press Get
  > pictures from Steam's store on the Games page at any time, and
  > change this answer there later.

- Its buttons are **Yes, use Steam's store** and **No, thanks**.
- Focus starts on the first paragraph, so a screen reader reads the
  words first. Neither button is the default, and Enter does nothing
  until Tab has put focus on a button.
- Either button goes on to the password box and the Save.
  The answer is stored only once the password is accepted.
- Escape, or a closed password box, stores nothing and saves nothing.
  Focus goes back to Save, the changes stay on the page with the
  "Not saved yet" sentence (`guardian/qml/GamesView.qml:147`), and the
  next Save asks again.
- Look for new games never brings the question. On a computer whose
  children already have lists, as after an update, the question comes
  at the next Save.

### The answer is the computer's

- It is `/var/lib/cairn/settings/store-pictures`, holding one word,
  `yes` or `no`, owned by root, mode 0644.
  No file means not asked, which fetches nothing by itself.
- Only `cairn-family` writes it, with `RootFolder::write`
  (`guardian/src/RootFolder.h:27-28`), which makes the folder, mode
  0755, when it is missing (`guardian/src/RootFolder.cpp:39-49`,
  `:55-69`). No tmpfiles line is needed.
- The answer is trusted only from a root-owned plain file, not a link,
  not writable by group or others, of at most 16 bytes, in a folder
  `RootFolder::isSafe` accepts (`guardian/src/RootFolder.cpp:30-37`),
  as the picture step trusts the children's lists only from a safe
  folder (`provision/cairn-game-pictures.py:381-383`). Anything else
  reads as not asked. A child cannot plant the file today, because
  `/var/lib/cairn` is root's, mode 0755; this is a second layer.
- Every account can read it. Nothing in a child's session reads it.
- A household can have several Guardians (DESIGN §3.2). The answer is
  the family's, so each grown-up sees the same, and one grown-up's yes
  makes the others' Saves ask the store too. The line under the buttons
  and the progress window tell each of them so.
- One line under the Games page's buttons says what it is, with one
  quiet button:
  - on: "Pictures from Steam's store: on. Saving games and looking for
    new games also get missing pictures from there." **Turn off**
  - off, or not asked: "Pictures from Steam's store: off. Cairn asks
    the store only when you press Get pictures from Steam's store."
    **Turn on**
- Turn on shows the question's first two paragraphs and "Turning this
  on also gets the missing pictures now.", with **Turn on** and
  **Cancel**. Focus starts on Cancel. Turn on then asks for the
  password, stores `yes` and gets the missing pictures in the same run,
  with the progress window. Cancel changes nothing and sends nothing.
- Turn off asks for the password at once, since it sends nothing.
- Provisioning and the image's family set-up never fetch, and never
  read the file. The set-up runs only `create-child`
  (`provision/cairn-family-setup.sh:136`).

### Get pictures from Steam's store

- A quiet button after Look for new games.
- When the answer is not yes, a press first shows a box with the
  heading "Get pictures from Steam's store" and these words: "This
  computer asks Steam's store over the internet for the pictures of
  games that have none. Valve, which runs Steam, and the companies that
  deliver Steam's store over the internet see this computer's internet
  address, the time, which of your games need a picture, and the
  country in this computer's language setting. Nothing about the
  children is sent. This asks the store once, now. It does not turn on
  asking when you save games." Its buttons are **Get pictures** and
  **Cancel**. Focus starts on the words. Cancel sends nothing and asks
  for no password.
- With the answer yes, a press goes straight to the password box: the
  parent has read the words and said yes.
- It runs its own subcommand, so its password box says what it does:
  "A grown-up's password is needed to get missing game pictures from
  Steam's store."
- It runs the picture step and the games list, as a Look does, then the
  store. It runs no tools step and copies nothing for any child.
- The stored answer stays as it is. Pressing is asking, once.

### Which games, and in what order

1. The picture step copies from the Guardian's own Steam, as today.
2. The games list is made, as today.
3. A Save or a Look then does its tools step and each child's copies,
   as today. Each child has their new list before the store is asked.
   Get pictures from Steam's store skips this step.
4. The store is asked for each game on the new list that has no picture
   in the pictures folder after step 1. That is each game Family would
   name as `picture-missing` or `picture-refused`
   (`guardian/src/CatalogueRefresh.cpp:70-82`), so a game whose own
   Steam picture was refused is asked for too.
   - No such game: nothing is sent at all.
   - The picture step did not finish: nothing is sent.
5. Root copies what the store gave, by the picture step's checks, only
   into a gap.

So a Save with a yes sends nothing once every shared game has a
picture.
A picture in place is never replaced by the store.
The Guardian's own Steam's picture replaces a store picture at the next
Save or Look once Steam has one, by ADR-0035's rule. Often the two are
the same file; when they are not, the picture a child looks for
changes once. Child test 2 watches for it.
ADR-0047's rule keeps and removes store pictures like any other, in the
one folder.

### The store step

- `provision/cairn-store-pictures.py`, standard-library Python, as the
  picture, copy and tools steps are (ADR-0033, ADR-0035, ADR-0046). It
  is installed as `libexec/cairn/cairn-store-pictures`, mode 644, and
  run with `python3 -I -B`.
- `cairn-family` runs it as a system job, as it runs the tools step,
  with `systemd-run`, unit `cairn-store-pictures`, but with
  `DynamicUser=yes` in place of `--uid`. systemd makes a throwaway
  account for the run and drops it after. No process of the job runs as
  root or as any person's account, so it cannot reach the Guardian's
  running programs (see "Run as the Guardian" under Alternatives).
- The job's sandbox:
  - `ProtectHome=yes` and `InaccessiblePaths=-/var/home`, so it reads
    no home, and with it no Steam sign-in or wallet;
  - `PrivatePIDs=yes`, `ProtectProc=invisible` and `ProcSubset=pid`, so
    it sees no other process (`PrivatePIDs=` needs systemd 257; Fedora
    44 has 259; the image's version is checked in the VM);
  - `ProtectSystem=strict`, so its download folder is the one place it
    can write;
  - `RestrictAddressFamilies=AF_INET AF_INET6`, with no `AF_UNIX`, so
    it reaches no D-Bus, display or other local socket. Names still
    resolve through systemd-resolved's stub at 127.0.0.53, which
    `/etc/resolv.conf` names; the VM checks it;
  - `SystemCallFilter=@system-service`, then
    `SystemCallFilter=~@debug process_vm_readv process_vm_writev`;
  - `MemoryDenyWriteExecute=yes`, `PrivateTmp=yes`, `PrivateDevices=yes`,
    `NoNewPrivileges=yes`, an empty `CapabilityBoundingSet=`;
  - the rest of systemd's usual hardening, listed in the slice that
    builds it.
- systemd makes the download folder for the job
  (`StateDirectory=cairn/store-pictures`, `StateDirectoryMode=0700`).
  With `DynamicUser=yes` it is really
  `/var/lib/private/cairn/store-pictures`, behind a folder only root can
  open, and `/var/lib/cairn/store-pictures` is a link to it. Cairn's
  code changes no folder's owner.
- The step empties the folder before it fetches.
- Time: `RuntimeMaxSec=120` for the job, and `TimeoutStopSec=5`,
  because the step is the first process of its own process space and
  so does not stop on systemd's polite signal. The step itself starts
  no new request after 100 seconds and says `stopped`.
- After every run, offline, stopped or failed too, root copies the
  pictures the step said it fetched, then empties the folder. So
  nothing downloaded stays, and nothing fetched before a stop is lost.

### What leaves the computer

- One GET to `https://api.steampowered.com/IStoreBrowseService/GetItems/v1/`
  for up to 50 app ids at a time, with `input_json` naming the ids, the
  language `english`, a country, and both kinds of picture paths.
- The country is the two capital letters after `_` in `LANG` in
  `/etc/locale.conf`, such as `GB` in `en_GB.UTF-8`, or `US` when it
  names none.
- Then one GET per game to `https://shared.steamstatic.com`, at
  `/store_item_assets/steam/apps/<appid>/` and the header the answer
  names. The address is built from those fixed pieces. The host and
  every other piece come from the step, never from the answer.
- The header must be `header.jpg` or `<40 hex digits>/header.jpg`, and
  the answer's path format exactly `steam/apps/<that appid>/${FILENAME}`
  with an optional `?t=<digits>`, each checked as a whole string. The
  publisher's art without its temporary sale or event picture comes
  first.
- A game the answer leaves out, or gives no header, gets the fixed
  address instead. Its art may be older (for app 620 the fixed address
  served a 2012 picture where the store showed one from 2026). Whether
  the fixed address can ever carry sale or event art is not confirmed.
- One connection to each host, used again for each request. A
  connection the server closed between requests is opened again once.
- https only, with the system's certificates and Python's default
  checks, and no pinning. No redirect is followed. No cookie is kept
  or sent. No User-Agent is sent. No proxy is used.
- A time limit of 10 seconds on each connection and each read. A name
  lookup has no limit of its own beyond the resolver's and the job's.
- Who sees it: Valve, and the companies that deliver Steam's store over
  the internet (on 2026-11-20, from one computer: Akamai for the API,
  Fastly for the pictures).

### Every answer is untrusted

The store step keeps a picture only when:

- the answer is 200, and `Content-Type` is `image/jpeg`;
- it is at most 1 MiB, read to at most one byte more;
- it starts with `FF D8 FF`.

It writes `<appid>.jpg` under a temporary name, then renames it, then
says `fetched APPID`.
The API's answer is read to at most 1 MiB and parsed with Python's
`json`.
Any request that cannot connect or read, with a refused certificate (as
on a café's sign-in page) or a time limit, means `offline`, and nothing
more is asked.
An answer `http.client` cannot read (`http.client.HTTPException`, which
is not an `OSError`) is `bad-answer` for that request only.
A 429 or 403 means `turned-away`, and nothing more is asked.
Nothing the server says reaches the window or the journal: only fixed
words and app ids, as the picture step does
(`provision/cairn-game-pictures.py:54-56`).

### Root takes the pictures by the picture step's rules

- `cairn-game-pictures --from-store FOLDER APPID...` is given only the
  app ids that the store was asked for and that the step said
  `fetched`.
- It copies `<appid>.jpg` from the folder only for a game that is still
  shared and has no picture in place. Any other is refused, by name.
- It reads each with the function that reads the Guardian's Steam
  today: opened without following a link or waiting on a FIFO, a plain
  file, at most 1 MiB, starting `FF D8 FF`
  (`read_named_picture`, `provision/cairn-game-pictures.py:261-284`).
- It then reads the JPEG's frame header, without decoding, and refuses
  a picture that is not 1 to 1024 pixels wide and 1 to 512 high. That
  check lives here, at the trust boundary, and nowhere else.
- It puts it in place as today: temporary name, `fsync`, rename, mode
  0644 (`put_in_place`, `:320`), after clearing its own leftover
  temporary files as the normal run does (`:466`).
- At the end it empties the folder with Python's `shutil.rmtree`,
  which follows no link on Linux (`shutil.rmtree.avoids_symlink_attacks`),
  and leaves the empty folder for the next run.
- Root never fetches and never decodes. Qt decodes a picture in the
  child's launcher and in Family, at the size drawn
  (`launcher/qml/Tile.qml:77-78`), as ADR-0035 already accepted for
  Steam's own cache. A picture Qt cannot draw shows the game's name.

### What cairn-family does

| Run | What it does |
|---|---|
| `save-games --store-pictures=yes\|no [LOGIN=APPID,...]...` | Writes the answer after the checks and the lock, before anything else, then saves, and asks the store at the end when the answer is yes. Only the question sends this option. |
| `save-games [LOGIN=APPID,...]...` | As today. With a stored yes it also asks the store at the end. |
| `get-pictures [--store-pictures=yes]` | The picture step, the games list, then the store. No tools step, no copies. With the option (Turn on), it first writes `yes`. New action `com.cairnlinux.family.get-pictures`, `auth_admin`. |
| `set-store-pictures yes\|no` | Writes the answer, and nothing else. Family's window sends only `no` (Turn off). New action `com.cairnlinux.family.set-store-pictures`, `auth_admin`: "A grown-up's password is needed to turn pictures from Steam's store on or off." |

The `save-games` action's message gains one sentence, there whatever the
answer: "When pictures from Steam's store are on, this also gets
missing game pictures from there."

New lines to Family's window, with fixed words and checked app ids as
ADR-0041 requires. They come after the last child's lines, or after the
games list in a Get pictures run:

| Line | Meaning |
|---|---|
| `store-asking` | Said just before the store step starts |
| `store-not-needed` | Every game on the list has a picture, so the store was not asked |
| `store-got APPID` | The store's picture is now in the folder |
| `store-none APPID` | The store gave no picture for it |
| `store-refused APPID` | The store step got one, and root's checks left it out |
| `store-offline` | The store could not be reached; nothing more was asked |
| `store-turned-away` | The store said not now; nothing more was asked |
| `store-stopped` | The step did not finish in its time |

`journalctl -t cairn-family` gets the store step's own words after
`store step: `, and root's copy after `store copy: `.
The step's error output is dropped, so no path, title or server answer
reaches the journal.

### What the parent sees

- At `store-asking`, the progress window says "Your changes are saved.
  Now getting missing pictures from Steam's store." after a Save, and
  "Getting missing pictures from Steam's store." after a Look or a Get
  pictures press.
- After a run that asked:
  - "New pictures from Steam's store: Spy Fox and Freddi Fish. Each
    child sees them the next time they log in."
  - "Steam's store could not be reached. Check that the computer is
    online, then press Get pictures from Steam's store."
  - "Steam's store did not give the pictures just now. Try again
    later."
  - "Steam's store was slow, so Cairn stopped asking. Press Get
    pictures from Steam's store to ask for the rest."
  - A game still without a picture keeps ADR-0047's "No picture yet"
    sentence, which gains "Steam's store has none for it either." when
    the store was asked for it.
  - "Cairn could not use the picture Steam's store gave for Spy Fox.",
    with the paragraph ending on ADR-0047's pointer to the journal.
  - A game the store gave is taken off the "No picture yet" sentence.
- Get pictures from Steam's store with nothing to ask: "Every shared
  game has a picture, so Steam's store was not asked."
- The card's note and the "No picture yet" sentence point only at the
  family's own Steam, whatever the answer. After a "No, thanks", only
  the line under the buttons and the button name the store.
- The exact strings are settled, with their tests, in the slice that
  builds them.

### What a child sees

- A picture on a tile, and nothing else new.
- No child can start a fetch or change the answer. polkit's levels rule
  refuses every `com.cairnlinux.` action to an account that is not an
  administrator, before any password
  (`session/polkit/rules.d/10-cairn-levels.rules:11-15`, `:132-140`).
- The launcher still reads only files on this computer, never an
  address (`launcher/src/TilePicture.h:10-14`).

### Steam's name and the art

- Family says "Steam's store" and never shows Steam's logo. Nothing
  says or suggests that Valve made or endorses Cairn.
- The pictures stay on this computer. Cairn never uploads or shares
  them, ships none in the image and commits none to this repository.
  Tests use stand-in pictures made at run time.

### Two questions still open

The maintainer has two questions still to answer. The Decision above
follows the recommended answers, which stay proposals until he answers.

1. **After a yes, do Save and Look also get pictures for new games?**
   Proposed: yes, so a new game gets its picture with no extra step.
   The cost: those Saves go online, and their password box says so in
   only one added sentence, though the line under the buttons and the
   progress window say it too. With no, only the button fetches (see
   "Only the button, with no remembered yes" under Alternatives), and
   each new game needs a press and a second password.
2. **Is the button called "Get pictures from Steam's store"?** The
   maintainer suggested "Update game icons". Proposed: "Get pictures
   from Steam's store", so its name says it goes online. A parent who
   said no could read "Update game icons" as something that stays on
   the computer. If another name is chosen, every string that names the
   button changes with it, before slice 5.

## Slices

Each slice leaves `main` working, with its tests, and carries its own
bookkeeping.

1. This ADR, proposed, with the DESIGN, ROADMAP and inventory edits.
   Docs only.
2. Root's side: the picture step learns `--from-store`. No network.
3. The store step, alone, with tests that hand it a stand-in
   connection. Nothing runs it yet.
4. `cairn-family` keeps the answer and runs the step. The first slice
   that can reach the internet, and only from `cairn-family`.
5. Family's window: the question, the button and the line.
6. The checks on the kids' laptop; then the maintainer decides.

Slices 2 and 3 do not depend on each other. Nothing reaches the
internet before slice 4, and nothing a family sees changes before
slice 5. No test reaches the internet, and no game art is committed.

## Consequences

- A parent gets most missing pictures with one yes, without opening
  Steam. A game only another grown-up's Steam has shown gets one too.
  If Steam's cache changes, the store still fills the gap.
- Cairn now has code that reaches the internet. It is one step, run
  only on a Guardian's press, behind a password, as a throwaway account,
  in a sandbox. It is the only Cairn program on the image that itself
  connects to the internet. The network inventory has row 5.24 for it.
- What stays true:
  - Family's window opens no connection itself. A press in it can now
    make `cairn-family` start the store step, which does.
  - The launcher fetches nothing.
  - The picture step downloads nothing.
  - Cairn collects nothing and needs no account.
- What is given up: ADR-0035's "never downloaded".
- A Save with a yes reaches the internet. Its password box says so in
  one added sentence, the Games page's line says it is on, and the
  progress window says so at the time.
- A Save waits for the store only after every child has their list.
  A slow link can hold the progress window for up to about two minutes
  after "Your changes are saved."
- Get pictures from Steam's store needs its own password, as every
  Family change does.
- Limits:
  - A game blocked in the computer's country may get no picture from
    the API, and falls back to the fixed address.
  - A delisted game that has only a hashed header gets none (not seen;
    none was found to try).
  - GetItems is not documented by Valve and may change. Then games fall
    back to the fixed address, and the newest get none.
  - A family behind a proxy gets `offline`.
  - A family filter that blocks one of the two hosts gets `offline` at
    the first request to it, and the pictures fetched before it stay.
  - With no User-Agent and Python's own TLS, a request may look unlike
    Steam's client. It does not name Cairn.
  - Rate limits are unknown. One press sends one API request for up to
    50 games, and one request per picture.
  - A store picture can change once the Guardian's own Steam has its
    own picture for the game.
- Nothing of it is built yet. Slices 2 to 5 bring their own tests, in
  Python and C++.
- ADR-0059, a separate proposal, would start some games in ScummVM
  rather than Steam. Pictures still go by Steam app id. Today the app
  id is read only from `steam -applaunch APPID`, by the launcher
  (`launcher/src/GamesFile.cpp:20-34`), Family
  (`guardian/src/GameCatalogue.cpp:32-43`) and the picture step
  (`tile_appid`, `provision/cairn-game-pictures.py:426`). A ScummVM
  entry must keep its Steam app id another way, or its tile loses its
  picture, children's lists stop keeping it, and Family stops naming
  it. That is ADR-0059's to decide.

## Alternatives considered

- **SteamGridDB or IGDB.** Each needs an account and a key: SteamGridDB
  a key from a signed-in account, IGDB a Twitch developer account. That
  would be a project account or the family's, which DESIGN §9.3
  excludes. SteamGridDB's art is also made by its users, not the
  publisher.
- **Download without asking**, at login, at boot, on a timer, or from
  the first Save. That is a network call nobody asked for, which
  non-negotiable 1 forbids.
- **Ship the pictures in the image.** The art is each publisher's, and
  ADR-0035 ships none. No set could cover every family's games, and it
  would grow the image.
- **Only the button, with no remembered yes.** The question's yes would
  fetch once, and later only the button would. It is simpler: no
  on/off line and no `set-store-pictures`. A new game would then need
  a press and a second password. This is open question 1.
- **Ask at every Save.** It nags, and the maintainer asked for once.
- **The button's box stores the answer.** A press with no answer would
  store yes and fetch. Turned down: a one-off press would then turn on
  asking at every later Save, which the parent did not ask for. The
  box asks once, and the first Save still asks the standing question.
- **Ask the store before the children's lists are written.** It would
  hold every child's new list behind the internet, up to two minutes on
  a slow link. Since ADR-0047 a tile finds its picture by app id and the
  games file names none, so the store can come last.
- **appdetails.** One game per request, and a cookie that lasts a year.
- **The fixed address only.** No picture for 5 of the family's 11
  games.
- **Fetch in Family, with Qt Network.** Family runs as the Guardian with
  the whole home in reach, and systemd cannot sandbox it. Qt keeps and
  sends cookies, sends `Mozilla/5.0` and the system's languages unless
  told not to. Putting the pictures in root's folder would still need a
  password.
- **Fetch as root, inside `cairn-family`.** A network answer would be
  parsed as root.
- **Run as the Guardian, with no home in reach** (the tools step's
  way). A program running as the Guardian can reach the Guardian's
  other running programs: it can read and write their memory with
  `process_vm_readv` and `process_vm_writev`, which systemd's
  `@system-service` filter allows (`systemd-analyze syscall-filter`),
  or through `/proc/<pid>/mem`. Fedora sets `kernel.yama.ptrace_scope
  = 0` (`/usr/lib/sysctl.d/10-default-yama-scope.conf` on the
  development computer, Fedora 44), which does not stop it.
  `ProtectProc=invisible` hides only other accounts' processes. A step
  taken over through a bug in Python or OpenSSL could then reach
  Family, Steam or the desktop. A throwaway account closes that.
- **A fixed system account from sysusers.d.** As good as a throwaway
  account. It needs a new account in the image; `DynamicUser=` needs
  none, and keeps its folder under `/var/lib/private`, where no other
  account can reach it.
- **The frame-size check in the store step.** The store step runs on
  the untrusted side; a wrong or taken-over step would skip it. Root's
  copy is the boundary, so the check is there, once.
- **`header_2x.jpg`, 920x430.** Only new games have it, and a tile draws
  the picture at about 373x174.
- **`library_header.jpg` from the header's folder.** It matched all 11
  games, but by a coincidence Valve does not document.

## Checks in the VM

In cairn-min, with an image built from slices 2 to 5, the Guardian's
own Steam closed and no child logged in, so only the store step looks
up Valve's names. `resolvectl monitor`, run as root during each run,
shows every name looked up. A shared real Steam game, such as Arthur's
Birthday (2471140), is made to lack a picture by moving its header out
of the Guardian's `librarycache` and its picture out of
`/var/lib/cairn/game-pictures`, as ADR-0047's checks did.

1. No answer stored: Save shows the question before the password box,
   with the words above. Focus is on the first paragraph, and Enter
   does nothing. Tab reaches both buttons. Escape closes it, with no
   password box, no settings file, the changes and the "Not saved yet"
   sentence still there, and focus on Save.
2. No, thanks, then the password: the Save is done, the file holds
   `no`, root's, mode 0644, and no lookup of `api.steampowered.com` or
   `shared.steamstatic.com` is made. The next Save asks nothing.
3. With `no` stored, Get pictures from Steam's store: the box with what
   is sent, focus on its words. Cancel: no password box and no lookup.
   Again, then Get pictures: its own password message, a fetch, and the
   file still `no`.
4. Turn on: the question's words, focus on Cancel; Turn on, the
   password, the progress window, the file holds `yes`, and the missing
   picture is fetched.
5. With a yes, a Save: one password; the progress window says the
   changes are saved before it says it is getting pictures; lookups of
   the two names only; `store step: fetched 2471140` and `store copy:
   copied 2471140` in the journal; the card's picture, and ada's tile's
   at her next login. `/var/lib/private/cairn/store-pictures` is empty
   afterwards.
6. With a yes and every shared game pictured, a Save looks up neither
   name.
7. The header put back in the Guardian's `librarycache`: a Look uses
   it and asks the store nothing for that game.
8. The VM's network link down, Get pictures from Steam's store: the
   offline sentence, `store step: offline`, the pictures folder byte for
   byte the same (`sha256sum` before and after), and the download
   folder empty.
9. A stand-in store step that writes a text file, a 2 MiB file, a link
   to `/etc/shadow`, a FIFO and a JPEG whose frame header says
   65535x65535 under app ids' names, and says `fetched` for each, for a
   game that already has a picture, and for an app id it was not asked
   for: each `store copy: refused APPID REASON` or left out, and the
   pictures folder unchanged.
10. A stand-in store step that tries to list `/var/home` and `/home`,
    write in `/var/lib/cairn/game-pictures`, open a netlink socket,
    connect to `/run/dbus/system_bus_socket`, list other processes in
    `/proc`, call `process_vm_writev` on Family's process id and write
    to `/proc/<that id>/mem`: each refused. The same step resolves
    `api.steampowered.com` with no `AF_UNIX`, and `id` shows an account
    that is not the Guardian. `systemctl show cairn-store-pictures`,
    during a slowed run, lists the sandbox. The real step runs under
    `MemoryDenyWriteExecute=yes` and `ProcSubset=pid` (Fedora's OpenSSL
    reads `/proc/sys/crypto/fips_enabled`; whether its absence matters
    is checked here). No AVC denial (`ausearch -m AVC -ts recent`).
11. A stand-in store step that waits for ever: `store-stopped` about
    two minutes after `store-asking`, the pictures it fetched before
    copied, and the download folder empty.
12. From ada's session (L1), `pkexec cairn-family get-pictures` and
    `set-store-pictures yes`: refused, with no password box.
13. Turn off: the password, then the file holds `no`, and the next Save
    looks up neither name.
14. No journal line holds a `/`, a game's title or anything the store
    sent.
15. Recorded: whether `ProtectHome=yes` alone already hides `/var/home`
    on the image; the image's systemd version; and that the download
    folder is `/var/lib/private/cairn/store-pictures`, with
    `/var/lib/cairn/store-pictures` a link to it, and that the next run
    gets the folder root emptied, owned by its own throwaway account.

## Checks on the kids' laptop

On an image built from slices 2 to 5:

1. The next Save brings the question. With a yes, every shared game
   whose tile showed its name has a picture, or Family names it.
2. Wi-Fi off, Get pictures from Steam's store: the offline sentence,
   and no picture changes.
3. A child's tile shows a store picture at their next login, and the
   launcher's journal has no sentence about a picture.

## What comes later

- **A page in Family that says what the computer sends** (planned, with
  an ADR of its own later). The answer can move there as one row:
  "Pictures from Steam's store: on" or "off". The row says what is sent
  (this computer's internet address, the time, its country setting, the
  Steam numbers of the games that need a picture) and to whom (Valve,
  and the companies that deliver Steam's store over the internet). Its
  "off" must say that the button still fetches when pressed, as the
  Games page's line does, so "off" never reads as "never". It reads and
  writes the same file, through `set-store-pictures`. A "last asked"
  date needs no new file: the page could read the `store step:` lines
  from the journal, as the Notices page reads the journal (ADR-0054),
  for as long as the journal keeps them.
- **The first-boot wizard** (DESIGN §4.3.1, step 5, P1-6) can ask the
  same question, and store the same file.
- **The capture on the wire** (#36) should include a Get pictures from
  Steam's store press.
- **The frame-size check for the Guardian's own Steam's pictures.**
  ADR-0035 accepted those as they are; root could check them the same
  way later.
- **Pictures for the other tiles** stay ADR-0035's later decision.
