// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "Account.h"

#include <QString>
#include <QStringList>
#include <QVariantList>

#include <optional>
#include <vector>

// One child as the Guardian's Children page shows them.
struct ListedChild {
    QString login;
    // The full name, or the login name when there is none, as on the login
    // screen.
    QString name;
    // The full name alone, empty when the account has none.
    QString fullName;
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

// Whether the two are one name to a family: the same but for the case of
// their letters and spaces at either end, as "Cy" and " cy ". An accent
// counts, so "Zoe" is not "Zoë".
bool sameName(const QString& one, const QString& other);

// The first child among them with this name, by sameName(), or nullptr.
// A child with the login `except` is passed over.
const ListedChild* childNamed(const std::vector<ListedChild>& children, const QString& name,
                              const QString& except = {});

// The names the family knows its grown-ups by: every administrator's
// account among them, a person's, by Account::displayName(), as the login
// screen shows them.
QStringList grownUpNamesAmong(const std::vector<Account>& accounts);

// The first of `names` that is `name` by sameName(), as it is written
// there, or nothing.
std::optional<QString> nameAmong(const QStringList& names, const QString& name);

// Their logins, in the same order.
QStringList loginsOf(const std::vector<ListedChild>& children);

// Each child as a map, login and name, in the same order, for a page's QML.
QVariantList loginsAndNames(const std::vector<ListedChild>& children);

// The same with their level, and otherLevels, the levels the Children page
// offers to move them to, for its QML.
QVariantList loginsNamesAndLevels(const std::vector<ListedChild>& children);

// Whether both lists hold the same children, by login and name, in order.
bool sameLoginsAndNames(const std::vector<ListedChild>& one, const std::vector<ListedChild>& other);

// The names of the children among `logins`, in that order, as one list in
// words, such as "Ada and Ben".
QString namesInWords(const std::vector<ListedChild>& children, const QStringList& logins);
