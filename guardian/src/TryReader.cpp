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

// The lock screen's programs, as JournalTries believes them.
const QStringList lockScreenPrograms{
    QStringLiteral("/usr/libexec/kscreenlocker_worker"),
    QStringLiteral("/usr/lib/kscreenlocker_worker"),
    QStringLiteral("/usr/libexec/kscreenlocker_greet"),
    QStringLiteral("/usr/lib/kscreenlocker_greet"),
};

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
    case LockScreenLocks: {
        // The lock screen runs as the person, so journald keeps its lines
        // in that person's own file, user-<uid>.journal. With neither
        // --system nor --user, journalctl reads every file it may open.
        QStringList asked = common + QStringList{since, fields};
        for (const QString& program : lockScreenPrograms) {
            if (asked.size() > common.size() + 2) {
                asked << QStringLiteral("+");
            }
            asked << QStringLiteral("SYSLOG_FACILITY=10") << QStringLiteral("_EXE=") + program;
        }
        return asked;
    }
    default:
        // Every password check, wrong or right, the account part's right
        // ones, which can empty faillock's count, and faillock's locks.
        return common + QStringList{QStringLiteral("--system"),
                                    since,
                                    fields,
                                    QStringLiteral("_TRANSPORT=audit"),
                                    QStringLiteral("_AUDIT_TYPE=1100"),
                                    QStringLiteral("+"),
                                    QStringLiteral("_TRANSPORT=audit"),
                                    QStringLiteral("_AUDIT_TYPE=1101"),
                                    QStringLiteral("AUDIT_FIELD_RES=success"),
                                    QStringLiteral("+"),
                                    QStringLiteral("_TRANSPORT=audit"),
                                    QStringLiteral("_AUDIT_TYPE=2100")};
    }
}

TryReader::Readable TryReader::journalReadable() {
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
        const QDir machine(QDir(folder).filePath(id));
        QFile journal(machine.filePath(QStringLiteral("system.journal")));
        if (journal.open(QIODevice::ReadOnly)) {
            m_unreadUsers = unreadableUserFiles(machine);
            return Readable::Yes;
        }
        if (journal.exists()) {
            found = Readable::No;
        }
    }
    return found;
}

// systemd gives wheel read access to every user's file through the
// folder's default ACL, but journalctl skips a file it may not open
// without a word, and the lock screen's locks are only in those files.
// The page still shows every try, from the system's file, and the locks
// worked out from them (ADR-0054). The uids whose files, current or
// archived (user-1000@...journal), could not be read.
std::vector<unsigned int> TryReader::unreadableUserFiles(const QDir& machine) {
    static const QRegularExpression userFile(QStringLiteral("^user-([0-9]{1,9})[@.]"));
    std::vector<unsigned int> uids;
    const QStringList files =
        machine.entryList({QStringLiteral("user-*.journal")}, QDir::Files | QDir::System);
    for (const QString& name : files) {
        QFile file(machine.filePath(name));
        if (file.open(QIODevice::ReadOnly)) {
            continue;
        }
        qWarning("cairn-guardian: cannot read %s; the lock screen's locks in it are not shown.",
                 qUtf8Printable(name));
        const QRegularExpressionMatch match = userFile.match(name);
        if (match.hasMatch()) {
            uids.push_back(match.captured(1).toUInt());
        }
    }
    return uids;
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
    for (const unsigned int uid : m_unreadUsers) {
        m_grouping->lockScreenUnread(uid);
    }
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
        if (const std::optional<GoodPassword> good = m_tries.goodFrom(*entry)) {
            m_grouping->addGood(*good);
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
