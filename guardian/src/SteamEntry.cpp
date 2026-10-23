// SPDX-License-Identifier: Apache-2.0
#include "SteamEntry.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSaveFile>

QString SteamEntry::text(const QString& login, const QString& name, const QString& steamFor) {
    return QStringLiteral("[Desktop Entry]\n"
                          "Type=Application\n"
                          "Name=%1\n"
                          "Comment=%2\n"
                          "Exec=%3 %4\n"
                          "TryExec=%3\n"
                          "Icon=steam\n"
                          "Categories=Game;\n")
        .arg(QCoreApplication::translate("SteamEntry", "Steam for %1").arg(name),
             QCoreApplication::translate("SteamEntry",
                                         "Open %1's own Steam, to sign in or change its settings")
                 .arg(name),
             steamFor, login);
}

QString SteamEntry::path(const QString& applications, const QString& login) {
    return QDir(applications).filePath(QStringLiteral("cairn-steam-for-%1.desktop").arg(login));
}

SteamEntry::Written SteamEntry::write(const QString& applications, const QString& login,
                                      const QString& text) {
    const QString target = path(applications, login);
    const QByteArray wanted = text.toUtf8();
    QFile existing(target);
    if (existing.open(QIODevice::ReadOnly) && existing.readAll() == wanted) {
        return Written::Unchanged;
    }
    existing.close();

    if (!QDir().mkpath(applications)) {
        return Written::Failed;
    }
    QSaveFile entry(target);
    if (!entry.open(QIODevice::WriteOnly) || entry.write(wanted) != wanted.size() ||
        !entry.commit()) {
        return Written::Failed;
    }
    // Every account's menu reads it, and only root changes it.
    const bool readable =
        QFile::setPermissions(target, QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                          QFileDevice::ReadGroup | QFileDevice::ReadOther);
    return readable ? Written::Changed : Written::Failed;
}
