// SPDX-License-Identifier: Apache-2.0
#include "OwnSteam.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>

#include <utility>

namespace {

constexpr int kPollMilliseconds = 250;

// Steam says a great deal, and keeps its own logs in its folder, so none of
// it goes to this window's journal.
bool startQuietly(const QString& program, const QStringList& arguments) {
    QProcess process;
    process.setProgram(program);
    process.setArguments(arguments);
    process.setStandardOutputFile(QProcess::nullDevice());
    process.setStandardErrorFile(QProcess::nullDevice());
    return process.startDetached();
}

} // namespace

OwnSteam::OwnSteam(uint uid, Programs programs, QObject* parent)
    : QObject(parent), m_uid(uid), m_programs(std::move(programs)) {
    m_poll.setInterval(kPollMilliseconds);
    connect(&m_poll, &QTimer::timeout, this, &OwnSteam::check);
}

// The client's own process is named steam; its helpers and the scripts that
// start it have other names. The same test as the root helper's pgrep.
bool OwnSteam::running() const {
    const QDir proc(m_programs.proc);
    const QFileInfoList entries = proc.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo& entry : entries) {
        bool isPid = false;
        entry.fileName().toUInt(&isPid);
        if (!isPid || entry.ownerId() != m_uid) {
            continue;
        }
        QFile comm(entry.filePath() + QStringLiteral("/comm"));
        if (comm.open(QIODevice::ReadOnly) && comm.readAll().trimmed() == "steam") {
            return true;
        }
    }
    return false;
}

void OwnSteam::pause() {
    if (m_poll.isActive()) {
        return;
    }
    if (!running()) {
        // Said once control is back with the caller, as when Steam was there.
        QTimer::singleShot(0, this, [this] { emit paused(true); });
        return;
    }
    m_askedToShutDown = true;
    startQuietly(m_programs.steam, {QStringLiteral("-shutdown")});
    m_deadline.setRemainingTime(m_programs.shutdownMilliseconds);
    m_poll.start();
}

bool OwnSteam::resume() {
    // A Steam still running never went, so it is not coming back. It stays
    // asked, and comes back once it has gone and a later visit ends.
    if (!m_askedToShutDown || running()) {
        return false;
    }
    m_askedToShutDown = false;
    const QString program =
        QFileInfo(m_programs.atLogin).isExecutable() ? m_programs.atLogin : m_programs.steam;
    return startQuietly(program, {QStringLiteral("-silent")});
}

void OwnSteam::check() {
    if (!running()) {
        m_poll.stop();
        emit paused(true);
    } else if (m_deadline.hasExpired()) {
        m_poll.stop();
        emit paused(false);
    }
}
