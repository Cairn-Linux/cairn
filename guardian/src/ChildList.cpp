// SPDX-License-Identifier: Apache-2.0
#include "ChildList.h"

#include "FamilyRole.h"

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
