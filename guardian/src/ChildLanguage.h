// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QByteArray>
#include <QString>

#include <optional>
#include <span>

// The languages a Guardian may give a child, and the file that gives one
// (ADR-0058). One table, so adding a language is one line in it, once the
// image has the language's locale.
namespace ChildLanguage {

struct Language {
    // The locale, as LANG takes it: en_GB.UTF-8.
    const char* locale;
    // What LANGUAGE takes, for programs' translations: en_GB:en.
    const char* translations;
    // The name Family shows, in English; nameOf() translates it.
    const char* name;
};

// Every language Family offers, in the order it offers them.
std::span<const Language> all();

// The language with this locale in the table, or nullptr.
const Language* find(const QString& locale);

// The language's name in the reader's own language.
QString nameOf(const Language& language);

// The file's name in a child's folder: Plasma's own name for it, so
// startplasma reads it through XDG_CONFIG_DIRS (ADR-0058).
inline constexpr char fileName[] = "plasma-localerc";

// The file, as Plasma's KConfig reads it, with every entry startplasma
// reads locked by [$i], so no file read after it, the child's own
// ~/.config/plasma-localerc included, can change one.
QByteArray fileText(const Language& language);

// The locale such a file gives, when it is one in the table.
std::optional<QString> localeIn(const QByteArray& text);

} // namespace ChildLanguage
