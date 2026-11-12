// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "ChildList.h"
#include "ChildrenPage.h"

#include <QString>
#include <QStringList>

#include <optional>

// The Children page's reasons for refusing a change before any password,
// and its reading of cairn-family's and pkexec's answers (ADR-0039).
// Each takes what the page last read and decides nothing else.
namespace ChildrenRefusals {

// A name the page refused because someone has it already, and their name
// as the family knows it.
struct Taken {
    ChildrenPage::Reason reason = ChildrenPage::Reason::None;
    QString clash;
};

// Whether `name` is taken: by `other`, a child found by the caller, with
// `childs` as the reason, or else by one of `grownUps` (#212, #292). The
// page reads the accounts just before it asks, so only a name given while
// the password is asked for goes through, as in Add a child.
std::optional<Taken> takenBy(const ListedChild* other, ChildrenPage::Reason childs,
                             const QStringList& grownUps, const QString& name);

// Why `newName`, already trimmed, cannot be `child`'s new name, leaving
// out a name someone has (see takenBy()), or nothing. `child` is nullptr
// when the page does not list them.
std::optional<ChildrenPage::Reason> nameRefusal(const ListedChild* child, const QString& newName);

// Why `locale` cannot be `child`'s new language, or nothing: the language
// chosen for them already. A language Family does not offer is left to
// cairn-family's own rule (ADR-0058).
std::optional<ChildrenPage::Reason> languageRefusal(const ListedChild* child,
                                                    const QString& locale);

// Why nothing changed, from a run's last word: one of cairn-family's
// refusal words or pkexec's own; Unknown for any other.
ChildrenPage::Reason reasonFor(const QString& answer);

} // namespace ChildrenRefusals
