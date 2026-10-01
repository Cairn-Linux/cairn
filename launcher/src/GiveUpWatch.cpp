// SPDX-License-Identifier: Apache-2.0
#include "GiveUpWatch.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

GiveUpWatch::GiveUpWatch(QObject* parent) : QObject(parent), m_watcher(this) {
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, &GiveUpWatch::pressed);
}

QString GiveUpWatch::path() const {
    return m_path;
}

void GiveUpWatch::setPath(const QString& path) {
    if (path == m_path) {
        return;
    }
    if (!m_path.isEmpty()) {
        m_watcher.removePath(m_path);
    }
    m_path = path;
    emit pathChanged();
    if (path.isEmpty()) {
        return;
    }
    // A file can only be watched once it exists, so the launcher makes it and
    // the helper writes into it.
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append) || !m_watcher.addPath(path)) {
        qWarning().noquote() << QStringLiteral(
                                    "Could not watch %1, so the give-up key will not bring the "
                                    "tiles back on its own.")
                                    .arg(path);
    }
}

QString GiveUpWatch::sessionPath() {
    return QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation) +
           QStringLiteral("/cairn/give-up");
}
