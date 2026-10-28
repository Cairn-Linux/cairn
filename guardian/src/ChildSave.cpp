// SPDX-License-Identifier: Apache-2.0
#include "ChildSave.h"

#include "ChildGameList.h"

QString ChildSave::run(const Shared& shared, const Account& child, const QStringList& ticked,
                       const CopyJob::Say& say) {
    const QList<QJsonObject> old = shared.file.read(child.login);
    const ChildGameList::Plan plan = ChildGameList::plan(old, ticked, shared.catalogue);
    for (const QString& appId : plan.notShared) {
        say(QStringLiteral("skipped %1 %2 not-shared").arg(child.login, appId));
    }

    CopyJob::Result copy;
    if (!plan.toCopy.isEmpty()) {
        say(QStringLiteral("copying %1").arg(child.login));
        copy = CopyJob::run(shared.copy, child.login, child.uid, plan.toCopy, say);
    }

    ChildGameList::NewList list = ChildGameList::result(old, ticked, copy.copied, shared.catalogue);
    std::optional<QString> held = copy.held;
    if (!shared.file.write(child.login, list.entries)) {
        // The file stays as it was, so the child's tiles do too.
        list.added = 0;
        list.removed = 0;
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
