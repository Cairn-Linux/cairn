// SPDX-License-Identifier: Apache-2.0
#include "SteamStartup.h"

#include <QFile>
#include <QList>
#include <QStandardPaths>

#include <algorithm>
#include <optional>
#include <ranges>
#include <utility>

namespace {
// Both logs gain at most a few lines a second while Steam starts. A
// first-time download writes a line per few hundred kilobytes, still well
// inside this.
constexpr qint64 lastPartBytes = qint64{256} * 1024;
// A start that has not finished after this long never will: Steam was
// stopped or crashed. A first install of the client took 15 minutes on the
// kids' laptop's Wi-Fi.
constexpr qint64 longestStartSeconds = qint64{20} * 60;
// After an update the updater starts again at once to verify itself, and
// Steam's start script starts it once more 17 or 18 s later (dev PC,
// 2026-10-27). A start a few minutes on is a later start of its own, and so
// is one that seems to come before the update. The updater writes the
// update line first, so a start seems earlier only when the update's time
// was written in the first pass of the hour that comes twice and read as
// the second (#248), or when the clock was set back between the two lines.
constexpr qint64 restartSeconds = 60;

// A line the updater writes while it downloads or installs an update of the
// client before starting it. It writes some of these while the client runs
// too, but by then the client has run its own start command.
bool isUpdating(const QByteArray& line) {
    static const QList<QByteArray> says = {
        "] Found pending update",     "] Downloading Update...", "] Downloading update",
        "] Downloaded new manifest:", "] Applying update...",    "] Extracting package...",
        "] Installing update...",
    };
    return std::ranges::any_of(says, [&line](const QByteArray& s) { return line.contains(s); });
}
} // namespace

SteamStartup::SteamStartup(QString bootstrapLog, QString consoleLog)
    : m_bootstrapLog(std::move(bootstrapLog)), m_consoleLog(std::move(consoleLog)) {}

SteamStartup::Now SteamStartup::read(const QDateTime& now) const {
    std::optional<QDateTime> started;
    std::optional<QDateTime> updated;
    bool updating = false;
    bool restarting = false;
    for (const QByteArray& line : lastLines(m_bootstrapLog)) {
        if (line.contains("] Startup - ")) {
            started = SteamLaunchLog::timeOf(line, now);
            updating = false;
            restarting = updated && started && *updated <= *started &&
                         updated->secsTo(*started) <= restartSeconds;
        } else if (line.contains("] Update complete, launching")) {
            updated = SteamLaunchLog::timeOf(line, now);
        } else if (started && isUpdating(line)) {
            updating = true;
        }
    }
    if (!started || started->secsTo(now) > longestStartSeconds) {
        return Now::Ready;
    }
    // The client has started once it runs its own start command, 0 to 21 s
    // after it logs `System startup time:`; a tap it held runs with it.
    for (const QByteArray& line : lastLines(m_consoleLog) | std::views::reverse) {
        if (line.contains("] ExecCommandLine: ") && line.contains("'-srt-logger-opened'")) {
            const std::optional<QDateTime> ready = SteamLaunchLog::timeOf(line, now);
            if (ready && *ready >= *started) {
                return Now::Ready;
            }
            break;
        }
    }
    if (updating) {
        return Now::Updating;
    }
    return restarting ? Now::Restarting : Now::Starting;
}

bool SteamStartup::updatingItself(const QDateTime& now) const {
    const Now steam = read(now);
    return steam == Now::Updating || steam == Now::Restarting;
}

QList<QByteArray> SteamStartup::lastLines(const QString& path) {
    QFile log(path);
    if (!log.open(QIODevice::ReadOnly)) {
        return {};
    }
    const qint64 from = std::max(qint64{0}, log.size() - lastPartBytes);
    if (!log.seek(from)) {
        return {};
    }
    QList<QByteArray> lines = log.readAll().split('\n');
    // The first is cut part-way unless the log was read from its start.
    if (from > 0 && !lines.isEmpty()) {
        lines.removeFirst();
    }
    return lines;
}

std::optional<QDateTime> SteamStartup::lastStartup(const QString& bootstrapLog,
                                                   const QDateTime& now) {
    for (const QByteArray& line : lastLines(bootstrapLog) | std::views::reverse) {
        if (line.contains("] Startup - ")) {
            return SteamLaunchLog::timeOf(line, now);
        }
    }
    return std::nullopt;
}

QString SteamStartup::defaultPath() {
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
           QStringLiteral("/Steam/logs/bootstrap_log.txt");
}
