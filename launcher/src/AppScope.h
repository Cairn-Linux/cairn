// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QString>
#include <QStringList>

// In the kiosk every program a tile or the Terminal starts runs in a systemd
// user scope of its own, named cairn-app-*.scope, so the grown-up's give-up
// key can end it and everything it started, however it was launched
// (ADR-0025, #98). The launcher, the compositor and the session's services
// are never in one.
namespace AppScope {

// The command that runs `exec` in a new scope for this launcher's
// `launchNumber`th program. systemd-run becomes the program, so the launcher
// still watches the app's own process.
QStringList command(const QStringList& exec, int launchNumber);

// A scope name no other launch in this session uses: the launcher's process
// id and how many programs it has started.
QString unitName(qint64 launcherPid, int launchNumber);

} // namespace AppScope
