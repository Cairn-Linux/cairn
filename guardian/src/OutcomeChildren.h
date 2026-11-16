// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "ChildList.h"

#include <QString>

#include <optional>
#include <vector>

// The children the Children page's sentence for the last change is about,
// each as the list showed them when the sentence was written: the child
// whose row the change was made on, or the child Add a child made or tried
// to make, and a child the sentence names as having a name already.
//
// The sentence was true of them as they were then. Once a later read of
// the accounts finds one of them gone from the list, back on it, or there
// with another name, level or language, the page takes the sentence away
// (ADR-0040). While each is as they were it stays, whatever else changed
// in the family.
class OutcomeChildren {
public:
    // A new change starts, and its sentence is about nobody yet.
    void clear();

    // The sentence is about the account with this login, whether the list
    // has it or not.
    void add(const QString& login);

    // The sentence is written now, over the list as it was just read.
    void noteAsListed(const std::vector<ListedChild>& children);

    // Whether one of them is no longer as noted, in the list as read now.
    bool changedIn(const std::vector<ListedChild>& children) const;

private:
    struct Child {
        QString login;
        // As the list showed them when the sentence was written, or
        // nothing when they were not on it.
        std::optional<ListedChild> asListed;
    };

    std::vector<Child> m_children;
};
