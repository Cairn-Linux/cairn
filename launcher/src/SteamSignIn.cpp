// SPDX-License-Identifier: Apache-2.0
#include "SteamSignIn.h"

#include <QDateTime>
#include <QList>

#include <algorithm>
#include <optional>
#include <utility>

SteamSignIn::SteamSignIn(QString consoleLog, QString bootstrapLog)
    : m_consoleLog(std::move(consoleLog)), m_bootstrapLog(std::move(bootstrapLog)) {}

SteamSignIn::Now SteamSignIn::read(const QDateTime& now) const {
    const std::optional<QDateTime> started = SteamStartup::lastStartup(m_bootstrapLog, now);
    const QByteArray failed = "] LogonFailure ";
    Now signIn = Now::Fine;
    for (const QByteArray& line : SteamStartup::lastLines(m_consoleLog)) {
        if (started) {
            const std::optional<QDateTime> time = SteamLaunchLog::timeOf(line, now);
            if (time && *time < *started) {
                continue;
            }
        }
        if (line.contains("] Client version: ") ||
            line.contains("] Waiting for compat in post-logon took: ")) {
            signIn = Now::Fine;
            continue;
        }
        const qsizetype at = line.indexOf(failed);
        if (at < 0) {
            continue;
        }
        const QByteArray reason = line.mid(at + failed.size()).trimmed();
        if (!triesAgain(reason)) {
            signIn = Now::Refused;
        } else if (signIn == Now::Fine) {
            signIn = Now::NoConnection;
        }
    }
    return signIn;
}

bool SteamSignIn::triesAgain(const QByteArray& reason) {
    // The reasons after which Steam's connection log says `Reconnect`, not
    // `Do not reconnect`, and the generic ones a server sends while it is
    // busy. Anything else, such as `Access Denied`, `Invalid Password` or
    // `Account Logon Denied`, needs a person to sign in.
    static const QList<QByteArray> again = {
        "No Connection", "Service Unavailable", "Try Another CM", "Timeout", "Failure",
    };
    return std::ranges::any_of(again, [&reason](const QByteArray& r) {
        return reason.compare(r, Qt::CaseInsensitive) == 0;
    });
}
