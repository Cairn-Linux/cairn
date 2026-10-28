# ADR-0039: One root command changes the family's accounts

**Status:** accepted 2026-10-23. In the cairn-iso VM, the family's
setup made a child with the command on 2026-10-22, and on 2026-10-23 the
Guardian tool's window (ADR-0040) ran it through pkexec at the Guardian's
desktop: a cancelled prompt changed nothing, then the password moved a
child to level 1 and added another at level 1.
**Date:** 2026-10-22
**Closes:** (none) — the first slice of issue #167; part of ROADMAP P1-5
**Amended by:** ADR-0041, proposed (a `save-games` subcommand, with
answer lines of its own)

## Context

ROADMAP P1-5 asks for the Guardian's tools as a command first and a
window on top of it. Issue #167 asks for the window and provisioning to
call the same tested code.

Until now a child's account was made only by shell scripts run as root.
`provision/cairn-provision.sh` makes children on a stock Bazzite, and
`provision/cairn-family-setup.sh` makes them on a computer that runs the
image, from the same functions (ADR-0038). Both need someone at a
terminal with `sudo`. A parent with neither has no way to add a child or
move one to level 2.

The rules for a child's account are already fixed by earlier decisions:

- A child's level is the one level group the account is in (ADR-0011).
- The new group is added before the old one is dropped, so a run cut off
  in between leaves the more restricted of two, never none (#100,
  ADR-0023).
