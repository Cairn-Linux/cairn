// SPDX-License-Identifier: Apache-2.0
#include "ChildGameList.h"

#include <QSet>

#include <algorithm>

namespace {

QSet<QString> appIdsOf(const QList<QJsonObject>& entries) {
    QSet<QString> appIds;
    for (const QJsonObject& entry : entries) {
        if (const std::optional<QString> appId = GameCatalogue::appIdOf(entry)) {
            appIds.insert(*appId);
        }
    }
    return appIds;
}

QString titleOf(const QJsonObject& entry) {
    return entry.value(QStringLiteral("title")).toString();
}

} // namespace

ChildGameList::Plan ChildGameList::plan(const QList<QJsonObject>& old, const QStringList& ticked,
                                        const GameCatalogue& catalogue) {
    const QSet<QString> had = appIdsOf(old);
    Plan plan;
    for (const QString& appId : ticked) {
        if (had.contains(appId)) {
            continue;
        }
        (catalogue.has(appId) ? plan.toCopy : plan.notShared).append(appId);
    }
    return plan;
}

ChildGameList::NewList ChildGameList::result(const QList<QJsonObject>& old,
                                             const QStringList& ticked, const QStringList& copied,
                                             const GameCatalogue& catalogue) {
    const QSet<QString> wanted(ticked.begin(), ticked.end());
    const QSet<QString> gotNow(copied.begin(), copied.end());
    NewList list;
    QSet<QString> listed;
    for (const QJsonObject& entry : old) {
        const std::optional<QString> appId = GameCatalogue::appIdOf(entry);
        if (!appId || listed.contains(*appId)) {
            continue;
        }
        if (!wanted.contains(*appId)) {
            ++list.removed;
            continue;
        }
        list.entries.append(catalogue.has(*appId) ? catalogue.entry(*appId) : entry);
        listed.insert(*appId);
    }
    for (const QString& appId : ticked) {
        if (!listed.contains(appId) && gotNow.contains(appId) && catalogue.has(appId)) {
            list.entries.append(catalogue.entry(appId));
            listed.insert(appId);
            ++list.added;
        }
    }
    std::ranges::stable_sort(list.entries, [](const QJsonObject& left, const QJsonObject& right) {
        return titleOf(left).toLower() < titleOf(right).toLower();
    });
    return list;
}
