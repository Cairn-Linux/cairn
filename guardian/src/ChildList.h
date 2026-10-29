// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "Account.h"

#include <QString>
#include <QStringList>
#include <QVariantList>

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

// The child with this login among them, or nullptr.
const ListedChild* childWithLogin(const std::vector<ListedChild>& children, const QString& login);

// Their logins, in the same order.
QStringList loginsOf(const std::vector<ListedChild>& children);

// Each child as a map, login and name, in the same order, for a page's QML.
QVariantList loginsAndNames(const std::vector<ListedChild>& children);

// Whether both lists hold the same children, by login and name, in order.
bool sameLoginsAndNames(const std::vector<ListedChild>& one, const std::vector<ListedChild>& other);

// The names of the children among `logins`, in that order, as one list in
// words, such as "Ada and Ben".
QString namesInWords(const std::vector<ListedChild>& children, const QStringList& logins);
