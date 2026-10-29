// SPDX-License-Identifier: Apache-2.0
#include "GamesRequest.h"

#include <QRegularExpression>
#include <QSet>

#include <algorithm>

namespace {

// The same rule as FamilyRequest's, provisioning's and the visit helper's.
bool isPlainLogin(const QString& login) {
    static const QRegularExpression plain(QStringLiteral("^[a-z_][a-z0-9_-]{0,31}$"));
    return plain.match(login).hasMatch();
}

// Steam's appids in the one form kidscan, the copy helper and the launcher
// accept: ASCII digits, no sign and no leading 0.
bool isAppId(const QString& text) {
    static const QRegularExpression appId(QStringLiteral("^[1-9][0-9]{0,9}$"));
    return appId.match(text).hasMatch();
}

// A child's list after the '=', split at each comma. Every item must be an
// appid, so "620," is refused, never read as 620.
std::optional<Refusal> checkList(const QStringList& items) {
    for (const QString& item : items) {
        if (!isAppId(item)) {
            return Refusal::BadAppId;
        }
    }
    if (items.size() > GamesRequest::mostGames ||
        QSet<QString>(items.begin(), items.end()).size() != items.size()) {
        return Refusal::Usage;
    }
    return std::nullopt;
}

// A child to log out comes before every list, once.
std::optional<Refusal> checkLogOut(const QString& login, const GamesRequest& request) {
    if (!isPlainLogin(login)) {
        return Refusal::BadName;
    }
    if (!request.children.isEmpty() || request.logOut.contains(login)) {
        return Refusal::Usage;
    }
    return std::nullopt;
}

GamesRequest refused(Refusal refusal) {
    GamesRequest request;
    request.refusal = refusal;
    return request;
}

} // namespace

GamesRequest GamesRequest::fromArguments(const QStringList& arguments) {
    static const QString logOutFlag = QStringLiteral("--log-out=");
    // Every child at most twice: once to log out, once for their list.
    if (arguments.isEmpty() || arguments.constFirst() != QStringLiteral("save-games") ||
        arguments.size() > (2 * mostChildren) + 1) {
        return refused(Refusal::Usage);
    }

    GamesRequest request;
    QSet<QString> logins;
    for (const QString& argument : arguments.mid(1)) {
        if (argument.startsWith(logOutFlag)) {
            const QString login = argument.mid(logOutFlag.size());
            if (const std::optional<Refusal> refusal = checkLogOut(login, request)) {
                return refused(*refusal);
            }
            request.logOut.append(login);
            continue;
        }
        const qsizetype equals = argument.indexOf(QLatin1Char('='));
        if (equals < 0) {
            return refused(Refusal::Usage);
        }
        Child child{.login = argument.left(equals), .appIds = {}};
        if (!isPlainLogin(child.login)) {
            return refused(Refusal::BadName);
        }
        if (logins.contains(child.login)) {
            return refused(Refusal::Usage);
        }
        logins.insert(child.login);

        const QString list = argument.mid(equals + 1);
        // An empty list is no games.
        const QStringList items = list.isEmpty() ? QStringList() : list.split(QLatin1Char(','));
        if (const std::optional<Refusal> refusal = checkList(items)) {
            return refused(*refusal);
        }
        child.appIds = items;
        request.children.append(child);
    }
    // A child is logged out only for a list this request gives them.
    if (request.children.size() > mostChildren ||
        !std::ranges::all_of(request.logOut,
                             [&logins](const QString& login) { return logins.contains(login); })) {
        return refused(Refusal::Usage);
    }
    return request;
}

QStringList GamesRequest::logins() const {
    QStringList names;
    for (const Child& child : children) {
        names.append(child.login);
    }
    return names;
}
