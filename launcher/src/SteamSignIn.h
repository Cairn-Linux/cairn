// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "SteamLaunchLog.h"
#include "SteamStartup.h"

#include <QByteArray>
#include <QDateTime>
#include <QString>

#include <cstdint>

// Whether the child's Steam client could sign in to its Steam account, read
// from its own log (#182).
//
// A Steam that has not signed in never takes up a tap. Each time it tries
// and fails it writes `LogonFailure <reason>` to `console_log.txt`, and each
// time it signs in it writes `Waiting for compat in post-logon took: ...`.
// Some reasons Steam tries again by itself, every minute or so: `No
// Connection` while the Wi-Fi is down. Others it does not: `Access Denied`
// when Steam no longer takes the saved sign-in, after which it waits for a
// person to sign in on a window the kiosk hides. Its later tries then fail
// with `No Connection` too, so one refusal counts until Steam signs in.
// On the kids' laptop (2026-10-27) a client that could not sign in kept the
// starting screen up for ten minutes.
class SteamSignIn {
public:
    enum class Now : std::uint8_t {
        // Signed in, or nothing in the log says it could not.
        Fine,
        // Steam cannot reach its servers and is trying again by itself.
        NoConnection,
        // Steam's servers turned the sign-in down. A grown-up must sign in.
        Refused,
    };

    explicit SteamSignIn(QString consoleLog = SteamLaunchLog::defaultPath(),
                         QString bootstrapLog = SteamStartup::defaultPath());

    // Where the client that runs now is with signing in, from the last part
    // of both logs, read at `now`. Lines from before the updater last
    // started it, or before the client last wrote its version, are from an
    // earlier client.
    Now read(const QDateTime& now) const;

    // A reason after `LogonFailure ` that Steam tries again by itself.
    static bool triesAgain(const QByteArray& reason);

private:
    QString m_consoleLog;
    QString m_bootstrapLog;
};
