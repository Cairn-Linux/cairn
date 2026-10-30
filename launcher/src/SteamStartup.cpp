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

// A line the updater writes while it downloads or installs an update of the
// client before starting it. A download while the client runs writes others.
bool isUpdating(const QByteArray& line) {
    static const QList<QByteArray> says = {
        "] Found pending update", "] Downloading Update...", "] Downloading update (",
        "] Applying update...",   "] Extracting package...", "] Installing update...",
    };
    return std::ranges::any_of(says, [&line](const QByteArray& s) { return line.contains(s); });
}
} // namespace

SteamStartup::SteamStartup(QString bootstrapLog, QString consoleLog)
    : m_bootstrapLog(std::move(bootstrapLog)), m_consoleLog(std::move(consoleLog)) {}

SteamStartup::Now SteamStartup::read(const QDateTime& now) const {
    std::optional<QDateTime> started;
    bool updating = false;
    for (const QByteArray& line : lastLines(m_bootstrapLog)) {
        if (line.contains("] Startup - ")) {
            started = SteamLaunchLog::timeOf(line);
            updating = false;
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
            const std::optional<QDateTime> ready = SteamLaunchLog::timeOf(line);
            if (ready && *ready >= *started) {
                return Now::Ready;
            }
            break;
        }
    }
    return updating ? Now::Updating : Now::Starting;
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

std::optional<QDateTime> SteamStartup::lastStartup(const QString& bootstrapLog) {
    for (const QByteArray& line : lastLines(bootstrapLog) | std::views::reverse) {
        if (line.contains("] Startup - ")) {
            return SteamLaunchLog::timeOf(line);
        }
    }
    return std::nullopt;
}

QString SteamStartup::defaultPath() {
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
           QStringLiteral("/Steam/logs/bootstrap_log.txt");
}
