// SPDX-License-Identifier: Apache-2.0
#include "SteamManifest.h"

#include <QRegularExpression>

namespace {

// kidscan's APPID, PROTON_NAME_RE and plain_folder_name, word for word.
const QRegularExpression& appIdPattern() {
    static const QRegularExpression pattern(
        QRegularExpression::anchoredPattern(QStringLiteral("[1-9][0-9]{0,9}")));
    return pattern;
}

const QRegularExpression& toolNamePattern() {
    static const QRegularExpression pattern(
        QStringLiteral("^(proton|steam linux runtime|steamworks)"),
        QRegularExpression::CaseInsensitiveOption);
    return pattern;
}

bool plainFolderName(const QString& name) {
    return !name.isEmpty() && name != u"." && name != u".." && !name.contains(u'/') &&
           !name.contains(QChar(u'\0')) && !name.startsWith(u".cairn-");
}

} // namespace

QStringList SteamManifest::toolAppIds() {
    return {
        QStringLiteral("228980"),  // Steamworks Common Redistributables
        QStringLiteral("1070560"), // Steam Linux Runtime 1.0
        QStringLiteral("1391110"), // Steam Linux Runtime 2.0 (soldier)
        QStringLiteral("1628350"), // Steam Linux Runtime 3.0 (sniper)
        QStringLiteral("1493710"), // Proton Experimental
    };
}

bool SteamManifest::isTool(const QString& appId, const QString& name) {
    return toolAppIds().contains(appId) || toolNamePattern().match(name).hasMatch();
}

std::optional<SteamManifest::Game> SteamManifest::gameIn(const QString& fileName,
                                                         const KeyValues& manifest) {
    const std::optional<QString> appId =
        manifest.value({QStringLiteral("AppState"), QStringLiteral("appid")});
    // A manifest with no name is still a game, named for its folder.
    const std::optional<QString> name =
        manifest.keys({QStringLiteral("AppState")})
                .contains(QStringLiteral("name"), Qt::CaseInsensitive)
            ? manifest.value({QStringLiteral("AppState"), QStringLiteral("name")})
            : QString();
    const std::optional<QString> folder =
        manifest.value({QStringLiteral("AppState"), QStringLiteral("installdir")});
    if (!appId || !appIdPattern().match(*appId).hasMatch() ||
        fileName != QStringLiteral("appmanifest_%1.acf").arg(*appId)) {
        return std::nullopt;
    }
    if (!name || !folder || !plainFolderName(*folder) || isTool(*appId, *name)) {
        return std::nullopt;
    }
    return Game{.appId = *appId, .name = name->isEmpty() ? *folder : *name};
}
