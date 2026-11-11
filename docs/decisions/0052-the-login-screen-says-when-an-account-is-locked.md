# ADR-0052: The login screen says when an account is locked

**Status:** proposed. It becomes accepted when the checks under "Checks
in the VM" have passed on cairn-min, then on the kids' laptop after its
next image update.
**Date:** 2026-11-05
**Closes:** (none) — issue #284. Amends ADR-0020 (the login screen) and
ADR-0049 (check 5's words).
**Amended by:** ADR-0053, proposed (the folder is setgid, so every new
count is in group root, and the boot reconciler repairs older counts)

## Context

On 2026-11-04 the Guardian on the kids' laptop was locked out by
faillock (ADR-0020, #283).
The login screen said "That password did not work. After a few wrong
tries, the account waits a few minutes before it lets anyone try again."
It said so for the right password too, and never said whether the
account was locked or for how long.
The Guardian thought they had forgotten their password.

The login screen could not see a lock:
- `/var/lib/faillock/<login>` is mode 0660, owned by the account, with
  the group of whoever made it (root for the login screen and sudo, the
  caller's own group for `su`), and the greeter runs as user `sddm`;
- authselect's `pam_faillock.so preauth silent` sends the greeter no
  message;
- libpam has no call that asks, and the `faillock` command opens the
  file read-write, so it fails as `sddm`.

## Decision

The greeter reads faillock's own record, read-only, and works out the
lock with faillock's own rule.
PAM stays the gate; nothing in any PAM stack changes.

**The read permission.**
systemd-tmpfiles puts a default ACL on `/var/lib/faillock`:
`d:u::rw,d:u:sddm:r,d:g::rw,d:m::rw,d:o::-`. The boot reconciliation
helper gives files already there the exact matching access ACL before
SDDM starts (`system_files/usr/lib/tmpfiles.d/cairn.conf`, #301).
Provisioning installs and runs the same helper.
- Every file faillock makes from then on has a read entry for `sddm`
  and nothing else new.
- The default entries keep group root's write, which faillock gives each
  file so that root without `CAP_DAC_OVERRIDE` can still write it. A bare
  `d:u:sddm:r` would leave the default group entry at `r-x`, and the file
  would lose that write.
- The folder's own mode, 0755, already lets `sddm` in, so it gets no
  access entry.
- No SELinux change: the targeted policy lets `xdm_t` read `faillog_t`
  (checked on the dev PC's policy, not yet on the image).

**The rule** (`greeter/src/`), copied from linux-pam 1.7.2:
- `FaillockSettings` reads `/etc/security/faillock.conf` as
  `faillock_config.c` does: the same defaults, the same line splitting.
  A value faillock would refuse, an unknown key, or one this copy does
  not follow (`admin_group`, `even_deny_root`, `root_unlock_time`) gives
  no settings at all.
- `TallyFile` reads the 64-byte records, with the valid bit tested on
  its own, as faillock tests it. A try from sudo carries the terminal bit
  too. It takes a shared `flock` without waiting, and tries again a
  second later if faillock is writing. If the lock fails for any other
  reason, it is not sure.
- `LockRule` is `check_tally` and `write_tally`:
  - the window is counted back from the newest try;
  - the account is still locked at exactly the newest try plus
    `unlock_time`;
  - once a lock runs out, the next wrong try counts as the first;
  - "one more wrong try locks" counts back from now.
- `AccountLock` gives the theme one state for each thing it says, with
  minutes rounded up as faillock rounds them.

**When it is not sure, it never claims a lock.**
It falls back to today's words when:
- the settings are unclear;
- the record cannot be read, or holds a time no clock gives;
- the folder is missing;
- the login name is not a file name.
A missing file counts as "no wrong tries" only in a folder that is there
and readable.

**The words.**

| Situation | What the screen says |
|---|---|
| Locked, two or more minutes left | "This account is locked after too many wrong passwords. Try again in N minutes." |
| Locked, a minute or less left (faillock still refuses at 0) | "… Try again in less than a minute." |
| The lock ends while the screen shows it | "You can try again now." |
| A password sent while locked, answered after the lock ends | "You can try again now." |
| A wrong password, and the next locks | "That password did not work. One more wrong try locks this account for 10 minutes." (from `unlock_time`) |
| A wrong password, two or more tries left | "That password did not work. Try again." |
| `unlock_time = never` (not used on Cairn) | "… A grown-up can unlock it." |
| The record's newest try is later than now | "… Try again later." |
| Unknown | Today's words. |

- A locked account is never told its password was wrong. That holds for
  a password sent in the last seconds of a lock too: faillock refuses it
  for the lock, and PAM answers about two seconds later, when the lock
  may be over. The screen keeps whether the account was locked when the
  password was sent.
- A tile that is locked says so as soon as it is chosen, before anyone
  types.
- The password row stays open throughout. If this copy of the rule were
  ever wrong, the screen would only say the wrong thing; it would never
  keep anyone from trying. A try while locked writes no record, so it
  does not make the lock longer.
- The lock comes before the Steam visit (ADR-0032), because faillock is
  in PAM's auth step and the visit list in its account step.
- Only tiles that ask for a password are looked at. L1 and L2 children
  are let in before faillock.
- The theme loads no translation, so a sentence with "%n minute(s)"
  would show "(s)" as written. Each sentence is whole, and the minutes in
  it are always 2 or more.

Set aside:
- **A `preauth` line without `silent` in the greeter's PAM file.**
  faillock's message comes only after a try, in the system's language,
  and the greeter would have to pick it apart. It also changes a PAM
  stack.
- **A root `.path` unit that writes the lock to `/run/cairn`.** It needs
  the same copy of the rule, as new root code, and races with SDDM's
  "login failed".
- **Judging by the record's size and time.** Old records stay in the
  file, so it would claim locks that are over.
- **AccountsService, logind or Cairn's pkexec helpers.** None knows about
  faillock, and the greeter cannot start the helpers.
- **The greeter counting wrong tries itself.** It would miss sudo and the
  lock screen, and forget at every restart.
- **Making each count readable by everyone.** Children would learn when
  a Guardian mistyped.
- **Hiding the password row while locked.** It would make the greeter a
  second gate, and a mistake in the copy would keep a Guardian out.

## Consequences

- A Guardian is told they are locked, for how long, and when they can
  try again, whichever program made the wrong tries.
- **What `sddm` can now learn:** which accounts have recent wrong tries,
  when each happened, and where from: a terminal (`/dev/pts/1`), another
  computer's address, or a service name. Only code running as `sddm`
  can read it, which means the greeter and its theme. Root-only logs
  already hold the same facts. It cannot write, empty or reset a count,
  so it cannot unlock anything.
  A narrower choice, an ACL on each Guardian's file made at setup, would
  need a step for every new account; it is not taken.
- **What a child learns.** A child at the login screen sees the
  countdown and "one more wrong try", so learns exactly when the next
  guess is allowed. The rate does not change: three tries in each ten
  minutes. ADR-0020 already accepts that a child sees the lockout, and
  faillock's own message says as much.
- **The copy can drift.** Cairn keeps a copy of faillock's rule, pinned
  to linux-pam 1.7.2. On each Fedora rebase that moves `pam` past 1.7.x,
  someone rereads `check_tally` and `write_tally`. Drift only ever
  changes words.
- A setting on a module line in `/etc/pam.d` (`deny=5` after
  `pam_faillock.so`) overrides the file, and this copy does not see it.
  authselect writes none.
- The minutes are rounded up as faillock's own message rounds them, so a
  600-second lock reads "10 minutes" here as at sudo. faillock lets a try
  in only the second after the count reaches 0, so at each step the real
  wait can be up to a second longer than the screen says. Only "You can
  try again now." says a try is allowed. It never comes early, but can
  come up to a second late, because the screen reads the record once a
  second.
- The screen reads the record once a second while a locked tile is open,
  and not at all otherwise.
- ADR-0049's check 5 recorded today's words for a locked account; they
  are now the ones above.
- Not changed: the Plasma lock screen still says "Unlocking failed",
  sudo "Sorry, try again", and the polkit box "Authentication failure".
  They are upstream.
- A screen reader may not announce the countdown, because Qt announces a
  changed name only on the item with focus. ADR-0008 asks only for the
  names now.

## Checks in the VM

On cairn-min after an image update with this change, then on the kids'
laptop:
1. `getfacl /var/lib/faillock /var/lib/faillock/*` shows
   `default:user:sddm:r--` and `default:group::rw-` on the folder, and on
   each file `user:sddm:r--`, `group::rw-` and `mask::rw-`, with no
   other named entry. A count made by three new wrong tries shows the
   same. Starting `cairn-reconcile-faillock.service` leaves every tally
   owned by its current account in group root and mode 0660.
2. `ps -eZ | grep sddm-greeter` shows `xdm_t`, `ls -Z /var/lib/faillock`
   shows `faillog_t`, and `journalctl -b | grep 'AVC avc'` shows no new
   denial after a locked tile is chosen. (`ausearch` sees no AVCs on the
   image, so its empty answer proves nothing.)
3. Three wrong tries at the login screen: the third shows "locked …
   Try again in 10 minutes." at once, not on the fourth.
4. Three wrong `su - <guardian>` tries, then the Guardian's tile: the
   screen agrees with `faillock --user <guardian>`, and shows the lock
   before anyone types.
5. The right password while locked says "locked", never "did not work".
   After the lock, it logs in, and the count is emptied with its ACL in
   place.
6. "You can try again now." appears without anyone touching the screen,
   and the words change once a minute.
7. An L2 child with a stray count still logs in with one press.
8. The laptop's `/var` takes ACLs. The shared Steam library's already
   work there, so it should.
9. The right password sent in the last two or three seconds of a lock
   says "You can try again now.", never "did not work", and the next
   try logs in.

## Record

2026-11-05, cairn-min on `dev.20261105-b0797c5`: checks 1 to 7 and 9
passed, and an audit of the evidence confirmed them. Check 8 passed on
the VM only; the laptop's `getfacl` waits for its image update.
