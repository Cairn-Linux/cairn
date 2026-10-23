// SPDX-License-Identifier: Apache-2.0
#include "FamilyRequest.h"

#include "FamilyRole.h"

#include <QRegularExpression>

#include <algorithm>

namespace {

// The same rule as provisioning's and the visit helper's: a plain login
// name, which can go into a file name and a menu entry as it is.
bool isPlainLogin(const QString& login) {
    static const QRegularExpression plain(QStringLiteral("^[a-z_][a-z0-9_-]{0,31}$"));
    return plain.match(login).hasMatch();
}

// Only the levels the kiosk serves: L3 and L4 have no child session yet.
std::optional<int> levelFrom(const QString& text) {
    for (int level = FamilyRole::lowestLevel; level <= FamilyRole::highestKioskLevel; ++level) {
        if (text == QString::number(level)) {
            return level;
        }
    }
    return std::nullopt;
}

// A colon or a newline would break the account database's line, and a comma
// would start its next field. A menu entry reads a backslash as an escape.
bool isPlainFullName(const QString& name) {
    const auto plain = [](QChar character) {
        return character.isPrint() && character != QLatin1Char(':') &&
               character != QLatin1Char(',') && character != QLatin1Char('\\');
    };
    return !name.trimmed().isEmpty() && name.trimmed() == name && std::ranges::all_of(name, plain);
}

// A subcommand this command has, with the arguments it takes.
bool followsTheUsage(const QStringList& arguments) {
    if (arguments.size() == 3) {
        return arguments.constFirst() == QStringLiteral("create-child") ||
               arguments.constFirst() == QStringLiteral("set-level");
    }
    return arguments.size() == 5 && arguments.constFirst() == QStringLiteral("create-child") &&
           arguments.at(3) == QStringLiteral("--full-name");
}

FamilyRequest refused(Refusal refusal) {
    FamilyRequest request;
    request.refusal = refusal;
    return request;
}

} // namespace

FamilyRequest FamilyRequest::fromArguments(const QStringList& arguments) {
    if (!followsTheUsage(arguments)) {
        return refused(Refusal::Usage);
    }

    FamilyRequest request;
    request.action = arguments.constFirst() == QStringLiteral("set-level") ? Action::SetLevel
                                                                           : Action::CreateChild;
    request.login = arguments.at(1);
    if (!isPlainLogin(request.login)) {
        return refused(Refusal::BadName);
    }
    const std::optional<int> level = levelFrom(arguments.at(2));
    if (!level) {
        return refused(Refusal::BadLevel);
    }
    request.level = *level;
    if (arguments.size() == 5) {
        if (!isPlainFullName(arguments.at(4))) {
            return refused(Refusal::BadFullName);
        }
        request.fullName = arguments.at(4);
    }
    return request;
}
