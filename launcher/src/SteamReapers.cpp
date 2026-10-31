// SPDX-License-Identifier: Apache-2.0
#include "SteamReapers.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QList>

#include <unistd.h>
#include <utility>

namespace {
QByteArray readAll(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}
} // namespace

SteamReapers::SteamReapers(QString processRoot) : m_processRoot(std::move(processRoot)) {}

QStringList SteamReapers::of(const QString& appId) const {
    if (appId.isEmpty()) {
        return {};
    }
    const QByteArray appIdArgument = QStringLiteral("AppId=%1").arg(appId).toUtf8();
    const QDir root(m_processRoot);
    const QStringList processes = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    QStringList found;
    for (const QString& pid : processes) {
        bool number = false;
        pid.toUInt(&number);
        if (!number || QFileInfo(root.filePath(pid)).ownerId() != getuid()) {
            continue;
        }
        if (readAll(root.filePath(pid + QStringLiteral("/comm"))).trimmed() != "reaper") {
            continue;
        }
        const QList<QByteArray> arguments =
            readAll(root.filePath(pid + QStringLiteral("/cmdline"))).split('\0');
        if (arguments.contains("SteamLaunch") && arguments.contains(appIdArgument)) {
            found.append(pid);
        }
    }
    return found;
}
