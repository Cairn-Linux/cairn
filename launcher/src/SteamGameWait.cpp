// SPDX-License-Identifier: Apache-2.0
#include "SteamGameWait.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <algorithm>
#include <optional>
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

SteamGameWait::SteamGameWait(QString processRoot, QString steamLog, QString steamStartupLog)
    : m_processRoot(std::move(processRoot)), m_log(steamLog), m_startup(steamStartupLog, steamLog),
      m_signIn(std::move(steamLog), std::move(steamStartupLog)) {}

void SteamGameWait::steamStartedAtLogin() {
    m_sinceSteamStarted.start();
}

void SteamGameWait::startLaunch(const QStringList& exec) {
    m_appId = appIdOf(exec);
    m_reapersBefore = m_appId.isEmpty() ? QStringList() : reapers();
    m_log.startLaunch(m_appId);
    m_lastStep = SteamLaunchLog::Step::Nothing;
    m_steamNow = SteamStartup::Now::Ready;
    m_signInNow = SteamSignIn::Now::Fine;
    m_sentAgain = false;
    m_sinceLaunch.start();
    m_sinceWork.start();
}

bool SteamGameWait::steamGame() const {
    return !m_appId.isEmpty();
}

bool SteamGameWait::stillComing() {
    if (m_appId.isEmpty() || !m_sinceLaunch.isValid()) {
        return false;
    }
    m_lastStep = m_log.read();
    m_steamNow = m_startup.read(QDateTime::currentDateTime());
    // Once Steam has the game in hand, its sign-in no longer matters: it
    // starts games while it cannot reach its servers.
    m_signInNow =
        m_lastStep == SteamLaunchLog::Step::Nothing ? m_signIn.read() : SteamSignIn::Now::Fine;
    if (m_signInNow == SteamSignIn::Now::Refused) {
        return false;
    }
    if (m_signInNow == SteamSignIn::Now::NoConnection) {
        return m_sinceLaunch.elapsed() < m_offlineLimitMilliseconds;
    }
    if (steamWorking() && m_sinceLaunch.elapsed() < m_workLimitMilliseconds) {
        m_sinceWork.start();
        return true;
    }
    if (m_sinceWork.elapsed() >= m_limitMilliseconds) {
        return false;
    }
    const bool steamStarting =
        m_sinceSteamStarted.isValid() && m_sinceSteamStarted.elapsed() < m_startupMilliseconds;
    if (steamStarting) {
        return true;
    }
    return newReaper();
}

bool SteamGameWait::steamWorking() const {
    if (m_signInNow != SteamSignIn::Now::Fine) {
        return false;
    }
    // A client started since the tap that has not taken it yet is about to
    // be asked again, or has just been.
    const bool tapToTake = m_log.steamStartedAt().has_value() && !m_log.reachedSteam();
    return m_lastStep == SteamLaunchLog::Step::Working || m_steamNow != SteamStartup::Now::Ready ||
           tapToTake;
}

bool SteamGameWait::sendAgain() {
    if (m_appId.isEmpty() || m_sentAgain || m_log.reachedSteam() ||
        m_signInNow != SteamSignIn::Now::Fine) {
        return false;
    }
    const std::optional<QDateTime> started = m_log.steamStartedAt();
    if (!started || started->secsTo(QDateTime::currentDateTime()) < takeTapSeconds || newReaper()) {
        return false;
    }
    m_sentAgain = true;
    // The game gets its time for a first window from the new tap.
    m_sinceWork.start();
    return true;
}

SteamLaunchLog::Step SteamGameWait::lastStep() const {
    return m_lastStep;
}

SteamSignIn::Now SteamGameWait::signIn() const {
    return m_signInNow;
}

void SteamGameWait::setOfflineLimitMilliseconds(int milliseconds) {
    m_offlineLimitMilliseconds = milliseconds;
}

void SteamGameWait::setStartupMilliseconds(int milliseconds) {
    m_startupMilliseconds = milliseconds;
}

void SteamGameWait::setLimitMilliseconds(int milliseconds) {
    m_limitMilliseconds = milliseconds;
}

void SteamGameWait::setWorkLimitMilliseconds(int milliseconds) {
    m_workLimitMilliseconds = milliseconds;
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

bool SteamGameWait::newReaper() const {
    const QStringList now = reapers();
    return std::ranges::any_of(
        now, [this](const QString& pid) { return !m_reapersBefore.contains(pid); });
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
