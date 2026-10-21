// SPDX-License-Identifier: Apache-2.0
#include "SteamLaunchLog.h"

#include <QFile>
#include <QStandardPaths>

#include <algorithm>
#include <utility>

namespace {
// Steam's log grows by a few lines a second at most. A launch that has
// gained more than this since the last look is read from its last part.
constexpr qint64 mostReadAtOnce = qint64{4} * 1024 * 1024;
} // namespace

SteamLaunchLog::SteamLaunchLog(QString path) : m_path(std::move(path)) {}

void SteamLaunchLog::startLaunch(const QString& appId) {
    m_appId = appId;
    m_step = Step::Nothing;
    m_reached = false;
    m_steamStartedAt.reset();
    const QFile log(m_path);
    m_offset = log.exists() ? log.size() : 0;
}

SteamLaunchLog::Step SteamLaunchLog::read() {
    if (m_appId.isEmpty()) {
        return Step::Nothing;
    }
    QFile log(m_path);
    if (!log.open(QIODevice::ReadOnly)) {
        return m_step;
    }
    const qint64 size = log.size();
    if (size < m_offset) {
        // Steam started again and began a new log; all of it is new.
        m_offset = 0;
        m_step = Step::Nothing;
    }
    const qint64 from = std::max(m_offset, size - mostReadAtOnce);
    if (!log.seek(from)) {
        return m_step;
    }
    const QByteArray gained = log.read(size - from);
    // A line Steam is still writing is read whole next time.
    const qsizetype end = gained.lastIndexOf('\n') + 1;
    m_offset = from + end;
    // The quote after the id keeps 294650 from matching 2946501.
    const QByteArray thisGame = "'-applaunch' '" + m_appId.toUtf8() + "'";
    for (const QByteArray& line : gained.first(end).split('\n')) {
        const Step step = stepOf(line, m_appId);
        if (step != Step::Nothing) {
            m_step = step;
        }
        if (!line.contains("ExecCommandLine: ")) {
            continue;
        }
        if (line.contains(thisGame)) {
            m_reached = true;
        } else if (line.contains("'-srt-logger-opened'")) {
            // A client's own start command, `steam -silent` from the login.
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
