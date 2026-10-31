// SPDX-License-Identifier: Apache-2.0
#include "SteamStall.h"

#include <optional>
#include <utility>

SteamStall::SteamStall(QString contentLog) : m_content(std::move(contentLog)) {}

void SteamStall::startLaunch(const QString& appId) {
    m_appId = appId;
    m_content.startLaunch(appId);
    m_restart = Restart::NotYet;
    m_now = Now::Fine;
    m_stalledThisTap = false;
}

SteamStall::Now SteamStall::read(const SteamLaunchLog& launch, SteamStartup::Now steam,
                                 const QDateTime& now) {
    if (m_appId.isEmpty()) {
        return Now::Fine;
    }
    m_content.read();
    if (m_appId == m_stuckAppId && launch.step() != SteamLaunchLog::Step::Nothing) {
        // Steam is working on the game it kept stuck again.
        m_stuckAppId.clear();
    }
    m_now = decide(launch, steam, now);
    if (m_now == Now::Stuck) {
        m_stuckAppId = m_appId;
    }
    return m_now;
}

SteamStall::Now SteamStall::decide(const SteamLaunchLog& launch, SteamStartup::Now steam,
                                   const QDateTime& now) {
    if (m_restart == Restart::Failed) {
        return Now::Stuck;
    }
    if (m_restart == Restart::Asked) {
        return Now::Mending;
    }
    if (m_restart == Restart::Done && !launch.reachedSteam()) {
        return retaking(launch, steam, now);
    }
    const std::optional<QDateTime> offSchedule = m_content.offScheduleAt();
    if (!launch.downloading() || !offSchedule || offSchedule->secsTo(now) < stallSeconds) {
        return Now::Fine;
    }
    m_stalledThisTap = true;
    return m_restart == Restart::NotYet ? Now::Stalled : Now::Stuck;
}

SteamStall::Now SteamStall::retaking(const SteamLaunchLog& launch, SteamStartup::Now steam,
                                     const QDateTime& now) const {
    if (steam != SteamStartup::Now::Ready) {
        return Now::Mending;
    }
    const std::optional<QDateTime> started = launch.steamStartedAt();
    const qint64 waited = started ? started->msecsTo(now) : m_sinceRestart.elapsed();
    return waited < m_retakeLimitMilliseconds ? Now::Mending : Now::Stuck;
}

bool SteamStall::askRestart() {
    if (m_now != Now::Stalled || m_restart != Restart::NotYet) {
        return false;
    }
    m_restart = Restart::Asked;
    m_now = Now::Mending;
    return true;
}

bool SteamStall::restarted(bool started) {
    if (m_restart != Restart::Asked) {
        return false;
    }
    m_restart = started ? Restart::Done : Restart::Failed;
    if (started) {
        // Only what the new client does counts from now.
        m_content.startLaunch(m_appId);
        m_sinceRestart.start();
    }
    return true;
}

bool SteamStall::logOutWouldHelp() const {
    return m_stalledThisTap || (!m_appId.isEmpty() && m_appId == m_stuckAppId);
}

QString SteamStall::journalNote(const QString& title) const {
    if (!logOutWouldHelp()) {
        return {};
    }
    return QStringLiteral("Steam stopped getting %1 ready, so %1 did not start. "
                          "Log the child out and back in, then try it again.")
        .arg(title);
}

void SteamStall::setRetakeLimitMilliseconds(int milliseconds) {
    m_retakeLimitMilliseconds = milliseconds;
}
