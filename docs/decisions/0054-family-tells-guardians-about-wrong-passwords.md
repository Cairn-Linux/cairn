# ADR-0054: Family tells Guardians about wrong passwords

**Status:** proposed. It becomes accepted when the checks under "Checks
in the VM" have passed on cairn-min.
**Date:** 2026-11-05
**Closes:** (none). Issue #285, first slice. Builds on ADR-0040 (Family),
ADR-0051 (the lock screen's fingerprint) and ADR-0052 (the login screen
says when an account is locked).

## Context

On 2026-11-04 a Guardian was locked out by faillock and thought they had
forgotten their password (#283, #284).
Nothing on the computer tells a Guardian, later, that wrong passwords
were typed for their account or a child's, where, or that the account
locked.
A child guessing at a Guardian's password leaves no trace a parent will
ever see.

What the journal already records, checked on the dev PC (Fedora 44,
systemd 259) and, by the design's author, on cairn-min (systemd 262,
pam 1.7.2):
- Every failed password check through PAM writes an audit `USER_AUTH`
  record (type 1100) with `res=failed`. journald splits its fields:
  `AUDIT_FIELD_ACCT`, the account whose password was tried;
  `AUDIT_FIELD_EXE`, the program; `AUDIT_FIELD_OP`; and the process's
  `pid`, `uid` and `auid` as `_PID`, `_UID` and `_AUDIT_LOGINUID`.
  Only a process with `CAP_AUDIT_WRITE` can write one.
- The lock screen runs as the person and cannot write audit, but
  pam_unix's setuid helper `unix_chkpwd` can. A non-root `unix_chkpwd`
  checks only its caller's own password, so its records are "in that
  account's own login": the lock screen, or any other program run there.
