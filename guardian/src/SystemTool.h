// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QString>
#include <QStringList>

// Runs one of the system's account tools as root, and says whether it
// succeeded.
//
// The program is named by its absolute path and started directly, never
// through a shell, so no argument is ever read as shell syntax. It gets a
// small environment of its own rather than its caller's: under sudo or
// pkexec that environment came from another account. What the tool prints
// on its standard output is dropped, so it cannot mix with cairn-family's
// own one line; what it prints as an error goes on to the journal.
namespace SystemTool {

bool run(const QString& program, const QStringList& arguments);

} // namespace SystemTool
