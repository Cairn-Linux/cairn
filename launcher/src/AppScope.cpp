// SPDX-License-Identifier: Apache-2.0
#include "AppScope.h"

#include <QCoreApplication>

QStringList AppScope::command(const QStringList& exec, int launchNumber) {
    const QString unit = unitName(QCoreApplication::applicationPid(), launchNumber);
    // --collect drops the scope once its programs exit, even after a failure,
    // and -- keeps a program whose name starts with a dash from reading as an
    // option.
    QStringList command{QStringLiteral("systemd-run"), QStringLiteral("--user"),
                        QStringLiteral("--scope"),     QStringLiteral("--quiet"),
                        QStringLiteral("--collect"),   QStringLiteral("--unit=%1").arg(unit),
                        QStringLiteral("--")};
    command.append(exec);
    return command;
}

QString AppScope::unitName(qint64 launcherPid, int launchNumber) {
    return QStringLiteral("cairn-app-%1-%2").arg(launcherPid).arg(launchNumber);
}
