// SPDX-License-Identifier: Apache-2.0
#include "SteamContentLog.h"

#include "SteamLaunchLog.h"

#include <QList>
#include <QStandardPaths>

#include <algorithm>
#include <utility>

namespace {
// The app id in a line such as `[2026-10-30 17:10:08] AppID 1070560 update
// started : ...`, right after the time, or empty.
QByteArray appIdOf(const QByteArray& line) {
    const QByteArray marker = "] AppID ";
    if (!line.startsWith('[') || line.indexOf(marker) != 20) {
        return {};
    }
    const qsizetype from = 20 + marker.size();
    const qsizetype end = line.indexOf(' ', from);
    return end < 0 ? QByteArray() : line.mid(from, end - from);
}

// A line about Steam's schedule or a download, which says Steam is still
// doing something for the launch. Lines about an app's files, the space it
// set aside or the download rate do not.
bool aboutDownloads(const QByteArray& line) {
    static const QList<QByteArray> says = {
        " scheduler update : ", " scheduler finished : ", " update started : ",
        " update changed : ",   " finished update",       " update canceled : ",
    };
    return std::ranges::any_of(says, [&line](const QByteArray& s) { return line.contains(s); });
}

// A download ends when it finishes or is cancelled, and every run on the
// schedule ends with the scheduler's own line, a shader cache's included.
bool endsADownload(const QByteArray& line) {
    return line.contains(" finished update") || line.contains(" update canceled : ") ||
           line.contains(" scheduler finished : ");
}
} // namespace

SteamContentLog::SteamContentLog(QString path) : m_tail(std::move(path)) {}

void SteamContentLog::startLaunch(const QString& appId) {
    m_appId = appId.toUtf8();
    m_offSchedule.reset();
    m_launchApps.clear();
    m_downloading.clear();
    if (!m_appId.isEmpty()) {
        m_launchApps.insert(m_appId);
    }
    m_tail.skipToEnd();
}

void SteamContentLog::read() {
    if (m_appId.isEmpty()) {
        return;
    }
    const QList<QByteArray> lines = m_tail.newLines();
    if (m_tail.startedAgain()) {
        // Steam started again and began a new log; all of it is new.
        m_offSchedule.reset();
        m_launchApps = {m_appId};
        m_downloading.clear();
    }
    for (const QByteArray& line : lines) {
        const QByteArray app = appIdOf(line);
        if (app.isEmpty()) {
            continue;
        }
        if (line.contains(" scheduler update : Priority First")) {
            m_launchApps.insert(app);
        }
        if (!m_launchApps.contains(app) || !aboutDownloads(line)) {
            continue;
        }
        if (line.contains(" update started : ")) {
            m_downloading.insert(app);
        } else if (endsADownload(line)) {
            m_downloading.remove(app);
        }
        const bool offSchedule =
            app == m_appId && line.contains(" scheduler finished : removed from schedule ");
        m_offSchedule = offSchedule ? SteamLaunchLog::timeOf(line) : std::nullopt;
    }
}

std::optional<QDateTime> SteamContentLog::offScheduleAt() const {
    if (!m_downloading.isEmpty()) {
        return std::nullopt;
    }
    return m_offSchedule;
}

QString SteamContentLog::defaultPath() {
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
           QStringLiteral("/Steam/logs/content_log.txt");
}
