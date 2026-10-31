// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QString>
#include <QStringList>

#include <functional>

class QProcess;

// One helper run as a system job, as root or as the account `user` names: a
// transient service of its own, started by systemd-run, that waits for it
// and hands back its lines (ADR-0041). The job is not in the Guardian's
// session, so a Guardian who logs out ends cairn-family and not the job.
// cairn-family runs the copy of a child's games, a child's logout and the
// tools step, which fills the tools folder from the Guardian's own Steam
// (ADR-0046), this way.
//
// It waits as long as the job takes and never kills it. Only systemd stops
// a job, and only one with a time limit.
class SystemJob {
public:
    using Take = std::function<void(const QString&)>;

    struct Job {
        QString systemdRun;
        // The service's name, which no two jobs for one child share.
        QString unit;
        // For systemctl status and the journal.
        QString description;
        // The program and its arguments.
        QStringList command;
        // The account the job runs as, by login, with its own groups from
        // the account database; empty means root.
        QString user;
        // systemd stops the job, and everything it started, after this many
        // seconds; 0 means no limit, as for a copy.
        int timeLimitSeconds = 0;
    };

    // Hands each whole line of the job's output to `take`, at most
    // mostLines of them, and returns how many lines it read in all. None
    // means the job never started: the unit's name is still taken, the
    // program is missing, or systemd is not there.
    static int run(const Job& job, const Take& take);

    // The most lines handed on from one job; the rest are drained unread.
    static constexpr int mostLines = 4000;

private:
    static int read(QProcess& process, const Take& take);
};
