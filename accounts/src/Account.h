// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QString>
#include <QStringList>

#include <optional>

// What Cairn needs to know about one account from the system's account
// database: when it was made, which groups it is in, and what the family
// calls it.
struct Account {
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
};

// Looks an account up by its login name through NSS, as PAM does. Nothing if
// there is no such account.
std::optional<Account> lookUpAccount(const QString& name);

// The same, by the account's number.
std::optional<Account> lookUpAccountByUid(unsigned int uid);
