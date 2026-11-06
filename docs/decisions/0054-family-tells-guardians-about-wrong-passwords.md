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
- When faillock locks an account, it writes an audit
  `ANOM_LOGIN_FAILURES` record (type 2100) naming the account by number
  (`suid=`), where the program can write audit (the login screen, sudo,
  su, polkit's helper), and a line at info, "pam_faillock(<service>:auth):
  Consecutive login failures for user <login> account temporarily locked".
  The lock screen writes only the line.
- systemd's own `tmpfiles.d/systemd.conf` gives the groups `adm` and
  `wheel` read access to every journal file, the user files too. Every
  Guardian Cairn makes is in `wheel` (ADR-0023, ADR-0039). Children are
  not, and can read only their own user journal.
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
`SYSLOG_FACILITY=10 _EXE=…/kscreenlocker_greet`, from every journal file,
because the lock screen writes to the person's own; then, from the system
journal only, the audit records of tries and locks
(`_TRANSPORT=audit _AUDIT_TYPE=1100 AUDIT_FIELD_RES=failed +
_TRANSPORT=audit _AUDIT_TYPE=2100`), each `--since=-30d`.
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
  program, for the `kde` or `kde-fingerprint` stack (ADR-0051), believed
  only when that program ran as root or as the account it names. A child
  can send that line with a genuine `_EXE`, by `LD_PRELOAD` into the lock
  screen, so the uid rule is the real guard, and it limits a forged lock
  to the child's own account. faillock's line from su carries the
  caller's uid and is dropped; the audit record covers su.
- *Names* in an entry are never shown. `AUDIT_FIELD_ACCT` is whatever
  was typed at su or sudo: spaces, line breaks, markup, bidi controls.
  Each name is looked up with `getpwnam`; an account is shown by its
  login tile's name, and anything else as "a login name that is not an
  account on this computer". A field journalctl gives as bytes counts as
  no name. Every `Text` on the page is `Text.PlainText`.
- *From whose login*: when `_AUDIT_LOGINUID` is set and is not the
  account tried, as for su from a child's login. su and sudo keep it
  whoever they run as. It names a place, not a person: anyone at the
  keyboard could have typed.

**Grouping** (`TryNotices`). Tries for one account from one place and one
login, each within faillock's `fail_interval` of the last, are one
notice, with their count, first and last time. Tries are counted, not
kept, so a flood takes no more room than its count. A lock goes with the
try from the same process; the lock screen's lock goes with the tries in
that person's own login that ended just before it; a lock with neither,
such as one from wrong fingers, is a notice of its own. Tries in the
`unlock_time` after a lock, other than the one that locked, were refused
whatever was typed, the right password too, and are never called wrong:
"It was tried twice more while the account was locked." At most 200
notices show, the newest, with "N older notices are not shown."

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
  "about". With `unlock_time = never`, no try is called "while locked",
  because the journal does not say when the lock was reset.
- The page reads the journal each time it is shown, which took 0.2 s on
  cairn-min for the audit run; the lock screen run reads every user
  journal and is the slow one. The window never waits for either.

## Checks in the VM

On cairn-min, with the image built from this change:
1. `_AUDIT_LOGINUID` and `_UID` on polkit-agent-helper-1's records: if
   0 and unset, "from whose login" is never said for password boxes.
2. `su 'x y'` from a test child: the field `AUDIT_FIELD_ACCT` holds, and
   the page says "a login name that is not an account".
3. faillock's audit record has `AUDIT_FIELD_SUID` (journald's name for
   `suid=`); without it every audit lock is dropped and only the lock
   screen's show.
4. The lock screen's faillock line has `_EXE` and the account's own
   `_UID`, for `kde` and, where a reader exists, `kde-fingerprint`.
5. Three wrong tries at each of the login screen, the lock screen, sudo,
   Family's password box and `su - guardian` from a child's login give
   five notices with the right place, and "locked".
6. A `logger -p authpriv.notice` forgery of pam_unix's and faillock's
   lines from a child's login shows nothing.
7. A child's session can read neither the system journal nor another
   account's.
8. `audit=0` is not on the kernel command line and
   `systemd-journald-audit.socket` is active.
