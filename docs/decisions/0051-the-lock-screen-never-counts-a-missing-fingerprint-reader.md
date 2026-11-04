# ADR-0051: The lock screen never counts a missing fingerprint reader

**Status:** proposed. It becomes accepted when the checks under "Checks
in the VM" have passed on cairn-min, on a fresh install, and on the kids'
laptop after its next image update.
**Date:** 2026-11-04
**Closes:** (none) — issue #283. Amends ADR-0038 (the `/etc` files the
image owns).

## Context

On 2026-11-04, the day the kids' laptop moved to the image, the Guardian
was locked out of the login screen for ten minutes with the right
password.
The laptop has no fingerprint reader.

Plasma's lock screen tries two PAM services each time it wakes: `kde`
for the password and `kde-fingerprint` for a finger.
Fedora's `kde-fingerprint`, from plasma-workspace, runs authselect's
`fingerprint-auth` as a substack.
With `with-fingerprint` and `with-faillock`, that file's auth lines are:

```
auth required pam_faillock.so preauth silent
auth [success=done default=bad] pam_fprintd.so
auth required pam_faillock.so authfail
auth required pam_deny.so
```

With no reader, or no finger enrolled, `pam_fprintd` answers
`PAM_AUTHINFO_UNAVAIL` at once.
`default=bad` keeps that as the result but lets the stack go on, so
`pam_faillock authfail` writes a wrong try.
Every wake of the lock screen spends one of the Guardian's three tries
(ADR-0020), before anything is typed.
Two wakes and one typo lock the Guardian out of the login screen, sudo
and the Family window's boxes, and the login screen says only "That
password did not work".

On cairn-min the same happened: one `kde-fingerprint` record at each
wake, three wakes and the Guardian's right password was refused by the
`kde` service.
A right password does not clear the count; records only age out after
`fail_interval`.

A fresh install of the image does not have the bug.
The image's `fingerprint-auth` is authselect's stub without
`with-fingerprint`, a single `pam_debug` line.
The laptop and cairn-min have it because bootc kept their `/etc/authselect`,
changed on the computer: Bazzite's `with-fingerprint`, plus the
`with-faillock` provisioning added (`provision/cairn-provision.sh`, step 9).
Every Bazzite computer set up through `docs/TRY.md` has the same pair.
#264's plan, turning `with-fingerprint` on in the image, would give it to
every Cairn computer.

## Decision

The image ships its own `/etc/pam.d/kde-fingerprint`
(`session/pam.d/kde-fingerprint`).
Its auth lines are authselect's, written out in place of the substack,
with one changed line:

```
auth [success=done authinfo_unavail=die default=bad] pam_fprintd.so max-tries=1
```

- `authinfo_unavail=die` ends the stack on "no reader, no finger or no
  answer", before `authfail`. The lock screen gets the answer it already
  reads as "fingerprint unavailable", and nothing is counted.
- `max-tries=1` makes each wrong finger its own try. Without it,
  someone could offer two wrong fingers and let the 30-second timeout
  end the prompt, which answers `PAM_AUTHINFO_UNAVAIL` and would now
  count nothing. With it, a wrong finger answers `PAM_MAXTRIES` at once
  and is counted, so three wrong fingers lock the account as three wrong
  passwords do.
- Nothing maps the answer to success: only `PAM_SUCCESS` reaches
  `success=done`. A locked account with a right finger is still refused,
  because `preauth` comes first.
- Two auth lines are left out. Fedora's `auth include postlogin` ran
  after the substack; here `success=done` ends the whole stack, and
  postlogin's only auth line is `pam_ecryptfs`, under `with-ecryptfs`,
  which Cairn does not turn on. authselect's optional `pam_gnome_keyring`
  line, under `with-pam-gnome-keyring`, is off too.
- The account, password and session lines are Fedora's, byte for byte,
  and keep following authselect.

