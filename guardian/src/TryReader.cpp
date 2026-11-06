// SPDX-License-Identifier: Apache-2.0
#include "TryReader.h"

#include "JournalRun.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QTimer>

#include <utility>

namespace {

enum Step : std::uint8_t { OldestEntry, LockScreenLocks, AuditRecords, Steps };

// Only the fields the rules read, so a long entry costs nothing.
const QString fields = QStringLiteral(
    "--output-fields=__REALTIME_TIMESTAMP,_TRANSPORT,_AUDIT_TYPE,_AUDIT_LOGINUID,_PID,_UID,_EXE,"
    "AUDIT_FIELD_RES,AUDIT_FIELD_OP,AUDIT_FIELD_ACCT,AUDIT_FIELD_EXE,AUDIT_FIELD_SUID,"
    "SYSLOG_FACILITY,MESSAGE");

} // namespace

TryReader::TryReader(Setup setup, QObject* parent)
    : QObject(parent), m_setup(std::move(setup)), m_tries(m_setup.lookups) {}

TryReader::State TryReader::state() const {
    return m_state;
}

const TryNotices::Notices& TryReader::notices() const {
    return m_notices;
}

std::optional<qint64> TryReader::oldest() const {
    return m_oldest;
}

QStringList TryReader::arguments(int step) const {
    const QStringList common{QStringLiteral("--quiet"), QStringLiteral("--no-pager"),
                             QStringLiteral("--output=json")};
    const QString since = QStringLiteral("--since=-%1d").arg(m_setup.days);
    switch (step) {
    case OldestEntry:
        return common + QStringList{QStringLiteral("--system"), QStringLiteral("--lines=+1"),
                                    QStringLiteral("--output-fields=__REALTIME_TIMESTAMP")};
    case LockScreenLocks:
        // The lock screen runs as the person, so its lines are in their
        // own journal files, not only the system's.
        return common + QStringList{since,
                                    fields,
                                    QStringLiteral("SYSLOG_FACILITY=10"),
                                    QStringLiteral("_EXE=/usr/libexec/kscreenlocker_greet"),
                                    QStringLiteral("+"),
                                    QStringLiteral("SYSLOG_FACILITY=10"),
                                    QStringLiteral("_EXE=/usr/lib/kscreenlocker_greet")};
    default:
        return common + QStringList{QStringLiteral("--system"),
                                    since,
                                    fields,
                                    QStringLiteral("_TRANSPORT=audit"),
                                    QStringLiteral("_AUDIT_TYPE=1100"),
                                    QStringLiteral("AUDIT_FIELD_RES=failed"),
                                    QStringLiteral("+"),
                                    QStringLiteral("_TRANSPORT=audit"),
                                    QStringLiteral("_AUDIT_TYPE=2100")};
    }
}

TryReader::Readable TryReader::journalReadable() const {
    static const QRegularExpression machineId(QStringLiteral("^[0-9a-f]{32}$"));
    QFile idFile(m_setup.machineId);
    if (!idFile.open(QIODevice::ReadOnly)) {
        return Readable::Missing;
    }
    const QString id = QString::fromLatin1(idFile.read(64)).trimmed();
    if (!machineId.match(id).hasMatch()) {
        return Readable::Missing;
    }
    Readable found = Readable::Missing;
    for (const QString& folder : m_setup.journalFolders) {
        QFile journal(QDir(folder).filePath(id + QStringLiteral("/system.journal")));
        if (journal.open(QIODevice::ReadOnly)) {
            return Readable::Yes;
        }
        if (journal.exists()) {
            found = Readable::No;
        }
    }
    return found;
}

void TryReader::read(const std::optional<FaillockSettings>& settings) {
    if (m_state == State::Reading) {
        return;
    }
    m_state = State::Reading;
    m_notices = {};
    m_oldest.reset();
    const Readable readable = journalReadable();
    if (readable != Readable::Yes) {
        if (readable == Readable::Missing) {
            qWarning("cairn-guardian: there is no system journal to read password tries from.");
        }
        const State state = readable == Readable::No ? State::Unreadable : State::Failed;
        QTimer::singleShot(0, this, [this, state] { finish(state); });
        return;
    }
    m_grouping.emplace(settings);
    runStep(OldestEntry);
}

void TryReader::runStep(int step) {
    m_step = step;
    m_run = new JournalRun(m_setup.journalctl, arguments(step), this);
    connect(m_run, &JournalRun::line, this, &TryReader::take);
    connect(m_run, &JournalRun::finished, this, &TryReader::stepDone);
    m_run->start();
}

void TryReader::take(const QByteArray& line) {
    const std::optional<QJsonObject> entry = JournalTries::parse(line);
    if (!entry) {
        return;
    }
    if (m_step == OldestEntry) {
        bool ok = false;
        const qint64 time =
            entry->value(QStringLiteral("__REALTIME_TIMESTAMP")).toString().toLongLong(&ok);
        if (ok && !m_oldest) {
            m_oldest = time;
        }
        return;
    }
    if (m_step == AuditRecords) {
        if (const std::optional<PasswordTry> found = m_tries.tryFrom(*entry)) {
            m_grouping->addTry(*found);
            return;
        }
    }
    if (const std::optional<LockEvent> lock = m_tries.lockFrom(*entry)) {
        m_grouping->addLock(*lock);
    }
}

void TryReader::stepDone(bool succeeded) {
    m_run->deleteLater();
    m_run = nullptr;
    if (!succeeded) {
        qWarning("cairn-guardian: journalctl failed at step %d of reading password tries.", m_step);
        finish(State::Failed);
        return;
    }
    if (m_step + 1 < Steps) {
        runStep(m_step + 1);
        return;
    }
    m_notices = m_grouping->notices();
    finish(State::Ready);
}

void TryReader::finish(State state) {
    m_grouping.reset();
    m_state = state;
    emit finished();
}
