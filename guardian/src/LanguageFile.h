// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "ChildLanguage.h"

#include <QString>

#include <optional>

// Each child's language file (ADR-0058), /var/lib/cairn/locale/LOGIN/
// plasma-localerc: root's alone to write, every account's to read. A
// child's Plasma reads it through XDG_CONFIG_DIRS, and cairn-session reads
// it for the kiosk. Nothing is written in the child's home.
namespace LanguageFile {

// The folder the image keeps them in.
inline constexpr char folder[] = "/var/lib/cairn/locale";

// Writes the child's file, renamed into place, in a folder of their own
// under `parent`, each folder made when missing and checked as
// RootFolder does: a plain folder of `owner`'s that no one else may write
// in, never a link. False when a folder is not safe or the file cannot be
// put in place.
bool write(const QString& parent, unsigned int owner, const QString& restorecon,
           const QString& login, const ChildLanguage::Language& language);

// Removes the child's file, as for a new account with an old login. True
// when it is gone, or was never there.
bool remove(const QString& parent, unsigned int owner, const QString& login);

// The locale the child's file gives, when it is one Family offers.
std::optional<QString> read(const QString& parent, const QString& login);

} // namespace LanguageFile
