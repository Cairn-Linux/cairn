// SPDX-License-Identifier: Apache-2.0
#include "ChildSave.h"

#include "ChildGameList.h"

ChildSave::Before ChildSave::takeOff(const Shared& shared, const Account& child,
                                     const QStringList& ticked) {
    Before before{.old = shared.file.read(child.login), .removed = std::nullopt};
    const ChildGameList::NewList list =
        ChildGameList::result(before.old, ticked, {}, shared.catalogue.value_or(GameCatalogue()));
    if (shared.file.write(child.login, list.entries)) {
        before.removed = list.removed;
    }
    return before;
}

QString ChildSave::run(const Shared& shared, const Account& child, const QStringList& ticked,
                       const Before& before, const CopyJob::Say& say) {
    const GameCatalogue catalogue = shared.catalogue.value_or(GameCatalogue());
    const ChildGameList::Plan plan = ChildGameList::plan(before.old, ticked, catalogue);
    // Without a full games list, whether a game is shared is not known.
    const QString notListed =
        shared.catalogue ? QStringLiteral("not-shared") : QStringLiteral("no-catalogue");
    for (const QString& appId : plan.notShared) {
        say(QStringLiteral("skipped %1 %2 %3").arg(child.login, appId, notListed));
    }

    CopyJob::Result copy;
    if (!plan.toCopy.isEmpty()) {
        say(QStringLiteral("copying %1").arg(child.login));
        copy = CopyJob::run(shared.copy, child.login, child.uid, plan.toCopy, say);
    }

    ChildGameList::NewList list = ChildGameList::result(before.old, ticked, copy.copied, catalogue);
    std::optional<QString> held = copy.held;
    if (!shared.file.write(child.login, list.entries)) {
        // The file holds what takeOff wrote, or what it held before.
        list.added = 0;
        list.removed = before.removed.value_or(0);
        held = QStringLiteral("not-written");
    }

    QString line = QStringLiteral("child %1 added %2 removed %3")
                       .arg(child.login)
                       .arg(list.added)
                       .arg(list.removed);
    if (held) {
        line += QStringLiteral(" held ") + *held;
    }
    say(line);
    return line;
}
