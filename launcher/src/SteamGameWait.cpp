// SPDX-License-Identifier: Apache-2.0
#include "SteamGameWait.h"

#include <QDateTime>
#include <QFileInfo>

#include <algorithm>
#include <optional>
#include <utility>

SteamGameWait::SteamGameWait(QString processRoot, QString steamLog, QString steamStartupLog,
                             QString steamContentLog)
    : m_reapers(std::move(processRoot)), m_log(steamLog), m_startup(steamStartupLog, steamLog),
      m_signIn(std::move(steamLog), std::move(steamStartupLog)),
      m_stall(std::move(steamContentLog)) {}

void SteamGameWait::steamStartedAtLogin() {
    m_sinceSteamStarted.start();
}

void SteamGameWait::startLaunch(const QStringList& exec) {
    m_appId = appIdOf(exec);
    m_reapersBefore = m_reapers.of(m_appId);
    m_log.startLaunch(m_appId);
    m_stall.startLaunch(m_appId);
    m_lastStep = SteamLaunchLog::Step::Nothing;
    m_steamNow = SteamStartup::Now::Ready;
    m_signInNow = SteamSignIn::Now::Fine;
    m_stallNow = SteamStall::Now::Fine;
    m_sentAgain = false;
    m_sinceLaunch.start();
    m_sinceTapSent.start();
    m_sinceWork.start();
}

bool SteamGameWait::steamGame() const {
    return !m_appId.isEmpty();
}

bool SteamGameWait::stillComing() {
    if (m_appId.isEmpty() || !m_sinceLaunch.isValid()) {
        return false;
    }
    const QDateTime now = QDateTime::currentDateTime();
    m_lastStep = m_log.read();
    m_steamNow = m_startup.read(now);
    m_stallNow = m_stall.read(m_log, m_steamNow, now);
    // Once Steam has the game in hand, its sign-in no longer matters: it
    // starts games while it cannot reach its servers.
    m_signInNow =
        m_lastStep == SteamLaunchLog::Step::Nothing ? m_signIn.read() : SteamSignIn::Now::Fine;
    if (m_signInNow == SteamSignIn::Now::Refused ||
        (m_stallNow == SteamStall::Now::Stuck && !newReaper())) {
        return false;
    }
    if (m_signInNow == SteamSignIn::Now::NoConnection) {
        return m_sinceTapSent.elapsed() < m_offlineLimitMilliseconds;
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
           tapToTake || m_stallNow == SteamStall::Now::Mending;
}

bool SteamGameWait::sendAgain() {
    if (m_appId.isEmpty() || m_sentAgain || m_log.reachedSteam() ||
        m_signInNow != SteamSignIn::Now::Fine || m_stallNow == SteamStall::Now::Stuck) {
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

bool SteamGameWait::restartSteam() {
    // Read here: stillComing() counts Steam signed in once it names the game.
    if (m_stallNow != SteamStall::Now::Stalled || m_steamNow != SteamStartup::Now::Ready ||
        m_signIn.read() != SteamSignIn::Now::Fine || newReaper() || !m_stall.askRestart()) {
        return false;
    }
    m_stallNow = SteamStall::Now::Mending;
    return true;
}

void SteamGameWait::steamRestarted(bool started) {
    if (!m_stall.restarted(started)) {
        return;
    }
    // What the old client wrote as it stopped says nothing about the game.
    m_log.startLaunch(m_appId);
    if (started) {
        // The new client gets what a tap gets, however late in the tap's
        // work limit the stall was seen.
        m_sentAgain = false;
        m_sinceTapSent.start();
        m_sinceLaunch.start();
    }
}

bool SteamGameWait::logOutWouldHelp() const {
    return !steamGaveAReason() && m_stall.logOutWouldHelp();
}

SteamLaunchLog::Step SteamGameWait::lastStep() const {
    return m_lastStep;
}

SteamSignIn::Now SteamGameWait::signIn() const {
    return m_signInNow;
}

QString SteamGameWait::journalNote(const QString& title) const {
    if (m_signInNow == SteamSignIn::Now::Refused) {
        return QStringLiteral("Steam could not sign in, so %1 did not start. "
                              "Steam needs to be signed in again.")
            .arg(title);
    }
    if (m_signInNow == SteamSignIn::Now::NoConnection) {
        return QStringLiteral("Steam could not reach the internet, so %1 did not start.")
            .arg(title);
    }
    if (m_lastStep == SteamLaunchLog::Step::WaitingForAnswer) {
        return QStringLiteral(
                   "Steam is waiting for an answer about %1 on a window the kiosk hides.")
            .arg(title);
    }
    if (m_lastStep == SteamLaunchLog::Step::Refused) {
        return QStringLiteral("Steam refused to start %1.").arg(title);
    }
    return m_stall.journalNote(title);
}

bool SteamGameWait::steamGaveAReason() const {
    return m_signInNow != SteamSignIn::Now::Fine ||
           m_lastStep == SteamLaunchLog::Step::WaitingForAnswer ||
           m_lastStep == SteamLaunchLog::Step::Refused;
}

void SteamGameWait::setOfflineLimitMilliseconds(int milliseconds) {
    m_offlineLimitMilliseconds = milliseconds;
}

void SteamGameWait::setRetakeLimitMilliseconds(int milliseconds) {
    m_stall.setRetakeLimitMilliseconds(milliseconds);
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
    const QStringList now = m_reapers.of(m_appId);
    return std::ranges::any_of(
        now, [this](const QString& pid) { return !m_reapersBefore.contains(pid); });
}
