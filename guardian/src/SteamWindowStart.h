// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QString>
#include <QStringList>

#include <functional>

// Starting a child's Steam window from the Games page (ADR-0032): the same
// window the "Steam for <child>" menu entries open, as a program of its
// own, so it outlives the Guardian tool.
namespace SteamWindowStart {

using Start = std::function<bool(const QString&, const QStringList&)>;

// Starts a program on its own, detached from this one.
bool startOnItsOwn(const QString& program, const QStringList& arguments);

// Starts `steamFor` for the child with `start`, and writes a line to the
// system log when it could not be started, where the page sends the
// parent. False then.
bool open(const QString& steamFor, const QString& login, const Start& start);

} // namespace SteamWindowStart
