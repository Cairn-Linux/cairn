// SPDX-License-Identifier: Apache-2.0
#include "SteamGameWait.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <algorithm>
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

SteamGameWait::SteamGameWait(QString processRoot) : m_processRoot(std::move(processRoot)) {}

void SteamGameWait::steamStartedAtLogin() {
    m_sinceSteamStarted.start();
}

void SteamGameWait::startLaunch(const QStringList& exec) {
    m_appId = appIdOf(exec);
    m_reapersBefore = m_appId.isEmpty() ? QStringList() : reapers();
    m_sinceLaunch.start();
}

bool SteamGameWait::steamGame() const {
    return !m_appId.isEmpty();
}

bool SteamGameWait::stillComing() const {
    if (m_appId.isEmpty() || !m_sinceLaunch.isValid() ||
        m_sinceLaunch.elapsed() >= m_limitMilliseconds) {
        return false;
    }
    const bool steamStarting =
        m_sinceSteamStarted.isValid() && m_sinceSteamStarted.elapsed() < m_startupMilliseconds;
    if (steamStarting) {
        return true;
    }
    const QStringList now = reapers();
    return std::ranges::any_of(
        now, [this](const QString& pid) { return !m_reapersBefore.contains(pid); });
}

void SteamGameWait::setStartupMilliseconds(int milliseconds) {
    m_startupMilliseconds = milliseconds;
}

void SteamGameWait::setLimitMilliseconds(int milliseconds) {
    m_limitMilliseconds = milliseconds;
}

QString SteamGameWait::appIdOf(const QStringList& exec) {
    if (exec.size() < 3 || QFileInfo(exec.first()).fileName() != QStringLiteral("steam")) {
        return {};
    }
    const qsizetype flag = exec.indexOf(QStringLiteral("-applaunch"));
    if (flag < 0 || flag + 1 >= exec.size()) {
        return {};
    }
    const QString& appId = exec.at(flag + 1);
    bool number = false;
    appId.toULongLong(&number);
    return number ? appId : QString();
}

QStringList SteamGameWait::reapers() const {
    const QByteArray appIdArgument = QStringLiteral("AppId=%1").arg(m_appId).toUtf8();
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
