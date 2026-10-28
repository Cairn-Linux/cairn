// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QString>
#include <QStringList>

#include <functional>
#include <optional>

class QProcess;

// One child's copy of the games on their list (ADR-0041), run by the visit
// helper as a system job: `cairn-steam-visit --copy-games CHILD APPID...`
// in a transient service of its own, started by systemd-run. The job is not
// in the Guardian's session, so a Guardian who logs out mid-copy ends this
// program and not the copy, which lets the child log in again when it ends.
//
// It waits as long as the copy takes, with no time limit, and never kills
// it: a copy cut off holds the child's login until its helper ends.
class CopyJob {
public:
    using Say = std::function<void(const QString&)>;

    struct Setup {
        QString systemdRun = QStringLiteral("/usr/bin/systemd-run");
        QString visitHelper;
    };

    struct Result {
        // The games the copy reported as copied or already there.
        QStringList copied;
        // Why the child's new games were held back, as one word: the
        // helper's refusal or condition, `failed`, or `not-started`.
        std::optional<QString> held;
    };

    // Says each of the helper's lines as it comes, with the child's login
    // put in from `login`, never from the helper. Only lines about the games
    // in `appIds` are said.
    static Result run(const Setup& setup, const QString& login, unsigned int uid,
                      const QStringList& appIds, const Say& say);

    // The most lines read from one copy; the rest are drained unread.
    static constexpr int mostLines = 4000;

private:
    CopyJob(QString login, QStringList appIds, Say say);

    void read(QProcess& process);
    void take(const QString& line);

    QString m_login;
    QStringList m_appIds;
    Say m_say;
    Result m_result;
    // The helper's last word: finished, failed or a refusal.
    std::optional<QString> m_ending;
    int m_lines = 0;
};
