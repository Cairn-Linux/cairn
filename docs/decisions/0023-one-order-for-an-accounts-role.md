# ADR-0023: One order for an account's role

**Status:** accepted
**Date:** 2026-09-30

## Context

ADR-0011 gives every Cairn account exactly one of `cairn-l1` … `cairn-l4`
and `cairn-guardian`.
Four places read those groups and act on them: the greeter's PAM service
(ADR-0016), the session dispatcher (ADR-0012), the polkit rule (#35) and
the login screen's tiles (ADR-0020).
Each assumed exactly one group and checked in its own order.
An account in more than one disagreed with itself (#101).
A Guardian also in `cairn-l1` was let in by PAM with no password, sent to
the kiosk by the dispatcher and denied Wi-Fi by polkit, while the login
screen drew a Guardian tile that asked for a password.
Checked in the VM on 2026-09-30: through the `sddm` PAM service with an
empty password, a Guardian in `cairn-l1` and an administrator in
`cairn-l1` were both let in.

Nothing Cairn ships puts an account in two of the groups on purpose.
But provisioning could, and could also leave a child in none (#100).
`set_level_group` dropped the old level before it added the new one, so a
run cut off between the two left the child in no Cairn group.
The dispatcher reads that as an account Cairn did not make and gives it
Plasma.

An account can also reach no group honestly.
A parent adds one in System Settings › Users, for a grandparent or for a
second child, and it gets no Cairn group and no `wheel`.
The account cannot say which it is.
Failing it closed would lock a grandparent out; letting it through gives a
second child Plasma.

## Decision

Every place that reads the groups uses one order, and the first match
wins:

1. **Administrator**: `cairn-guardian` or `wheel`, whatever else the
   account is in.
   Always asked for a password, gets Plasma, and the polkit rule steps
   aside.
2. **L1 or L2**: the kiosk, no password at the greeter, and the kiosk's
   polkit denials.
3. **L3 or L4**: Plasma, a password, and the Guardian-only polkit
   denials.
4. **An account Cairn did not make**: Plasma, a password and Fedora's
   polkit defaults, as the machine gave it before Cairn arrived.

Within a rank, two levels mean the more restricted one, as before.
Provisioning adds an account's new level before it removes the old one,
so an interrupted change leaves two levels, which rank 2 reads as the more
restricted, and never none.
Provisioning refuses to make an existing child the Guardian, as it
already refuses to make an administrator a child.
Rank 4 stays open for now.
Giving an account Cairn did not make a level, and keeping parents from
making such accounts behind the Guardian tool's back, is the Guardian
tool's job (#113).

## Consequences

- A parent can never be locked into the kiosk or let in without a
  password by a stray group, and a child can never gain a desktop from
  one.
- `wheel` counts as an administrator even without `cairn-guardian`, so the
  installer's own account (Bazzite's `bazzite` user) keeps its desktop.
- The four places now say the same thing and each cites this ADR, so a
  fifth (the Guardian tool) has one rule to follow.
- An account made in System Settings still gets Plasma until #113 lands.
  That is a known gap, stated here and in #113, not a fallback we chose to
  keep.
- The checks are still text-level tests plus a VM run; behavioural PAM and
  polkit harnesses are #102.
