// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QString>
#include <QStringList>

// Steam's reapers for a game, read from the process list (ADR-0029, #132).
//
// Steam starts a game's process under a `reaper` of its own. While one runs
// for the game, Steam is bringing it up, whatever its logs say.
class SteamReapers {
public:
    explicit SteamReapers(QString processRoot = QStringLiteral("/proc"));

    // The process ids of Steam's reapers for the game `appId`: processes of
    // this user called exactly `reaper` with the arguments `SteamLaunch` and
    // `AppId=<appid>`, the same test cairn-give-up makes (#117).
    QStringList of(const QString& appId) const;

private:
    QString m_processRoot;
};
