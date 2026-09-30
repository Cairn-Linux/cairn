// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QString>
#include <QStringList>

#include <optional>

// What the login screen needs to know about one account from the system's
// account database: when it was made, and which groups it is in.
struct Account {
    // Accounts are numbered as they are made, so this orders them by age.
    unsigned int uid = 0;
    QStringList groups;
};

// Looks an account up by its login name through NSS, as PAM does. Nothing if
// there is no such account.
std::optional<Account> lookUpAccount(const QString& name);
