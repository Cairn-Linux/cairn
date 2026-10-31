// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <optional>

// Runs one of the system's tools as root, and says whether it succeeded,
// or what it said.
//
// The program is named by its absolute path and started directly, never
// through a shell, so no argument is ever read as shell syntax. It gets a
// small environment of its own rather than its caller's: under sudo or
// pkexec that environment came from another account. A tool gets at most
// two minutes, then it is killed.
namespace SystemTool {

// What the tool prints on its standard output is dropped, so it cannot mix
// with cairn-family's own lines; what it prints as an error goes on to the
// journal.
bool run(const QString& program, const QStringList& arguments);

// The same, with `input` on the tool's standard input in place of nothing,
// for a tool that reads a secret there, such as chpasswd (ADR-0043). A
// secret never goes into the arguments, which every account can read in
// /proc while the tool runs.
bool runWithInput(const QString& program, const QStringList& arguments, const QByteArray& input);

// What the tool printed on its standard output, when it succeeded and
// printed at most `maxBytes`; nothing otherwise. Its error output is
// dropped, because it can name files in someone's home.
std::optional<QByteArray> output(const QString& program, const QStringList& arguments,
                                 qint64 maxBytes);

// Each whole line the tool printed on its standard output, whether it
// succeeded or not, for a tool whose own lines say how far it got, such as
// the picture step; none when it printed more than `maxBytes`. Its error
// output is dropped, as for output().
QStringList lines(const QString& program, const QStringList& arguments, qint64 maxBytes);

} // namespace SystemTool
