// SPDX-License-Identifier: Apache-2.0
#include "GamesRequest.h"

#include <QRegularExpression>
#include <QSet>

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

GamesRequest refused(Refusal refusal) {
    GamesRequest request;
    request.refusal = refusal;
    return request;
}

} // namespace

GamesRequest GamesRequest::fromArguments(const QStringList& arguments) {
    if (arguments.isEmpty() || arguments.constFirst() != QStringLiteral("save-games") ||
        arguments.size() > mostChildren + 1) {
        return refused(Refusal::Usage);
    }

    GamesRequest request;
    QSet<QString> logins;
    for (const QString& argument : arguments.mid(1)) {
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
        // An empty list is no games. Otherwise every item must be an appid,
        // so "620," is refused, never read as 620.
        const QStringList items = list.isEmpty() ? QStringList() : list.split(QLatin1Char(','));
        for (const QString& item : items) {
            if (!isAppId(item)) {
                return refused(Refusal::BadAppId);
            }
        }
        if (items.size() > mostGames ||
            QSet<QString>(items.begin(), items.end()).size() != items.size()) {
            return refused(Refusal::Usage);
        }
        child.appIds = items;
        request.children.append(child);
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
