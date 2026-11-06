// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "FaillockSettings.h"
#include "JournalTries.h"
#include "TryNotices.h"

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringList>

#include <cstdint>
#include <optional>

class JournalRun;
class QDir;

// Reads the last days of the journal into notices, as the Guardian, without
// root (ADR-0054): systemd lets the wheel group read every journal file,
// each user's own too, and every Guardian Cairn makes is in wheel. Three
// runs of journalctl, one after another: the journal's oldest entry, the
// lock screen's locks from every file, then, from the system's file, the
// audit records of password checks and locks.
class TryReader : public QObject {
    Q_OBJECT

public:
    enum class State : std::uint8_t {
        Reading,
        Ready,
        // This account cannot read the system's journal. journalctl would
        // still succeed, with only the account's own entries, so the page
        // would say "no wrong passwords" and be wrong.
        Unreadable,
        Failed, // journalctl did not run, or failed
    };

    struct Setup {
        QString journalctl = QStringLiteral("/usr/bin/journalctl");
        // Where journald keeps the system's journal, persistent then
        // volatile, each with a folder named by the machine's id.
        QStringList journalFolders{QStringLiteral("/var/log/journal"),
                                   QStringLiteral("/run/log/journal")};
        QString machineId = QStringLiteral("/etc/machine-id");
        int days = 30;
        JournalTries::Lookups lookups;
    };

    explicit TryReader(Setup setup, QObject* parent = nullptr);

    // Starts a read, grouping tries by faillock's settings, or its
    // defaults without them; nothing while one runs. finished() follows,
    // always after read() returns.
    void read(const std::optional<FaillockSettings>& settings);
    State state() const;
    const TryNotices::Notices& notices() const;
    // The journal's oldest entry, in microseconds, when it has one.
    std::optional<qint64> oldest() const;

    // The program lines journalctl is asked for, by step, for the tests.
    QStringList arguments(int step) const;

signals:
    void finished();

private:
    enum class Readable : std::uint8_t { Yes, No, Missing };

    Readable journalReadable() const;
    static void warnOfUnreadableUserFiles(const QDir& machine);
    void runStep(int step);
    void take(const QByteArray& line);
    void stepDone(bool succeeded);
    void finish(State state);

    Setup m_setup;
    JournalTries m_tries;
    std::optional<TryNotices> m_grouping;
    TryNotices::Notices m_notices;
    std::optional<qint64> m_oldest;
    JournalRun* m_run = nullptr;
    int m_step = 0;
    State m_state = State::Ready;
};
