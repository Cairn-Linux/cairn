// SPDX-License-Identifier: Apache-2.0
#include "SteamQuestions.h"

void SteamQuestions::startLaunch(const QString& appId) {
    m_appId = appId;
}

SteamLaunchLog::Step SteamQuestions::read(const SteamLaunchLog& launch) {
    const SteamLaunchLog::Step step = launch.step();
    if (launch.clientStarted()) {
        // A new client is asking nothing an earlier one asked.
        m_questioned.clear();
    }
    if (step == SteamLaunchLog::Step::WaitingForAnswer) {
        m_questioned.insert(m_appId);
    } else if (step != SteamLaunchLog::Step::Nothing) {
        m_questioned.remove(m_appId);
    } else if (launch.reachedSteam() && m_questioned.contains(m_appId)) {
        return SteamLaunchLog::Step::WaitingForAnswer;
    }
    return step;
}

void SteamQuestions::steamRestarted() {
    m_questioned.clear();
}
