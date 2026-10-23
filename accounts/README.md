# accounts

Who each account is in the family, read from the system's account
database. One small static library, `cairn-accounts`, so every program
that asks uses one rule. The login screen's module (`../greeter/`) and the
Guardian's side (`../guardian/`) link it. Nothing of it is installed on its
own: it is compiled into each of them.

| File | What |
|---|---|
| `src/Account` | Looks an account up through NSS, as PAM does, by login name or by number, or lists every account. It gives the account's number, its groups, its login name and its full name, the first field of its comment. `displayName()` is the full name, or the login name when there is none, as on the login screen. `isCairnChild()` is the one rule for whose account `cairn-family` may change and the Guardian tool lists: a person's account, number 1000 or more, in a level group and no administrator. `nameInUse()` says whether an account or a group has a name, so the Guardian tool never makes a login that is taken. |
| `src/FamilyRole` | The role an account's groups give it, in ADR-0023's order. An administrator comes first, whatever else they are in: `cairn-guardian` is a Guardian, and `wheel` without it is an administrator who is not family, such as the installer's account. Then a child, with a level from 1 to 4. An account in two level groups is at the lower one, the more restricted (#100). Anyone else is not family. |

The same order is in PAM (`../session/sddm/pam.d/sddm`), polkit's levels
rule and the session dispatcher (`../session/`). If they ever disagreed,
PAM is the gate: the library only decides what a screen draws and what
`cairn-family` will change.

The library is position independent, because the login screen's module
is a shared library that SDDM loads as a plugin.

## Tests

CTest runs `familyrole`, the rule over a table of groups, with names
that only look like a level or a grown-up's group, and `account`: the
system's own lookup of `root` and of the account running the test, the
whole list, names in use, and who counts as a Cairn child.
