// SPDX-License-Identifier: Apache-2.0
#include "ChildList.h"

#include "FamilyRole.h"

#include <QLocale>
#include <QVariantMap>

#include <algorithm>

std::vector<ListedChild> childrenAmong(std::vector<Account> accounts) {
    std::erase_if(accounts, [](const Account& account) { return !account.isCairnChild(); });
    std::ranges::stable_sort(accounts, {}, &Account::uid);

    std::vector<ListedChild> children;
    children.reserve(accounts.size());
    for (const Account& account : accounts) {
        children.push_back({.login = account.login,
                            .name = account.displayName(),
                            .level = FamilyRole::fromGroups(account.groups).level.value_or(0)});
    }
    return children;
}

QStringList loginsOf(const std::vector<ListedChild>& children) {
    QStringList logins;
    for (const ListedChild& child : children) {
        logins.append(child.login);
    }
    return logins;
}

QVariantList loginsAndNames(const std::vector<ListedChild>& children) {
    QVariantList maps;
    for (const ListedChild& child : children) {
        maps.append(QVariantMap{
            {QStringLiteral("login"), child.login},
            {QStringLiteral("name"), child.name},
        });
    }
    return maps;
}

bool sameLoginsAndNames(const std::vector<ListedChild>& one,
                        const std::vector<ListedChild>& other) {
    return std::ranges::equal(one, other, [](const ListedChild& first, const ListedChild& second) {
        return first.login == second.login && first.name == second.name;
    });
}

const ListedChild* childWithLogin(const std::vector<ListedChild>& children, const QString& login) {
    const auto found = std::ranges::find(children, login, &ListedChild::login);
    return found == children.end() ? nullptr : &*found;
}

QString namesInWords(const std::vector<ListedChild>& children, const QStringList& logins) {
    QStringList names;
    for (const ListedChild& child : children) {
        if (logins.contains(child.login)) {
            names.append(child.name);
        }
    }
    return QLocale().createSeparatedList(names);
}
