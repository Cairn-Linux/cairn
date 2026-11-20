# ADR-0053: Every count of wrong passwords stays in group root

**Status:** proposed. It becomes accepted when the checks under "Checks
in the VM" have passed on cairn-min, then on the kids' laptop after its
next image update.
**Date:** 2026-11-05
**Closes:** (none) — issue #289. Amends ADR-0020 (the faillock folder)
and ADR-0052 (its check 1).

## Context

From L3 a child has a real shell (DESIGN.md §3, the capability
ladder), and so `su`.
pam_faillock keeps each account's count of wrong passwords in
`/var/lib/faillock/<login>`.
linux-pam 1.7.2's `open_tally` (`modules/pam_faillock/faillock.c`)
makes the file with `open(path, O_RDWR | O_CREAT, 0660)`, then
`fchown(fd, uid, -1)`: it sets the owner to the account and leaves the
group as the file was made.
It adds group write if the file lacks it, and nothing else.
Nothing in pam_faillock, nor in the `faillock` command, ever renames,
removes or changes the group of a count; a good login (at the login
screen, sudo or su, not at the lock screen: ADR-0051) and
`faillock --reset` only empty it.

`su` is setuid root, not setgid, so it runs with the caller's group.
A child's first wrong `su - <guardian>`, where the Guardian has no count
yet, makes the Guardian's count in the child's group, mode 0660.
On cairn-min, `bazzite` ran `su - ben` with a wrong password and
`/var/lib/faillock/ben` came out `ben:bazzite 0660`.
From then on the child can write the Guardian's count:
- empty it between guesses, so guessing the Guardian's password is never
  slowed, or
- fill it, to lock the Guardian out for 10 minutes at a time.

ADR-0052's default ACL made no difference: its `d:g::rw` entry is the
file's own group, whichever that is.

## Decision

`/var/lib/faillock` is setgid, group root, mode 2755.
A file made in a setgid folder takes the folder's group, whoever makes
it, so every count is `<login>:root 0660` from the first wrong try,
whatever program made it.
This is the kernel's rule, and pam_faillock does not change the group
afterwards.

A count made before, or brought by a moved computer, is reconciled at
boot before SDDM starts. A small helper opens the faillock directory and
each tally with `O_NOFOLLOW`, verifies the pathname still names the open
regular file, resolves the filename to its current local account, and
sets `<account>:root 0660`. It replaces the access ACL with exactly the
owner and root-group write access plus read-only `sddm`; an old named
user cannot keep write access. A symlink, non-regular file, missing
account, pathname replacement or failed ownership/ACL operation stops
the service, and SDDM requires that service.

- **On the image:** `system_files/usr/lib/tmpfiles.d/cairn.conf` makes
  `/var/lib/faillock` setgid and gives new counts the default `sddm` ACL.
  `cairn-reconcile-faillock.service` runs the shared helper after
  tmpfiles and requires it before SDDM, then restores SELinux labels.
- **On a computer set up by hand:** `provision/cairn-provision.sh` makes
  the folder 2755, installs the same helper, and runs it on every pass.
- **New accounts:** nothing. The Family app's `useradd` and the family's
  setup make no count; the folder gives the first one its group.
- **The image's build** runs the real tmpfiles lines and shared helper
  over a migrated tally owned by a distinct attacker with a named write
  ACL, then permanently drops a child process to that attacker and
  proves open-for-write with truncation is denied. It separately runs
  the image's pam_faillock through libpam, as root with another group,
  as `su` does (`build_files/check-faillock-owner.py`). Successful
  authentication and `faillock --reset` each get their own non-empty
  tally and assertion, so one cannot hide a failure in the other.

Set aside:
- **A count made ahead for each account,** at boot or when an account is
  made. It needs a tmpfiles line or a step per account, in the Family
  app's C++ as well as provisioning, and still leaves a gap for an
  account made outside Cairn. The folder covers every account at once.
- **A pam_faillock option.** 1.7.2 has none for the group or mode.
- **Group `cairn-guardian` on the folder.** Guardians would then be able
  to write every child's count, and each other's. Root is the group
  faillock's own comment expects ("owners of this file are user:root").
- **A default ACL alone.** A default ACL cannot change a file's owning
  group.
- **Taking `su` away from children.** The ladder gives L3 a real shell
  on purpose, and ADR-0020 already counts `su` tries.

## Consequences

- A child with `su` can no longer empty or fill another account's count.
  Each count's owner can still write their own, as before: that is
  upstream's choice, and only the account itself can use it.
- At most the first boot after this change, an older count is unsafe until
  the reconciliation service runs; SDDM cannot start if repair fails.
- Setgid on the folder changes nothing else: faillock makes only files
  there, and only root can make anything there.
- ADR-0052's check 1 now expects reconciliation to restore the exact
  owner, root group, mode and read-only `sddm` access.
- No SELinux change: the label comes from the path, and the service runs
  `restorecon` after reconciliation as provisioning already does.
- If a later pam renamed counts into place, or changed their group, the
  build's check fails, and this needs a new look.

## Checks in the VM

On cairn-min after an image update with this change, then on the kids'
laptop:
1. `stat -c '%U:%G %a' /var/lib/faillock` shows `root:root 2755`.
2. As an account outside `cairn-guardian` with `su`, `su - <another
   account>` with a wrong password where that account has no count: the
   count is `<login>:root`, mode 0660, with `user:sddm:r--` in
   `getfacl`.
3. A Guardian-named count planted with a child's numeric owner and named
   `rw` ACL is back to `<guardian>:root 0660` after starting
   `cairn-reconcile-faillock.service`; its only named access entry is
   `user:sddm:r--`, and the child cannot truncate it.
4. A good login and `faillock --user <login> --reset` each clear a fresh
   non-empty count independently and leave it in group root.
5. On the kids' laptop, after its image update, `ls -ln /var/lib/faillock`
   shows group 0 on every count.

## Record

2026-11-05, cairn-min on `dev.20261105-00f4d67`, with this change's
`cairn.conf` laid over it as `/etc/tmpfiles.d/cairn.conf` rather than
built in: checks 1 to 4 passed, `bazzite` (not a Guardian) standing in
for an L3 child. The count `ben` that the #288 audit's `su` had left in
group `bazzite` went back to group root. A new count for `guardian` made
by `bazzite`'s wrong `su` was `guardian:root 660`, and `bazzite` could
not empty it. They wait for an image built with the change, and check 5
for the kids' laptop.

2026-11-06, cairn-min on `dev.20261105-b00fa88`, an image built with this
change and #285's: checks 1 to 4 and 6 passed with the change built in.
The build check ran and passed. A count planted in a child's group, and
one at mode 0666, came back `<login>:root 0660` with the `sddm` entry at
boot and from `systemd-tmpfiles --create`. A good su, sudo and login
screen login, and `faillock --reset`, kept group root. The lock screen
still writes counts, and three wrong unlocks still lock. Check 5 waits
for the kids' laptop.
