// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "Refusal.h"

#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

// The games a Guardian saved for each child, read from cairn-family's
// arguments (ADR-0041):
//
//   save-games [LOGIN=APPID,APPID...]...
//
// Each argument is one child's whole new list, never a change to it, so
// `ben=` means ben has no games. A child not named is left alone, so
// `save-games` alone changes no child's list and only looks for new games. Every
// value is checked here, before anything looks at an account, because each
// goes into a file name or the copy helper's arguments.
struct GamesRequest {
    // The most children and games one Save may name: far more than any
    // family has, and few enough that a mistake cannot run for hours.
    static constexpr qsizetype mostChildren = 32;
    static constexpr qsizetype mostGames = 1000;

    struct Child {
        QString login;
        // In the order given, each once.
        QStringList appIds;
    };

    // In the order given, each once.
    QList<Child> children;
    // Set when the arguments are not a request this command takes; then
    // nothing else here may be used.
    std::optional<Refusal> refusal;

    static GamesRequest fromArguments(const QStringList& arguments);

    // The login names, in order, for the journal.
    QStringList logins() const;
};
