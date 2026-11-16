// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "ChildList.h"

#include <QString>

#include <functional>
#include <optional>
#include <vector>

// The Children page's memory of an add that may have made the account
// before it stopped, so that adding that name again finishes it instead of
// making a second account with the same name (ADR-0039). It lasts only
// while the window is open.
class UnfinishedAdd {
public:
    // The add that is about to run.
    void starting(const QString& login, const QString& fullName);

    // That add's answer. A failure, or an answer the window could not read
    // (`readable` false), may have made the account, so the add is
    // remembered. A refusal or a closed password box changed nothing.
    void answered(const QString& answer, bool readable);

    // The login an add of `fullName` must use to finish the remembered one:
    // only when the name is the same, whatever the case of its letters, and
    // that login is still free or still that very child.
    std::optional<QString> loginFor(const QString& fullName,
                                    const std::vector<ListedChild>& children,
                                    const std::function<bool(const QString&)>& nameInUse) const;

private:
    struct Add {
        QString login;
        QString fullName;
    };

    Add m_adding;
    std::optional<Add> m_unfinished;
};