The file sits beside the image's `sddm` and `login` in `/etc/pam.d`.
plasma-workspace marks its copy `%config(noreplace)`, so a later package
writes `.rpmnew` and leaves Cairn's in place.
authselect manages only its own files and never touches this one.
bootc's three-way merge delivers it to a moved computer whose
`/etc/pam.d/kde-fingerprint` is unchanged from the image, as on the
laptop and cairn-min.
Provisioning installs the same file, for Bazzite computers set up through
`docs/TRY.md`.

`build_files/check-lock-screen-pam.py` runs at image build and fails it
if:
1. plasma-workspace ships a different `kde-fingerprint`;
2. authselect's `fingerprint-auth` template has a new `pam_fprintd` line;
3. any PAM service runs `fingerprint-auth`'s auth lines;
4. Fedora's line, put back, no longer writes a record (so the check can
   see one);
5. Cairn's line, with no reader, gives anything but
   `PAM_AUTHINFO_UNAVAIL` with no record.

It uses `pam_start_confdir` on a copy of `/etc/pam.d` in `/tmp`, so the
image does not change.
`session/tests/test_pam_kde_fingerprint.py` pins the whole auth block.

Set aside:
- **A Cairn authselect profile.** It does not reach a moved computer
  whose `/etc/authselect` was changed, and Cairn would own a profile.
- **Editing `/etc/authselect/fingerprint-auth`.** authselect's
  `apply-changes --upgrade` undoes it at a fresh install's first boot.
- **Lock-screen settings.** kscreenlocker 6.7 and 6.8 have no switch for
  the fingerprint, and deleting the file turns off fingerprint unlock,
  against ADR-0032.
- **Masking fprintd.** `pam_fprintd` answers the same without it.
- **Not counting fingerprints at all, as Arch's GDM does.** A wrong
  finger would then never count, against ADR-0020.

## Consequences

- A Guardian on a computer without a reader keeps their three tries for
  their own typing.
- The lock screen no longer follows `with-fingerprint`: on any computer
  with a reader and an enrolled finger it offers the fingerprint, ahead
  of #264. ADR-0032 already expects the lock screen to accept one.
- Each lock-screen wake now starts fprintd on a fresh install too, where
  the stub answered at once.
- #283 no longer blocks #264, but #264 is not free of this question.
  With `with-fingerprint`, `system-auth` has `auth sufficient
  pam_fprintd.so`, and `sufficient` never counts a wrong finger, at sudo
  or a polkit box. #264's own decision must say how wrong fingers count
  there.
- Cairn keeps a second copy of upstream's auth lines. The build check
  stops the build when either upstream file changes, so the copy gets a
  look.
- A Bazzite computer provisioned with this file and later moved to the
  image keeps its copy as changed on the computer, so later changes to
  it do not arrive. `provision/README.md` step 6 lists it among the files
  to compare.
- Retire this ADR if authselect adds `authinfo_unavail=die` to its
  profile: delete Cairn's file and let check 2 show the new line.
  Report to authselect, with a note to KDE that kscreenlocker could ask
  fprintd for a device before it starts the conversation.
- Not changed here: a right password at the lock screen does not clear
  the count (KDE bug 481808 is near it), and the login screen's words
  for a lock are #284.

## Checks in the VM

On cairn-min after an image update with this file, then on a fresh install
from an ISO, then on the kids' laptop:
1. `sudo ostree admin config-diff | grep kde-fingerprint` lists nothing,
   and `cmp /usr/etc/pam.d/kde-fingerprint /etc/pam.d/kde-fingerprint`
   passes. On cairn-min, also before the update, with `ls -Z` on both.
2. Three lock, wake and unlock cycles leave no `kde-fingerprint` record
   in `faillock --user <guardian>`.
3. A wrong password at the lock screen adds one `kde` record.
4. With the Guardian locked out, waking the lock screen adds no record,
   and `fprintd` and `kscreenlocker_greet` do not restart over and over.
5. Checks 2 and 4 again once the base moves to kscreenlocker 6.8, whose
   fingerprint worker retries a slow "unavailable".

A real reader (a wrong finger, a timeout) cannot be checked until Cairn
has one; ADR-0032 leaves the same open.
