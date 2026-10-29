// SPDX-License-Identifier: Apache-2.0
#include "SteamWindowStart.h"

#include <QProcess>

bool SteamWindowStart::startOnItsOwn(const QString& program, const QStringList& arguments) {
    return QProcess::startDetached(program, arguments);
}

bool SteamWindowStart::open(const QString& steamFor, const QString& login, const Start& start) {
    if (start(steamFor, {login})) {
        return true;
    }
    qWarning("cairn-guardian: %s could not be started for %s, so its Steam window did not open.",
             qUtf8Printable(steamFor), qUtf8Printable(login));
    return false;
}
