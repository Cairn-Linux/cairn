// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "Account.h"

#include <QString>
#include <QStringList>

#include <vector>

// One child as the Guardian's Children page shows them.
struct ListedChild {
    QString login;
    // The full name, or the login name when there is none, as on the login
    // screen.
    QString name;
    // 1 to 4; the lower one for an account in two level groups (#100).
    int level = 0;
};

// The family's children among these accounts, in the order their accounts
// were made, as the login screen shows them (ADR-0020). Only accounts
// cairn-family would change count: administrators, system accounts and
// accounts in no level group are left out.
std::vector<ListedChild> childrenAmong(std::vector<Account> accounts);

// Their logins, in the same order.
QStringList loginsOf(const std::vector<ListedChild>& children);
