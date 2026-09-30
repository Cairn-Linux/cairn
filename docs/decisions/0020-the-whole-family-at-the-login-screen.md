# ADR-0020: The whole family at the login screen

**Status:** accepted
**Date:** 2026-09-30
**Closes:** (none) — addresses issues #81 and #82; amends ADR-0016

## Context

[DESIGN §4.3](../DESIGN.md#43-sessions-and-accounts) asked the login screen
for large avatar tiles, no password for L1 and L2, and Guardians not shown
beside the children. ADR-0016 met the last with SDDM's `HideUsers`, kept the
stock Breeze theme for Phase 0 and left the Cairn theme to Phase 1 (P1-16).
A Guardian reached the password prompt through Breeze's "Other" button.

The first pilot with a child, on 2026-09-25 (#8), found two problems. The
login screen looked nothing like the launcher, so the system did not feel
like one place (#81). And nothing told a child which profile was theirs
(#82): Breeze shows the same grey outline for every account.

On 2026-09-30 the maintainer decided three more things. Cairn is often the
family computer, so a parent must be able to use it as easily as a child,
not through a hidden control. A child account is optional at first boot; the
Guardian is the one account the machine needs. And pictures a child chooses
as an avatar are an idea for later, not now.

A visible Guardian tile invites a child to guess the password: a Guardian
account is the one that can raise a level. Many parents will not know how to
protect it. Fedora's `authselect` already has the two pieces needed:
`with-faillock`, which locks an account for a while after wrong guesses, and
`with-pam-u2f-2fa`, which asks for a security key after the password for any
account that has one registered. `pam-u2f` is in the Bazzite image.

The brand guide's "In use" page sketches the login screen: the mark, "Who's
playing today?" and a round tile per child in a brand colour with the first
letter of the name. It is a first draft, not a specification.

## Decision

The login screen is the Cairn SDDM theme, built now, in Phase 0, so the next
child test sees it; P1-16 becomes P0-11. It follows the brand guide's "Who's
playing today?" sketch, with every size and colour from `Cairn.Brand.Tokens`.

Everyone in the family has a tile in the same row: each child and each
Guardian, children first, each group in the order the accounts were made.
Nobody is hidden, so `HideUsers` goes. An account outside the family (in no
level group and not in `cairn-guardian`) gets a tile too, after the
Guardians, and asks for a password: a hidden account with a password is how
a machine gets a way in that nobody in the family knows about, and showing
it lets a parent notice it. A tile is a circle in a brand
colour with the first letter of the account's name, and the name beneath;
chosen pictures are for later and need their own ADR. Choosing an L1 or L2
child's tile logs them in with no password. Choosing any other tile, a
Guardian's or an older child's, asks for the password on the same screen.
Which kind a tile is comes from the account's groups (ADR-0011), read by a
small C++ QML module the theme imports, never from a list kept by hand. PAM
stays the real gate (ADR-0016); the theme only decides whether to show a
password field.

The Guardian account is protected with upstream pieces. Provisioning now, and
the image later, enable `authselect`'s `with-faillock`: a few wrong passwords
lock the account for a few minutes, for the greeter, `su`, `sudo` and the lock
screen alike. The count of wrong passwords is kept in `/var/lib/faillock`,
not Fedora's default `/var/run/faillock`, which is cleared at every boot, so
holding the power button does not reset it. A security key through `with-pam-u2f-2fa` is an option a
Guardian can turn on, offered in plain words by the first-boot wizard and the
Guardian tool, never required. The parent-facing text says plainly that a
password a child has watched being typed is the weak point.

At first boot the wizard requires the Guardian account and nothing else; a
child account can be made there or later from the Guardian tool.

## Consequences

- The login screen and the launcher share the brand tokens, which answers
  #81. The initial and colour tell tiles apart for a child who knows their
  letter; a child who cannot read yet may still need the colour, or later a
  picture, which is what #82 stays open for.
- A parent sees their own tile and never needs to know about a hidden
  control. The cost is that a child sees it too. The lockout slows guessing;
  it does not stop a child who has watched the password being typed. Only
  the security key does that, and it costs the family money and a thing to
  keep.
- The lockout applies to every Guardian login path, so a parent who mistypes
  their password a few times waits too. The parent-facing text says so.
- The Bazzite VM's installer account, `bazzite` (shown as "cairn"), keeps a
  tile. A Cairn image ships no accounts and no running `sshd`; its first
  account is the Guardian the first-boot wizard makes (P1-4).
- The login screen is only the front of the Guardian boundary. PAM, the
  consoles, the boot menu and firmware, and the levels above L2 are audited
  as their own tasks, once early in development (#85) and once before the
  first public image (#86).
- The theme needs a C++ QML module on the greeter's import path, so SDDM gets
  a `GreeterEnvironment` line. The brand tokens module goes there too.
- The level is still invisible from a child's session (DESIGN §3.2): the
  login screen is not a child session, and it shows who is in the family,
  not anyone's level.
- DESIGN §4.3 and §4.3.1 change in the same commit; ADR-0016's display
  manager decision stands, its `HideUsers` and "Other" button do not.
