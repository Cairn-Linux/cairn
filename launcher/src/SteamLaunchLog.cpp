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
    for (const QByteArray& line : gained.first(end).split('\n')) {
        const Step step = stepOf(line, m_appId);
        if (step != Step::Nothing) {
            m_step = step;
        }
    }
    return m_step;
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

QString SteamLaunchLog::defaultPath() {
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
           QStringLiteral("/Steam/logs/console_log.txt");
}