- When faillock *announces* a lock, it writes an audit
  `ANOM_LOGIN_FAILURES` record (type 2100) naming the account by number
  (`suid=`), where the program can write audit (the login screen, sudo,
  su, polkit's helper), and a line at info, "pam_faillock(<service>:auth):
  Consecutive login failures for user <login> account temporarily locked".
  The lock screen writes only the line, from `kscreenlocker_worker`
  (Plasma 6.8; `kscreenlocker_greet` before it), as the person, so
  journald keeps it in that person's own `user-<uid>.journal`.
- Not every lock is announced. pam 1.7.2's `write_tally` announces only
  when its own count of records equals `deny`, and that count takes in
  the records an ended lock left behind, which `check_tally`, the rule
  that really locks, no longer counts. So a lock that starts within
  `fail_interval` of an ended one usually passes `deny` silently, and,
  in one narrow case, a lock is announced one try before it happens
  (cairn-min, 2026-11-05 and 2026-11-06; the source, `pam_faillock.c`,
  is unchanged on pam's main branch). faillock's preauth is `required`,
  so the stack goes on after it refuses, and `unix_chkpwd`'s own record
  says `res=success` for the right password typed while locked; only
  the whole PAM check's `USER_AUTH` record says it failed.
- systemd's own `tmpfiles.d/systemd.conf` gives the groups `adm` and
  `wheel` read access to every journal file, the user files too: through
  a default ACL on the machine's folder, which each new
  `user-<uid>.journal` takes (cairn-min, 2026-11-06: `guardian` and
  `bazzite`, both in `wheel`, read every user's file, rotated ones too).
  Every Guardian Cairn makes is in `wheel` (ADR-0023, ADR-0039).
  Children are not, and can read only their own user journal.
- journald keeps up to 10 % of `/var`, at most 4 GB, with no time limit:
  weeks to months, less on a disk Steam has nearly filled.

## Decision

Family gains a fourth page, **Notices**: each time wrong passwords were
typed for an account on this computer in the last 30 days, for every
account, where, from whose login, and whether that locked the account.

**Where it reads.** Only the journal, as the Guardian, through `wheel`.
No helper, no pkexec, no new group, no new ACL.
A pkexec helper would ask for a password each time the page opens, and
each wrong one would itself count toward a lock.
faillock's own records under `/var/lib/faillock` are not read: they keep
15 minutes, a good login empties them, and they say nothing of a lock
that ended.

**How it reads.** `/usr/bin/journalctl -o json`, started directly with a
clean environment, read line by line while it runs (`JournalRun`), and
parsed with `QJsonDocument`; libsystemd's `sd-journal` would be a new
dependency. Three runs, one after another (`TryReader`): the system
journal's oldest entry; the lock screen's lines,
`SYSLOG_FACILITY=10 _EXE=/usr/libexec/kscreenlocker_worker` (and
`kscreenlocker_greet`, and both under `/usr/lib`), from every journal
file, with neither `--system` nor `--user`, because the lock screen
writes to the person's own; then, from the system journal only, the
audit records of password checks, the account part's successes and locks
(`_TRANSPORT=audit _AUDIT_TYPE=1100 + _TRANSPORT=audit _AUDIT_TYPE=1101
AUDIT_FIELD_RES=success + _TRANSPORT=audit _AUDIT_TYPE=2100`), each
`--since=-30d`.
Before it runs anything, `TryReader` opens
`/var/log/journal/<machine-id>/system.journal`: journalctl run without
`wheel` succeeds with only the caller's own entries, and an empty list
would then say "no wrong passwords" and be wrong.

**What counts** (`JournalTries`, the trust boundary, with most of the
tests):
- *A try* is only an audit `USER_AUTH` record that failed, with
  `op=PAM:authentication` or `PAM:unix_chkpwd`. sshd's key checks and
  systemd's credentials are records of the same type with other `op`s.
  A child can cause a real one, by really trying, but cannot write one.
  pam_unix's syslog line says the same and anyone can write it with
  `logger`, so it is not read.
- *Places*, by program: `sddm-helper` and `plasmalogin-helper` (Fedora
  44's own login screen, which a rebase can bring) are the login screen;
  `unix_chkpwd` is "in their own login, at the lock screen or in another
  program"; then sudo, su, polkit's helper ("a password box") and `login`
  (a text console). Any other program is "another program (name)", never
  dropped: a way in nobody knows about is what a parent should see. Its
  name shows only when it is plain letters, digits and `._+-`.
- *A lock* is faillock's audit record, whose account is a number looked
  up on this computer; or faillock's line from the lock screen's own
  program, `/usr/libexec/kscreenlocker_worker` or
  `/usr/libexec/kscreenlocker_greet` by full path (a child's own program
  of that name, in their home, is not it), for the `kde` or
  `kde-fingerprint` stack (ADR-0051), believed only when that program ran
  as root or as the account it names. A child
  can send that line with a genuine `_EXE`, by `LD_PRELOAD` into the lock
  screen, so the uid rule is the real guard, and it limits a forged lock
  to the child's own account. faillock's line from su carries the
  caller's uid and is dropped; the audit record covers su.
- *A right password* is a `USER_AUTH` record of a whole PAM check that
  succeeded (never `unix_chkpwd`'s, which can succeed while faillock
  refuses), followed from the same process by a `USER_ACCT` success:
  faillock's account part, which empties the count only after its own
  password check in that process. The lock screen never runs it
  (ADR-0053). A right password at all while the page counts the account
  locked means the count was emptied out of sight. The login screen lets
  a level 1 or 2 child in before the password stack
  (`session/sddm/pam.d/sddm`), so faillock never runs there and their
  login writes successes that empty nothing: a login screen success for
  an account in `cairn-l1` or `cairn-l2`, and in neither `cairn-guardian`
  nor `wheel`, is not a right password.
- *Names* in an entry are never shown. `AUDIT_FIELD_ACCT` is the name PAM
  was given. At su and sudo it is always an account (su refuses a name
  that is no account before PAM, sudo asks for its caller's own
  password), but a prompt that takes a typed name, such as a text
  console's `login`, passes on whatever was typed: spaces, line breaks,
  markup, bidi controls.
  Each name is looked up with `getpwnam`; an account is shown by its
  full name with its login name beside it, "Ben (ben)", and anything else
  as "a login name that is not an account on this computer". A full name
  alone would not do: polkit lets an account change its own full name
  with no password (ADR-0043), so a child who set theirs to "Mum" and
  then tried su would read as the other parent. Control and format
  characters are dropped from the full name, so a bidi override cannot
  turn the login name round. A field journalctl gives as bytes counts as
  no name. Every `Text` on the page is `Text.PlainText`.
- *From whose login*: when `_AUDIT_LOGINUID` is set and is not the
  account tried, as for su from a child's login. su and sudo keep it
  whoever they run as. It names a place, not a person: anyone at the
  keyboard could have typed.

**Grouping** (`TryNotices`). Tries for one account from one place and one
login, each within faillock's `fail_interval` of the last, are one
notice, with their count, first and last time, until a wrong try after
that notice's lock ended, which starts the next. Tries are counted, not
kept, so a flood takes no more room than its count.

**Locks worked out from the tries** (`FaillockTally`). Since faillock
does not announce every lock, each account's tries, in time order from
every place, go through faillock's own two rules, as `check_tally` and
`write_tally` are written, faults included: with `deny` valid records
within `fail_interval` of the newest, every try is refused until
`unlock_time` after it; once it ends, the next try takes every record out
of the count. A try refused that way was refused whatever was typed, the
right password too, and is never called wrong: "It was tried twice more
while it was locked." The journal has the last word where it speaks:
- A lock from the same process as a try (or, for the lock screen, the
  line just after its try) goes with that try, and the count takes it,
  even if the tries it saw were fewer (wrong fingers leave no record).
- A lock the count did not see coming, when faillock would have said so,
  did not happen: someone emptied the count out of sight, with
  `faillock --reset`.
- A lock in the journal inside a lock the count still holds means the
  same. When a try's process wrote it (or, at the lock screen, its line
  came just after a try faillock announced), the tries refused since, at
  most `deny` less one, the newest within `fail_interval`, become wrong
  ones: never more than the lock needs, because nothing says when the
  reset came. A lock inside a lock that no try explains, such as one
  from wrong fingers, empties the count and shows as a lock, and the
  tries refused before it stay refused: the fingers may have been all of
  it.
- The lock screen's line just after a try faillock announced but did not
  make by its own rule is that announcement, not a lock.
- A right password, then the account part in that process, empties the
  count; a right password while the count holds a lock empties it too,
  and the tries refused before it stay refused.
- A lock no try explains, such as one from wrong fingers, goes with the
  open notice of wrong tries in that login, or is a notice of its own.
At most 200 notices show, the newest, with "N older notices are not
shown." Without `faillock.conf` read, or with `unlock_time = never`, no
lock is worked out and no try counts as made while locked; only the
journal's locks show.

**Words** (`NoticeWords`, all `tr()`). Examples, for a Guardian:
- "Your password was typed wrong 3 times at the login screen, on
  Wednesday, 4 November 2026, from 16:02 to 16:05. That locked the account
  for 10 minutes, at every password prompt. The right password did not
  work until about 16:15."
- "Your password was typed wrong 3 times with su, from Ben's login, on …"
- "Someone tried to use Ada's account 3 times with su, …" for a child at
  level 1 or 2, who has no password, so no try is "wrong".
- When a lock has fewer wrong passwords than faillock's `deny`: "Wrong
  tries at other prompts, and wrong fingers at the lock screen, count
  toward a lock too."
- The sentences come in the order things happened: the wrong passwords,
  the lock they caused ("That locked …"), why it took fewer than `deny`,
  then the tries refused while it held. "That" is the wrong passwords, so
  a notice of tries refused while locked never says a lock came of them.
  A try refused under another lock than a notice's own goes on a notice
  of its own. On a notice without a lock, tries refused under a lock
  from elsewhere may have come before the wrong ones, so they read "In
  that time it was also tried twice while the account was locked."
- Under the list: "3 wrong tries within 15 minutes, by password or by
  finger, lock an account for 10 minutes. If this was you, there is
  nothing to do. If it was not, someone may be guessing: a password
  nobody has watched you type is the best protection. Children do not
  see this page." The numbers come from `faillock.conf`, read by
  `FaillockSettings`, moved to `accounts/` so the login screen and Family
  share it.
- "This computer's record goes back only to …" when the journal's oldest
  entry is newer than 30 days; "Family cannot read this computer's
  record of password tries from your account. An account in the wheel
  group can."; and "Family could not read …" when journalctl fails.
- Dates and times are the locale's own (`QLocale`), never built by hand.
- The keyboard reads the page: each notice, and the words under them,
  takes the keyboard in turn with Tab, the window scrolling to show it;
  Down and Up go between notices; Page Down and Page Up scroll and take
  the keyboard to a notice in sight. Each keeps its sentence as its
  `Accessible.name`.

**What Family keeps.** Nothing. The journal is already the record; a
second log of the family's tries would be a new store of personal data
to own. Nothing leaves the computer.

**Who sees it.** Every Guardian sees every account's tries, other
Guardians' too: they share the job, and the journal is already theirs to
read. Children never see the page (ADR-0040) and nothing in a child's
session changes. The parent docs may tell older children that Guardians
can see wrong passwords, as each family chooses.

### Not in this slice

- Marking new notices, and "Notices (2 new)" on the button: a file of
  the newest time seen in the Guardian's own home. Left out to keep the
  first slice to "was I locked, and why"; it brings state and a write
  rule of its own.
- A desktop notification at a Guardian's login when there are new
  notices.
- "Locked right now, N minutes left", which needs faillock's records.
- Offering a security key beside a notice (ADR-0020).

### Where the design and its review differed

The design (`#285`) and a review of it disagreed on a few points; this
ADR takes the safer side of each.
- The review found that `_UID` and `_AUDIT_LOGINUID` are structured
  fields, so `MESSAGE` is never parsed for an audit record.
- The review noted that polkit 127 can run its helper as a socket-started
  root service, whose records would carry no caller. On the dev PC the
  helper is still setuid and its record carries the caller's uid and
  auid. Either way "from whose login" is only said when auid is set and
  differs, so the worst case is no "from".
- The review pointed out that a child can run `unix_chkpwd` directly, so
  that place says "at the lock screen or in another program", not "at
  the lock screen".
- The review asked for a read that cannot be made to fail by a flood:
  journalctl runs asynchronously, line by line, with no byte cap that
  fails the whole read. Over-long lines (64 KiB) are skipped.
- The design excluded sshd; this slice shows it as "another program",
  since no records exist unless sshd runs, and then a parent should see
  them.
- The design had the "seen" file in slice 1; the review moved it to
  slice 2. It is in slice 2.

## Consequences

- A Guardian sees, without a password and without a terminal, that wrong
  passwords were typed, where, and whether it locked an account. The
  kids' laptop's lock-out of 2026-11-04 would have shown as one notice.
- Nothing new is installed with privilege: the read rides on systemd's
  own `wheel` ACL. A Guardian outside `wheel`, which Cairn never makes,
  is told the page cannot read the record.
- Tries are only as complete as journald's audit intake. If the kernel
  runs with `audit=0`, or `systemd-journald-audit.socket` is off, tries
  vanish silently. Each rebase's VM check catches that.
- A login screen whose helper is neither `sddm-helper` nor
  `plasmalogin-helper` shows as "another program" until it is added.
- A child can make real tries on any account with su, and fake a lock
  screen lock on their own account. Both show as what they are; neither
  can hide a real try.
- A full disk shortens the record below 30 days, and the page says so.
- "about 16:15" is wrong if someone ran `faillock --reset`; hence
  "about". Tries after such a reset that lead to no new lock are still
  counted as refused, since the journal does not say a reset happened.
  With `unlock_time = never`, no try is called "while locked", because
  the journal does not say when the lock was reset.
- The page follows pam 1.7.2's rule, faults included. If pam fixes
  `write_tally`, every lock will be announced, and the page still agrees:
  each announced lock is the journal's word. A pam whose `check_tally`
  changed would need `FaillockTally` changed with it; each rebase's VM
  check is where that shows.
- The lock screen's locks are in each person's own journal file. A file a
  Guardian cannot read (a `wheel` ACL taken away by hand) is skipped by
  journalctl without a word; Family logs it, and the page shows those
  tries with the locks worked out from them. For that account a lock
  worked out at the lock screen stands without its line, where otherwise
  a missing line would mean the count was emptied out of sight.
- The page reads the journal each time it is shown, which took 0.2 s on
  cairn-min for the audit run; the lock screen run reads every user
  journal and is the slow one. The window never waits for either.

## Checks in the VM

On cairn-min, with the image built from this change:
1. `_AUDIT_LOGINUID` and `_UID` on polkit-agent-helper-1's records: if
   0 and unset, "from whose login" is never said for password boxes.
2. `su 'x y'` from a test child: su refuses it before PAM, so there is
   no record and the page shows nothing. (A name that is no account
   reaches PAM only where a name is typed, such as a text console; the
   page then says "a login name that is not an account".)
3. faillock's audit record has `AUDIT_FIELD_SUID` (journald's name for
   `suid=`); without it every audit lock is dropped and only the lock
   screen's show.
4. The lock screen's faillock line has `_EXE=/usr/libexec/kscreenlocker_worker`
   and the account's own `_UID`, for `kde` and, where a reader exists,
   `kde-fingerprint`, and another Guardian can read it.
5. Three wrong tries at each of the login screen, the lock screen, sudo,
   Family's password box and `su - guardian` from a child's login give
   five notices with the right place, and "locked".
6. A `logger -p authpriv.notice` forgery of pam_unix's and faillock's
   lines from a child's login shows nothing.
7. A child's session can read neither the system journal nor another
   account's.
8. `audit=0` is not on the kernel command line and
   `systemd-journald-audit.socket` is active.

## Record

2026-11-05, cairn-min on `dev.20261105-b00fa88` (pam 1.7.2,
kscreenlocker 6.8.2), with the page as first written:
- Check 1 passed: polkit's helper is setuid root; its records carry
  `_UID` and `_AUDIT_LOGINUID` of the caller.
- Check 2 passed with nothing to show: su refused `x y` and an unknown
  name before PAM, so the check's premise was wrong (corrected above).
- Check 3 passed: faillock's record has `AUDIT_FIELD_SUID`, from
  sddm-helper, sudo, su and polkit's helper.
- Check 4 failed: the lock screen's line came from
  `/usr/libexec/kscreenlocker_worker`, with the account's own `_UID`, and
  the page read only `kscreenlocker_greet`, so no lock screen lock
  showed. `kde-fingerprint` was not run: no reader.
- Check 5 failed: the login screen and Family's password box showed the
  right place and the lock. The lock screen's lock was missing (check
  4), and two locks at sudo and at su from a child's login were missing
  because pam_faillock wrote neither record nor line for them: each
  started within `fail_interval` of an ended lock, and was reproduced on
  its own at 00:15. Tries refused during those locks were called wrong.
  After a `faillock --reset` between places, notices said "tried 3 times
  … while it was locked … That locked the account", and a second lock
  from one place showed as a notice of its own beside its tries.
- Checks 6, 7 and 8 passed: `logger` forgeries from a child's login,
  `kscreenlocker_greet` and `kscreenlocker_worker` alike, showed nothing;
  the child could read no journal but their own; audit is on.
- The page could not be scrolled from the keyboard.

2026-11-06, after the fixes above: on cairn-min (the same image, booted
from the snapshot `after-290-291-2026-11-05`), `guardian` and `bazzite`
read every user's journal file through `wheel`'s default ACL, and each
read the other's `kscreenlocker_worker` lock line; with `--system`
neither did. The journal of 2026-11-05 and 6, read as `guardian` with
the new queries and replayed through `JournalTries`, `TryNotices` and
`NoticeWords` on the dev PC, gave the notices that happened: the lock
screen's lock, the silent locks at sudo (23:28, two wrong, one refused)
and at su from a child's login (23:57), the two locks of 00:04 as two
notices, the right password refused at the login screen at 22:55 still
counted as refused, and no notice contradicting itself. The new image is
not built yet, so checks 4 and 5 wait for it in the window; the ADR stays
proposed until then.
