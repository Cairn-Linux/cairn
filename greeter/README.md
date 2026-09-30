# greeter

The login screen (ADR-0020, P0-11): "Who's playing today?" and a tile for
everyone in the family. SDDM is the display manager (ADR-0016); this is its
theme and the small C++ module the theme imports.

- `theme/`: the SDDM theme, QML only. `Main.qml` draws the tiles from the
  brand tokens; `FamilyTile.qml` is one round tile, `PasswordRow.qml` the
  password field under a tile that needs one, `TextButton.qml` its two
  buttons. `mark-on-ink.svg` links to the brand's mark.
- `src/`: `Cairn.Greeter`, the module. `AccountKind` says what kind of
  family member an account is from its groups (ADR-0011) and whether its
  tile asks for a password. `FamilyModel` sits over SDDM's user model: it
  sorts children first, then Guardians, then anyone outside the family, and
  adds the name, first letter and colour a tile draws. `Account` looks an
  account up through NSS, as PAM does.
- `tests/`: `tst_accountkind` and `tst_familymodel` for the C++;
  `tst_loginscreen` loads the real `theme/Main.qml` with a stand-in for
  SDDM's `sddm`, `userModel` and `sessionModel` and drives it by keyboard
  and mouse.
- `i18n/greeter_en.ts`: the theme's strings (ADR-0007), from the
  `greeter_lupdate` target.

The theme only decides what to draw. PAM decides who gets in: the greeter's
own PAM service lets `cairn-l1` and `cairn-l2` in with no password and asks
everyone else for one (`../session/sddm/pam.d/sddm`). `AccountKind` repeats
that rule so the screen shows a password field exactly where PAM will want
one.

## Installing

`../provision/cairn-provision.sh` installs the theme to
`/usr/local/share/cairn/sddm/themes/cairn/`, and the `Cairn.Brand` and
`Cairn.Greeter` modules to `/usr/local/lib64/cairn/qml/`, which
`../session/sddm/sddm.conf.d/10-cairn.conf` puts on the greeter's import
path. The module comes from the release build.

## Not here yet

A picture a child chooses for their tile (ADR-0020 leaves it for later), a
power-off control for a grown-up at the login screen, and a translation
beyond English.
