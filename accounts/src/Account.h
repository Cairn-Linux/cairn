// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QString>
#include <QStringList>

#include <optional>
#include <vector>

// What Cairn needs to know about one account from the system's account
// database: when it was made, which groups it is in, and what the family
// calls it.
struct Account {
    // The first number useradd gives a person, from Fedora's login.defs.
    // Anything lower is a system account.
    static constexpr unsigned int firstPersonUid = 1000;

    // Accounts are numbered as they are made, so this orders them by age.
    unsigned int uid = 0;
    QStringList groups;
    QString login;
    // The first field of the account's comment, which `useradd --comment`
    // writes. Empty when the account has none.
    QString fullName;

    // The name the family knows the account by: its full name, or its login
    // name when it has none, as on the login screen.
    QString displayName() const;

    // A child's account that Cairn made: a person's account in a level
    // group, and no administrator (ADR-0039). The one rule for which
    // accounts cairn-family may change and the Guardian's window lists.
    bool isCairnChild() const;
};

// Looks an account up by its login name through NSS, as PAM does. Nothing if
// there is no such account.
std::optional<Account> lookUpAccount(const QString& name);

// The same, by the account's number.
std::optional<Account> lookUpAccountByUid(unsigned int uid);

// Every account the system's account database knows, through NSS, in the
// order it gives them.
std::vector<Account> listAccounts();

// Whether an account or a group already has this name. useradd makes a group
// of the same name for each new account, so either one blocks a new login.
bool nameInUse(const QString& name);
