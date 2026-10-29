// SPDX-License-Identifier: Apache-2.0
#include "SaveResults.h"

#include "GamesSaving.h"

#include <QVariantList>

#include <algorithm>

QHash<QString, QString> SaveResults::titlesOf(const QList<GameShelf::Game>& games) {
    QHash<QString, QString> titles;
    for (const GameShelf::Game& game : games) {
        titles.insert(game.appId, game.title);
    }
    return titles;
}

QVariantMap SaveResults::forChild(const SaveReport::Child& child, const QString& name,
                                  const QHash<QString, QString>& titles) {
    QVariantList skipped;
    for (const SaveReport::Skip& skip : child.skipped) {
        skipped.append(QVariantMap{
            {QStringLiteral("appId"), skip.appId},
            {QStringLiteral("title"), titles.value(skip.appId)},
            {QStringLiteral("reason"), static_cast<int>(GamesSaving::skipFor(skip.reason))},
        });
    }
    return QVariantMap{
        {QStringLiteral("login"), child.login},
        {QStringLiteral("name"), name},
        {QStringLiteral("added"), child.added.value_or(-1)},
        {QStringLiteral("removed"), child.removed.value_or(-1)},
        {QStringLiteral("held"),
         static_cast<int>(GamesSaving::heldFor(child.held.value_or(QString())))},
        {QStringLiteral("later"), child.later},
        {QStringLiteral("skipped"), skipped},
    };
}

bool SaveResults::allGiven(const QList<SaveReport::Child>& children) {
    return std::ranges::none_of(children, [](const SaveReport::Child& child) {
        return child.held.has_value() || !child.skipped.isEmpty();
    });
}
