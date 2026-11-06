// SPDX-License-Identifier: Apache-2.0
#include "StandInFaillock.h"

#include <QDir>
#include <QFile>

#include <algorithm>
#include <cstring>

StandInFaillock::StandInFaillock(const QByteArray& settings, QObject* parent) : QObject(parent) {
    QDir(m_root.path()).mkdir(QStringLiteral("faillock"));
    setSettings(QString::fromUtf8(settings));
}

QString StandInFaillock::dir() const {
    return m_root.filePath(QStringLiteral("faillock"));
}

QUrl StandInFaillock::settingsFile() const {
    return QUrl::fromLocalFile(m_root.filePath(QStringLiteral("faillock.conf")));
}

QByteArray StandInFaillock::record(qint64 time, quint16 status, const QByteArray& source) {
    QByteArray bytes(64, '\0');
    std::memcpy(bytes.data(), source.constData(), std::min<qsizetype>(source.size(), 52));
    const auto when = static_cast<quint64>(time);
    std::memcpy(bytes.data() + 54, &status, sizeof status);
    std::memcpy(bytes.data() + 56, &when, sizeof when);
    return bytes;
}

void StandInFaillock::addFailure(const QString& loginName, double time) const {
    QFile file(dir() + QLatin1Char('/') + loginName);
    if (file.open(QIODevice::Append)) {
        file.write(record(static_cast<qint64>(time)));
    }
}

void StandInFaillock::write(const QString& loginName, const QByteArray& bytes) const {
    QFile file(dir() + QLatin1Char('/') + loginName);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(bytes);
    }
}

void StandInFaillock::reset() const {
    QDir folder(dir());
    for (const QString& name : folder.entryList(QDir::Files)) {
        folder.remove(name);
    }
}

void StandInFaillock::setSettings(const QString& settings) const {
    QFile file(m_root.filePath(QStringLiteral("faillock.conf")));
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write("dir = " + dir().toUtf8() + "\n" + settings.toUtf8());
    }
}
