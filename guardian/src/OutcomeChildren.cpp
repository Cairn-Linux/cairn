// SPDX-License-Identifier: Apache-2.0
#include "OutcomeChildren.h"

#include <algorithm>

void OutcomeChildren::clear() {
    m_children.clear();
}

void OutcomeChildren::add(const QString& login) {
    m_children.push_back({.login = login, .asListed = std::nullopt});
}

void OutcomeChildren::noteAsListed(const std::vector<ListedChild>& children) {
    for (Child& child : m_children) {
        const ListedChild* listed = childWithLogin(children, child.login);
        child.asListed = listed == nullptr ? std::nullopt : std::optional<ListedChild>(*listed);
    }
}

// A child who was not on the list and still is not has not changed: the
// sentence that says they are no child's account here is still true.
bool OutcomeChildren::changedIn(const std::vector<ListedChild>& children) const {
    return std::ranges::any_of(m_children, [&children](const Child& child) {
        const ListedChild* now = childWithLogin(children, child.login);
        if (now == nullptr || !child.asListed) {
            return (now != nullptr) != child.asListed.has_value();
        }
        return !sameOnTheList(*now, *child.asListed);
    });
}
