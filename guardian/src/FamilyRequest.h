// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "Refusal.h"

#include <QString>
#include <QStringList>

#include <cstdint>
#include <optional>

// One change cairn-family was asked for, read from its arguments:
//
//   create-child LOGIN LEVEL [--full-name NAME]
//   set-level LOGIN LEVEL
//   set-name LOGIN NAME
//   set-language LOGIN LOCALE
//
// Every value is checked here, before anything looks at an account, because
// each goes into a system tool's arguments or a file name.
struct FamilyRequest {
    enum class Action : std::uint8_t { CreateChild, SetLevel, SetName, SetLanguage };

    Action action = Action::CreateChild;
    QString login;
    // Not set for set-name or set-language, which leave the level as it is.
    int level = 0;
    // Always set for set-name.
    std::optional<QString> fullName;
    // Always set for set-language, and always one in ChildLanguage's table.
    QString locale;
    // Set when the arguments are not a request this command takes; then
    // nothing else here may be used.
    std::optional<Refusal> refusal;

    static FamilyRequest fromArguments(const QStringList& arguments);
};
