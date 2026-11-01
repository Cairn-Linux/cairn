// SPDX-License-Identifier: Apache-2.0
#include "SteamLaunchLog.h"

#include <QFileInfo>
#include <QList>
#include <QStandardPaths>

#include <utility>

SteamLaunchLog::SteamLaunchLog(QString path) : m_tail(std::move(path)) {}

void SteamLaunchLog::startLaunch(const QString& appId) {
    m_appId = appId;
    m_step = Step::Nothing;
    m_downloading = false;
    m_reached = false;
    m_clientStarted = false;
    m_steamStartedAt.reset();
    m_tail.skipToEnd();
}

SteamLaunchLog::Step SteamLaunchLog::read() {
    if (m_appId.isEmpty()) {
        return Step::Nothing;
    }
    const QList<QByteArray> lines = m_tail.newLines();
    if (m_tail.startedAgain()) {
        // Steam started again and began a new log; all of it is new.
        m_step = Step::Nothing;
        m_downloading = false;
        m_clientStarted = true;
    }
    // The quote after the id keeps 294650 from matching 2946501.
    const QByteArray thisGame = "'-applaunch' '" + m_appId.toUtf8() + "'";
    for (const QByteArray& line : lines) {
        const Step step = stepOf(line, m_appId);
        if (step != Step::Nothing) {
            m_step = step;
            m_downloading = line.contains("changed task to DownloadingDepots ");
        }
        if (!line.contains("ExecCommandLine: ")) {
            continue;
        }
        // A client's own start command: `steam -silent` from the login, or a
        // tap that found no Steam running and started one with the game.
        const bool clientStart = line.contains("'-srt-logger-opened'");
        if (clientStart) {
            m_clientStarted = true;
        }
        if (line.contains(thisGame)) {
            m_reached = true;
        } else if (clientStart) {
            m_steamStartedAt = timeOf(line);
        }
    }
    return m_step;
}

bool SteamLaunchLog::reachedSteam() const {
    return m_reached;
}

std::optional<QDateTime> SteamLaunchLog::steamStartedAt() const {
    return m_steamStartedAt;
}

bool SteamLaunchLog::clientStarted() const {
    return m_clientStarted;
}

SteamLaunchLog::Step SteamLaunchLog::step() const {
    return m_step;
}

bool SteamLaunchLog::downloading() const {
    return m_downloading;
}

QString SteamLaunchLog::appIdOf(const QStringList& exec) {
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

SteamLaunchLog::Step SteamLaunchLog::stepOf(const QByteArray& line, const QString& appId) {
    // The comma after the id keeps 294650 from matching 2946501.
    const QByteArray game = "GameAction [AppID " + appId.toUtf8() + ", ActionID ";
    const QByteArray launch = " : LaunchApp ";
    const qsizetype at = line.indexOf(game);
    if (appId.isEmpty() || at < 0) {
        return Step::Nothing;
    }
    const qsizetype what = line.indexOf(launch, at + game.size());
    if (what < 0) {
        return Step::Nothing;
    }
    const QByteArray says = line.mid(what + launch.size());
    if (says.startsWith("changed task to Failed ") || says.startsWith("failed with ")) {
        return Step::Refused;
    }
    if (says.startsWith("changed task to WaitingGameWindow ") ||
        says.startsWith("changed task to Completed ")) {
        return Step::GameStarted;
    }
    if (says.startsWith("changed task to ") || says.startsWith("continues with user response")) {
        return Step::Working;
    }
    if (says.startsWith("waiting for user response to ")) {
        return Step::WaitingForAnswer;
    }
    return Step::Nothing;
}

std::optional<QDateTime> SteamLaunchLog::timeOf(const QByteArray& line) {
    if (!line.startsWith('[') || line.size() < 21 || line.at(20) != ']') {
        return std::nullopt;
    }
    QDateTime time = QDateTime::fromString(QString::fromLatin1(line.mid(1, 19)),
                                           QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    if (!time.isValid()) {
        return std::nullopt;
    }
    return time;
}

QString SteamLaunchLog::defaultPath() {
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
           QStringLiteral("/Steam/logs/console_log.txt");
}
