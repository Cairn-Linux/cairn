// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QString>
#include <QStringList>

#include <optional>

// Fills the tools folder, /var/lib/cairn/steam-tools, with Steam's own tools
// from the Guardian's own Steam, at every Save and every look for new games
// (ADR-0046), so that the copy step can give each child the tools with their
// games. It runs the tools step, provision/cairn-share-tools.py, as a system
// job, as the Guardian whose password opened the Save and never as root,
// and systemd stops the job after timeLimitSeconds. Whenever the step does
// not finish, the folder keeps what it had.
namespace ToolsRefresh {

// Where each part is. main() gives the tools step's place; a test gives
// stand-ins.
struct Setup {
    QString systemdRun = QStringLiteral("/usr/bin/systemd-run");
    QString python = QStringLiteral("/usr/bin/python3");
    QString shareStep;
};

struct Result {
    // The step ran to its end.
    bool made = false;
    // Each `tool-not-shared APPID REASON` line, which the Save says too.
    QStringList leftOut;
    // The step's lines worth a line in the journal: all but `tool-had` and
    // `finished`.
    QStringList notes;
};

// Four tools took about 3 s on the VM. This leaves room for a slow disk,
// and a library on a drive that never answers cannot hold a Save for ever.
inline constexpr int timeLimitSeconds = 300;

// `guardian` is the login of the Guardian whose Steam has the tools. With
// none, under sudo, it starts nothing, and the folder stays as it is.
Result run(const Setup& setup, const std::optional<QString>& guardian);

// Takes one of the step's lines into `result`, by the step's own grammar.
// Any other line is left out.
void take(Result& result, const QString& line);

} // namespace ToolsRefresh
