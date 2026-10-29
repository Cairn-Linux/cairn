// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QString>
#include <QStringList>

#include <cstdint>
#include <functional>
#include <optional>

class QProcess;

// One child's copy of the games on their list (ADR-0041), run by the visit
// helper as a system job: `cairn-steam-visit --copy-games CHILD APPID...`.
// For a Save it runs in a transient service of its own, started by
// systemd-run. That job is not in the Guardian's session, so a Guardian who
// logs out mid-copy ends this program and not the copy, which lets the
// child log in again when it ends.
//
// It waits as long as the copy takes, with no time limit, and never kills
// it: a copy cut off holds the child's login until its helper ends.
//
// For a child the parent chose to log out now, the helper is given
// `--log-out`, and logs the child out first when they are logged in.
//
// finish-games is already a system unit of its own, so it runs the helper
// itself. It cannot use systemd-run --pipe: inside a system service, the
// service manager drops the request when the output it is handed is a pipe
// made there, which QProcess's always is (systemd 259, seen 2026-10-29).
class CopyJob {
public:
    using Say = std::function<void(const QString&)>;

    // Where the helper runs: in a transient service of its own, or in this
    // program's own system unit.
    enum class Runs : std::uint8_t { InItsOwnUnit, InThisUnit };

    struct Setup {
        QString systemdRun = QStringLiteral("/usr/bin/systemd-run");
        QString visitHelper;
        Runs runs = Runs::InItsOwnUnit;
    };

    // What the copy does when the child is logged in: holds their games
    // until they log out, or logs them out first.
    enum class IfLoggedIn : std::uint8_t { Hold, LogOut };

    struct Result {
        // The games the copy reported as copied or already there.
        QStringList copied;
        // Why the child's new games were held back, as one word: the
        // helper's refusal or condition, `failed`, or `not-started`. A
        // log-out that a program of the child's outlived is `stuck`.
        std::optional<QString> held;
    };

    // Says each of the helper's lines as it comes, with the child's login
    // put in from `login`, never from the helper. Only lines about the games
    // in `appIds` are said, and `logging-out` and `logged-out`.
    static Result run(const Setup& setup, const QString& login, unsigned int uid,
                      const QStringList& appIds, IfLoggedIn ifLoggedIn, const Say& say);

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
