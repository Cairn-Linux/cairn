// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QString>

#include <cstdint>

// A child's "Steam for <child>" entry in a Guardian's application menu
// (ADR-0032). The text is the same as provisioning's steam_entry, so a
// machine set up either way has the same entries.
namespace SteamEntry {

enum class Written : std::uint8_t { Changed, Unchanged, Failed };

// The entry for the child with this login name, who is known as `name`.
// `steamFor` is the window's wrapper, where this build installs it.
QString text(const QString& login, const QString& name, const QString& steamFor);

// Where the entry for a child goes in the applications folder.
QString path(const QString& applications, const QString& login);

// Writes the entry beside its place and renames it into place, so Plasma,
// which watches the folder, never reads half of one. An entry that already
// says the same is left alone.
Written write(const QString& applications, const QString& login, const QString& text);

} // namespace SteamEntry
