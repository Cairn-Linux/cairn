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
removes or changes the group of a count; a good login and
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

A count made before, or brought by a moved computer, is put back to
group root and mode 0660 at every boot.
Its owner is left alone: pam_faillock sets it at each try, and no one
line can name each account's owner.

- **On the image:** two lines in
  `system_files/usr/lib/tmpfiles.d/cairn.conf`:
  `d /var/lib/faillock 2755 root root` and
  `z /var/lib/faillock/* 0660 - root`.
- **On a computer set up by hand:** `provision/cairn-provision.sh` makes
  the folder 2755 and puts each count back the same way, on every run.
- **New accounts:** nothing. The Family app's `useradd` and the family's
  setup make no count; the folder gives the first one its group.
- **The image's build** runs the real lines: `systemd-tmpfiles --root`
  over a copy of the folder, then the image's own pam_faillock through
  libpam, as root with another group, as `su` runs it
  (`build_files/check-faillock-owner.py`). It also checks that the new
  count keeps ADR-0052's read entry for `sddm`, and that a good login and
  `faillock --reset` leave the group alone. A run in a folder without
  setgid comes first, to show the check sees the caller's group at all.

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
- At most the first boot after this change, a count made by `su` earlier
  is still in the child's group until tmpfiles runs, which is before the
  login screen starts.
- Setgid on the folder changes nothing else: faillock makes only files
  there, and only root can make anything there.
- ADR-0052's check 1 now expects tmpfiles to put back group root, not
  to leave the group as it was.
- No SELinux change: the label comes from the path, not the group, and
  `restorecon` and tmpfiles' `z` keep `faillog_t`.
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
3. A count put in the caller's group by hand
   (`chgrp <caller> /var/lib/faillock/<login>`) is back in group root
   after `systemd-tmpfiles --create`, with its owner and ACL kept.
4. A good login and `faillock --user <login> --reset` leave the count in
   group root.
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
