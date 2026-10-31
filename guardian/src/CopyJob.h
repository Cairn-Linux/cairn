// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "SystemJob.h"

#include <QString>
#include <QStringList>

#include <functional>
#include <optional>

// One child's copy of the games on their list (ADR-0041), run by the visit
// helper as a system job (SystemJob): `cairn-steam-visit --copy-games CHILD
// APPID...`. A Guardian who logs out mid-copy ends cairn-family and not the
// copy, which lets the child log in again when it ends.
//
// systemd-inhibit holds a block lock on sleep and the lid switch for as
// long as the copy runs, so closing the lid does not suspend the computer
// halfway. Shutting down stays allowed. When the lock cannot be taken the
// copy never starts and is held `not-started`.
class CopyJob {
public:
    using Say = std::function<void(const QString&)>;

    struct Setup {
        QString systemdRun = QStringLiteral("/usr/bin/systemd-run");
        QString systemdInhibit = QStringLiteral("/usr/bin/systemd-inhibit");
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
    // in `appIds` are said, and every line about Steam's tools, which the
    // copy program copies after the games (ADR-0046). A tool is never a
    // copied game, and never holds the child's games back.
    static Result run(const Setup& setup, const QString& login, unsigned int uid,
                      const QStringList& appIds, const Say& say);

    // The most lines read from one copy; the rest are drained unread.
    static constexpr int mostLines = SystemJob::mostLines;

private:
    CopyJob(QString login, QStringList appIds, Say say);

    void take(const QString& line);
    bool takeGame(const QString& line);
    bool takeTool(const QString& line);

    QString m_login;
    QStringList m_appIds;
    Say m_say;
    Result m_result;
    // The helper's last word: finished, failed or a refusal.
    std::optional<QString> m_ending;
};
