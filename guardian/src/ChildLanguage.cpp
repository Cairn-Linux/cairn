// SPDX-License-Identifier: Apache-2.0
#include "ChildLanguage.h"

#include <QByteArrayView>
#include <QCoreApplication>

#include <algorithm>
#include <array>

namespace {

// English first: the image's own language. A new line here needs the
// image to have the locale, and a sentence in ADR-0058.
constexpr std::array languages{
    ChildLanguage::Language{.locale = "en_US.UTF-8",
                            .translations = "en_US:en",
                            .name = QT_TRANSLATE_NOOP("ChildLanguage", "English (US)")},
    ChildLanguage::Language{.locale = "en_GB.UTF-8",
                            .translations = "en_GB:en",
                            .name = QT_TRANSLATE_NOOP("ChildLanguage", "English (UK)")},
};

// Every [Formats] key startplasma copies into the session's environment,
// plasma-workspace 6.8's startkde/startplasma.cpp. A key the file left out
// would come from the child's own ~/.config/environment.d instead.
constexpr std::array formatKeys{
    "LANG",        "LC_ADDRESS",   "LC_COLLATE",     "LC_CTYPE", "LC_IDENTIFICATION",
    "LC_MONETARY", "LC_MESSAGES",  "LC_MEASUREMENT", "LC_NAME",  "LC_NUMERIC",
    "LC_PAPER",    "LC_TELEPHONE", "LC_TIME",        "LC_ALL",
};

constexpr QByteArrayView lockedLang("LANG[$i]=");

} // namespace

std::span<const ChildLanguage::Language> ChildLanguage::all() {
    return languages;
}

const ChildLanguage::Language* ChildLanguage::find(const QString& locale) {
    const auto* const found = std::ranges::find_if(languages, [&locale](const Language& language) {
        return locale == QLatin1String(language.locale);
    });
    return found == languages.end() ? nullptr : &*found;
}

QString ChildLanguage::nameOf(const Language& language) {
    return QCoreApplication::translate("ChildLanguage", language.name);
}

QByteArray ChildLanguage::fileText(const Language& language) {
    QByteArray text = "# Written by cairn-family set-language (ADR-0058). Family changes it.\n"
                      "[Formats]\n";
    for (const char* key : formatKeys) {
        text += QByteArray(key) + "[$i]=" + language.locale + '\n';
    }
    text += QByteArray("\n[Translations]\nLANGUAGE[$i]=") + language.translations + '\n';
    return text;
}

std::optional<QString> ChildLanguage::localeIn(const QByteArray& text) {
    for (const QByteArray& line : text.split('\n')) {
        if (line.startsWith(lockedLang)) {
            QString locale = QString::fromUtf8(line.mid(lockedLang.size()));
            if (find(locale) == nullptr) {
                return std::nullopt;
            }
            return locale;
        }
    }
    return std::nullopt;
}
