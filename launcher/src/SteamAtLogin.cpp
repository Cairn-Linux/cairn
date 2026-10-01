// SPDX-License-Identifier: Apache-2.0
#include "SteamAtLogin.h"

#include <QDebug>
#include <QFileInfo>
#include <QProcess>

#include <algorithm>

bool SteamAtLogin::wanted(const QList<TileModel::Tile>& tiles) {
    return std::ranges::any_of(tiles, [](const TileModel::Tile& tile) {
        return !tile.exec.isEmpty() &&
               QFileInfo(tile.exec.first()).fileName() == QStringLiteral("steam") &&
               tile.exec.contains(QStringLiteral("-applaunch"));
    });
}

QStringList SteamAtLogin::command() {
    // A --setenv with no value copies the variable from the launcher, which
    // labwc started on the child's display.
    return {QStringLiteral("systemd-run"),
            QStringLiteral("--user"),
            QStringLiteral("--unit=cairn-steam"),
            QStringLiteral("--collect"),
            QStringLiteral("--quiet"),
            QStringLiteral("--setenv=DISPLAY"),
            QStringLiteral("--setenv=WAYLAND_DISPLAY"),
            QStringLiteral("--"),
            QStringLiteral("steam"),
            QStringLiteral("-silent")};
}

void SteamAtLogin::start() {
    const QStringList run = command();
    if (!QProcess::startDetached(run.first(), run.mid(1))) {
        qWarning().noquote() << QStringLiteral(
            "Could not start Steam in the background; a Steam tile will start it instead.");
    }
}