- An administrator, in `cairn-guardian` or `wheel`, is never a child,
  whatever else they are in (ADR-0023, #101).
- A child has no password. The login screen lets L1 and L2 in without
  one, and a locked password keeps `su`, ssh and the lock screen out
  (#35).
- Each child gets a "Steam for <child>" entry in a Guardian's menu
  (ADR-0032).

Only L1 and L2 have a child session so far. L3 and L4 would reach Plasma
with nothing of Cairn's in it.

Provisioning adopts an existing plain account as a child without asking.
That account keeps whatever it had before, and #113 asks to revisit it.

The rule for who an account is lived inside the login screen's module,
`greeter/src/AccountKind`. A second copy of the display-name rule was in
`guardian/src/AccountName`.

The only privileged path so far is ADR-0032's: a Guardian's window starts
`pkexec` with a root helper, polkit asks for an administrator's password,
and the helper checks the caller again itself.

## Decision

**One root command, `cairn-family`, written in C++ with Qt Core only,
makes a child's account and changes a child's level. The Guardian's
window will run it through pkexec, behind a polkit action of its own that
asks for a password for every change. The family's setup runs it as root
with sudo. It changes only accounts Cairn made, and gives only levels 1
and 2.**

### The command

It is installed as `/usr/libexec/cairn/cairn-family`, mode 0755, and lives
in `guardian/`, the Guardian's side. It has two subcommands:

| Subcommand | What it does |
|---|---|
| `create-child LOGIN LEVEL [--full-name NAME]` | A new account gets `useradd --create-home`, with the full name as its comment when one is given and its level group, then `passwd --lock`. An existing Cairn child gets its password locked if it is not, its full name if one is given and differs, and its level. A second run with the same values changes nothing. |
| `set-level LOGIN LEVEL` | An existing Cairn child gets its password locked if it is not, then the new level group, then leaves every other level group. A child already at exactly that level, with its password locked and its entry as written, is left alone. |

Both write the child's "Steam for <child>" entry in
`/usr/local/share/applications`, with the same text as provisioning's
`steam_entry`. On Bazzite that folder is the computer's own
`/var/usrlocal/share/applications` (ADR-0038). The entry is written
beside its place and renamed into place, so Plasma never reads half of
one. A renamed file keeps the label of the file it was written as, so
`restorecon` labels it again when `restorecon` is there.

The level group comes with `useradd`, not after it. A run cut off after
`useradd` then leaves a child the next run can finish, never an account
it would refuse.

A Cairn child is a person's account, number 1000 or more, in a level
group and not an administrator. Anything else under the name is refused:
an administrator, a Guardian, a system account, or an account in no level
group. Giving a level to an account made some other way waits for #113.

It answers on standard output in one line of machine words, as the visit
helper does: `done`, or `refused` and a reason.

| Reason | When |
|---|---|
| `not-root` | It is not running as root |
| `not-a-grown-up` | pkexec started it for an account that is not an administrator |
| `usage` | Not a subcommand with the arguments it takes |
| `bad-name` | The login name is not a plain one, `^[a-z_][a-z0-9_-]{0,31}$` |
| `bad-full-name` | The full name is empty, has spaces at an end, or holds a control character, a colon, a comma or a backslash |
| `bad-level` | Anything but `1` or `2` |
| `taken` | `create-child` names an account Cairn did not make |
| `not-a-child` | `set-level` names no Cairn child |
| `failed` | A system tool, or writing the entry, failed |

It exits 0 on `done` and 1 on a refusal. It writes one line to the
journal as `cairn-family`, with only values it has already checked. The
window will turn each reason into a sentence. The command itself has no
words for a person.

### Who may run it

It must run as root. When pkexec started it, `PKEXEC_UID` must be a
number, and that account must be an administrator, in `cairn-guardian` or
`wheel`. The command checks this itself rather than trust polkit alone,
as the visit helper does (ADR-0032). When it runs as root without
pkexec, it is the family's setup under sudo, and it goes on.

It runs `useradd`, `passwd`, `gpasswd`, `usermod` and `restorecon` by
absolute path, directly, never through a shell. Each gets a small
environment of its own, not its caller's. What a tool prints on its
standard output is dropped, so it cannot mix with the command's one
line. No secret ever passes through the command: a child has no password
to set.

### The polkit action

`com.cairnlinux.family` names the command's path. It is `auth_admin`
for an active session, never `auth_admin_keep`, so the parent types their
password, or uses a fingerprint, for every change. `allow_any` and
`allow_inactive` are `no`. The levels rule needs no change: it lets an
administrator through first and refuses every `com.cairnlinux.` action
to everyone else, before any prompt.

### The window

The Guardian tool runs the command (ADR-0040). It starts `pkexec`
directly with the command's absolute path, one change at a time, and
reads the one line back: `done` with exit 0, `refused` and a word with
exit 1, or pkexec's own 126 and 127 when the command never ran. Each
word and an answer it cannot read get a sentence of their own; pkexec's
two share one. Before it asks for the password, the window checks the
request with the command's own `FamilyRequest`, so a value the command
would refuse is refused without a prompt. It never gives `create-child` the login of an
account or group that already exists, except to finish an add of its
own that failed part way, with the same name.

### One rule for who an account is

The rule moves into a small static library, `accounts/`
(`cairn-accounts`), that the login screen and the command both link.
`Account` looks an account up through NSS, by name or by number, with its
full name. `FamilyRole` gives the role in ADR-0023's order with a numeric
level: a Guardian, then another administrator, then a child at the lower
of any two level groups, then not family. The login screen's
`AccountKind` keeps its kinds and asks `FamilyRole`. The window's
display name comes from `Account` too, and `AccountName` goes.

### The family's setup

`cairn-family-setup.sh` makes each child with the image's
`cairn-family create-child`, in place of provisioning's
`ensure_children`. Then provisioning's `make_steam_entries` writes every
child's entry, named on that run or not, with the same text. So a child left off the run still gets the
image's entry. It also removes the entry of an account that is no longer
a child. The Guardian's steps stay in bash.
`cairn-provision.sh` is unchanged; it goes away with the rest of
`provision/` (ADR-0038).

The setup compares the image's scripts with the checkout's, but a
compiled command cannot be compared that way. So it only requires the
command to be there. It stops on any answer but `done` and prints the
answer as it came, so an image from another commit can stop the run but
can never be taken for done. A child it names that already has an
account must be in a level group, checked before anything changes.

## Consequences

- The window, provisioning on the image and the tests all run the same
  code. Its trust boundary is in C++, with the plainest code and the
  most tests (CLAUDE.md): every refusal, the add-before-remove order, a
  second run, the entry's exact text, and the tools' environment, with
  stand-ins for the tools and the accounts so no test needs root.
- This is the first root helper in C++. The visit helper stays in bash.
- A parent types their password for every change. Adding two children
  asks twice.
- A family that wants an existing account as a child cannot do it from
  the tool, nor from the image's setup, until #113. The hand path on
  stock Bazzite still adopts one.
- No level 3 or 4 from the tool until an older child's session exists.
  The command keeps an existing L3 or L4 child a Cairn child, so
  `set-level` can bring one back to 1 or 2.
- Only the image has the command. Provisioning by hand on a stock
  Bazzite neither installs it nor its action, and keeps its own steps.
- A new child's home comes from `/etc/skel`, which carries Steam's
  autostart file. That is unchanged from provisioning.
- Nothing here is per child beyond the account and its entry. ADR-0011's
  per-child config (name, avatar, allowlists) is still to come.
- Left for later:
  - `reset-child`, which waits for the skeleton home and creations
    folder (P1-8).
  - `set-pin`, which waits for where the PIN hash lives (#35).
  - Copying the games for a new child. The visit helper refuses its
    copy mode under pkexec, so the window needs its own way (ADR-0033).
  - Making an existing account a child, or a Guardian (#113).
  - The Guardian's window on top of the command, the second slice of
    #167: built, ADR-0040.
- Dependencies: Qt Core and the C library only. pkexec through
  `QProcess` needs no new library, as in ADR-0032.
