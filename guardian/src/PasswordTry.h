// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "Account.h"

#include <QString>
#include <QtTypes>

#include <cstdint>
#include <optional>

// Where a password was typed, as far as the journal can tell from the
// program that asked for it (ADR-0054).
enum class TryPlace : std::uint8_t {
    LoginScreen, // sddm-helper, or plasmalogin-helper after a rebase
    // unix_chkpwd, which checks only its caller's own password: the lock
    // screen, or any other program run in that account's own login.
    OwnLogin,
    Sudo,
    Su,
    PasswordBox, // polkit's helper, behind a password box such as Family's
    Console,     // login, on a text console
    Other,       // any other program; its name goes with it
};

// One wrong password, or one try refused while the account was locked, from
// an audit record the kernel took from a program allowed to write one.
struct PasswordTry {
    // Microseconds since 1970, as the journal keeps time.
    qint64 time = 0;
    // The account whose password was tried. Nothing when the name typed is
    // no account on this computer: that name is never shown.
    std::optional<Account> account;
    TryPlace place = TryPlace::Other;
    // For Other: the program's file name, or empty when it is not a plain
    // name worth showing.
    QString program;
    // The login it was typed in, when that is not the account tried, as for
    // su from a child's login. An account the computer no longer knows has
    // only its number.
    std::optional<Account> from;
    // The process that asked, to tell the try that locked an account from
    // tries made while it was locked.
    qint64 pid = 0;
};

// A password that was right, from an audit record: USER_AUTH when it was
// checked, then USER_ACCT when the account part ran. faillock's account
// part empties the count, but only after its own password check in that
// process (pam_faillock.c, pam_sm_acct_mgmt), so it takes both, from one
// process. The lock screen never runs the account part, and empties
// nothing (ADR-0053).
struct GoodPassword {
    qint64 time = 0;
    Account account;
    qint64 pid = 0;
    // USER_ACCT, the account part, rather than USER_AUTH.
    bool accountPart = false;
};

// faillock locking an account, from its audit record or, for the lock
// screen, which cannot write one, its line in the journal.
struct LockEvent {
    qint64 time = 0;
    Account account;
    TryPlace place = TryPlace::Other;
    QString program;
    qint64 pid = 0;
};
