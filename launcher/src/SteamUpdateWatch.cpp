// SPDX-License-Identifier: Apache-2.0
#include "SteamUpdateWatch.h"

#include "SteamLaunchLog.h"

#include <QByteArray>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <unistd.h>

namespace {
// Often enough that the stones come and go within a few seconds of Steam's
// log, seldom enough that reading the last part of a log costs little.
constexpr int readMilliseconds = 5000;
// Under this, the journal counts an update in seconds, not minutes.
constexpr qint64 secondsBeforeMinutes = 120;

QByteArray readAll(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

bool logsSayUpdating(SteamStartup::Now now) {
    return now == SteamStartup::Now::Updating || now == SteamStartup::Now::Restarting;
}

// A time in Steam's log, for the journal.
QString clockTime(const QDateTime& time) {
    return time.toString(QStringLiteral("HH:mm:ss"));
}
} // namespace

SteamUpdateWatch::SteamUpdateWatch(QObject* parent) : QObject(parent), m_timer(this) {
    m_timer.setInterval(readMilliseconds);
    connect(&m_timer, &QTimer::timeout, this, [this] { readAt(QDateTime::currentDateTime()); });
}

bool SteamUpdateWatch::active() const {
    return m_active;
}

void SteamUpdateWatch::setActive(bool active) {
    if (active == m_active) {
        return;
    }
    m_active = active;
    emit activeChanged();
    if (active) {
        m_timer.start();
        readAt(QDateTime::currentDateTime());
    } else {
        m_timer.stop();
    }
}

bool SteamUpdateWatch::updating() const {
    return m_updating;
}

QString SteamUpdateWatch::processRoot() const {
    return m_processRoot;
}

void SteamUpdateWatch::setProcessRoot(const QString& processRoot) {
    if (processRoot == m_processRoot) {
        return;
    }
    m_processRoot = processRoot;
    emit processRootChanged();
    if (m_active) {
        readAt(QDateTime::currentDateTime());
    }
}

void SteamUpdateWatch::readAt(const QDateTime& now) {
    // Only a watch that saw an update needs its start command after the
    // longest start, to say when it finished.
    const SteamStartup::Reading reading = m_startup.readWithTimes(now, m_updating);
    const bool steamRuns = logsSayUpdating(reading.now) && steamRunning();
    const bool updating = logsSayUpdating(reading.now) && steamRuns;
    if (updating == m_updating) {
        return;
    }
    if (updating) {
        // The logs say Steam is updating only after a start, so there is one.
        m_startedAt = reading.lastStartup.value_or(now);
        qInfo().noquote() << QStringLiteral("Steam started updating itself at %1 by its log, so "
                                            "its games wait on the tiles.")
                                 .arg(clockTime(*m_startedAt));
    } else {
        sayWhyItEnded(reading, steamRuns);
    }
    m_heldBack.clear();
    m_updating = updating;
    emit updatingChanged();
}

bool SteamUpdateWatch::holdsBack(const QString& title, const QStringList& exec) {
    const QString appId = SteamLaunchLog::appIdOf(exec);
    if (appId.isEmpty() || !m_updating) {
        return false;
    }
    // The last read may be up to 5 s old: a press on a game Steam has just
    // finished with opens it.
    readAt(QDateTime::currentDateTime());
    if (!m_updating) {
        return false;
    }
    if (!m_heldBack.contains(appId)) {
        m_heldBack.insert(appId);
        qInfo().noquote()
            << QStringLiteral("%1 did not open: Steam is updating itself.").arg(title);
    }
    return true;
}

bool SteamUpdateWatch::steamRunning() const {
    const QDir root(m_processRoot);
    const QStringList processes = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& pid : processes) {
        bool number = false;
        pid.toUInt(&number);
        if (!number || QFileInfo(root.filePath(pid)).ownerId() != getuid()) {
            continue;
        }
        if (readAll(root.filePath(pid + QStringLiteral("/comm"))).trimmed() == "steam") {
            return true;
        }
    }
    return false;
}

void SteamUpdateWatch::sayWhyItEnded(const SteamStartup::Reading& reading, bool steamRuns) const {
    if (reading.clientStarted) {
        const std::optional<QDateTime> from = m_startedAt ? m_startedAt : reading.lastStartup;
        const qint64 seconds = from ? from->secsTo(*reading.clientStarted) : 0;
        const QString took = seconds < secondsBeforeMinutes
                                 ? QStringLiteral("%1 seconds").arg(seconds)
                                 : QStringLiteral("%1 minutes").arg(seconds / 60);
        qInfo().noquote() << QStringLiteral("Steam finished updating itself at %1 by its log, "
                                            "after %2. Its games open again.")
                                 .arg(clockTime(*reading.clientStarted), took);
    } else if (reading.now == SteamStartup::Now::Ready && reading.lastStartup) {
        // Ready with a start and no start command: the start is too old.
        qInfo().noquote() << QStringLiteral("Steam's log has shown no start for 20 minutes, so "
                                            "its games open again on the tiles. Steam may still "
                                            "be updating.");
    } else if (logsSayUpdating(reading.now) && !steamRuns) {
        qInfo().noquote() << QStringLiteral(
            "Steam is not running, so its games open again on the tiles.");
    } else {
        qInfo().noquote() << QStringLiteral("Steam's log no longer shows an update, so its games "
                                            "open again on the tiles.");
    }
}
