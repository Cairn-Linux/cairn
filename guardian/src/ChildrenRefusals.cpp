// SPDX-License-Identifier: Apache-2.0
#include "ChildrenRefusals.h"

#include "ChildLanguage.h"

#include <QHash>

using Reason = ChildrenPage::Reason;

std::optional<ChildrenRefusals::Taken> ChildrenRefusals::takenBy(const ListedChild* other,
                                                                 Reason childs,
                                                                 const QStringList& grownUps,
                                                                 const QString& name) {
    if (other != nullptr) {
        return Taken{childs, other->name};
    }
    if (const std::optional<QString> grownUp = nameAmong(grownUps, name)) {
        return Taken{Reason::AGrownUpsName, *grownUp};
    }
    return std::nullopt;
}

// The same name in other capitals is a change, and so is a child's login
// given as the full name they never had.
std::optional<Reason> ChildrenRefusals::nameRefusal(const ListedChild* child,
                                                    const QString& newName) {
    if (child == nullptr) {
        return Reason::NotAChild;
    }
    if (newName.isEmpty()) {
        return Reason::NoName;
    }
    if (newName == child->fullName) {
        return Reason::NameAsBefore;
    }
    return std::nullopt;
}

std::optional<Reason> ChildrenRefusals::languageRefusal(const ListedChild* child,
                                                        const QString& locale) {
    if (child == nullptr) {
        return Reason::NotAChild;
    }
    if (ChildLanguage::find(locale) != nullptr && locale == child->language) {
        return Reason::LanguageAsBefore;
    }
    return std::nullopt;
}

Reason ChildrenRefusals::reasonFor(const QString& answer) {
    static const QHash<QString, Reason> reasons = {
        {QStringLiteral("not-root"), Reason::NotRoot},
        {QStringLiteral("not-a-grown-up"), Reason::NotAGrownUp},
        {QStringLiteral("usage"), Reason::Usage},
        {QStringLiteral("bad-name"), Reason::BadName},
        {QStringLiteral("bad-full-name"), Reason::BadFullName},
        {QStringLiteral("bad-level"), Reason::BadLevel},
        {QStringLiteral("bad-language"), Reason::BadLanguage},
        {QStringLiteral("taken"), Reason::Taken},
        {QStringLiteral("not-a-child"), Reason::NotAChild},
        {QStringLiteral("failed"), Reason::Failed},
        {QStringLiteral("busy"), Reason::Busy},
        {QStringLiteral("dismissed"), Reason::Dismissed},
        {QStringLiteral("not-authorized"), Reason::NotAuthorized},
    };
    return reasons.value(answer, Reason::Unknown);
}
