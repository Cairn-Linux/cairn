// SPDX-License-Identifier: Apache-2.0
#include "OwnSteamGames.h"

#include <QDir>
#include <QFileInfo>

#include <algorithm>

namespace {

// kidscan's STEAM_ROOTS: two links Steam makes, its own folder, and the
// Flatpak's.
const QStringList& rootsInHome() {
    static const QStringList roots{
        QStringLiteral(".steam/steam"),
        QStringLiteral(".steam/root"),
        QStringLiteral(".local/share/Steam"),
        QStringLiteral(".var/app/com.valvesoftware.Steam/data/Steam"),
    };
    return roots;
}

// The folder with links followed, or "" when it is not a folder.
QString realFolder(const QString& path) {
    const QFileInfo info(path);
    return info.isDir() ? info.canonicalFilePath() : QString();
}

bool insideOrSame(const QString& folder, const QString& outer) {
    return !outer.isEmpty() && (folder == outer || folder.startsWith(outer + u'/'));
}

} // namespace

QStringList OwnSteamGames::steamRoots(const QString& home) {
    QStringList roots;
    for (const QString& relative : rootsInHome()) {
        const QString root = realFolder(QDir(home).filePath(relative));
        if (!root.isEmpty() && QFileInfo(root + QStringLiteral("/steamapps")).isDir() &&
            !roots.contains(root)) {
            roots.append(root);
        }
    }
    return roots;
}

QStringList OwnSteamGames::libraries(const QString& root, const QString& home) {
    QStringList found;
    const QString own = realFolder(root + QStringLiteral("/steamapps"));
    if (!own.isEmpty()) {
        found.append(own);
    }
    const std::optional<KeyValues> listed =
        KeyValues::readFile(root + QStringLiteral("/steamapps/libraryfolders.vdf"));
    if (!listed) {
        return found;
    }
    const QString block = QStringLiteral("libraryfolders");
    for (const QString& key : listed->keys({block})) {
        // Steam writes a block with a path in it; very old ones wrote the
        // path alone.
        std::optional<QString> path = listed->value({block, key, QStringLiteral("path")});
        if (!path) {
            path = listed->value({block, key});
        }
        if (path && path->startsWith(QStringLiteral("~/"))) {
            *path = QDir(home).filePath(path->mid(2));
        }
        // Steam writes whole paths. Anything else would be read from
        // wherever the window happened to start.
        if (!path || !QDir::isAbsolutePath(*path)) {
            continue;
        }
        const QString library = realFolder(*path + QStringLiteral("/steamapps"));
        if (!library.isEmpty() && !found.contains(library)) {
            found.append(library);
        }
    }
    return found;
}

QList<SteamManifest::Game> OwnSteamGames::find(const QString& home, const QString& sharedLibrary,
                                               const QSet<QString>& shared) {
    const QString family = realFolder(sharedLibrary);
    QList<SteamManifest::Game> games;
    QSet<QString> seen = shared;
    for (const QString& root : steamRoots(home)) {
        for (const QString& library : libraries(root, home)) {
            if (insideOrSame(library, family)) {
                continue;
            }
            const QStringList manifests = QDir(library).entryList(
                {QStringLiteral("appmanifest_*.acf")}, QDir::Files, QDir::Name);
            for (const QString& fileName : manifests) {
                const std::optional<KeyValues> manifest =
                    KeyValues::readFile(QDir(library).filePath(fileName));
                const std::optional<SteamManifest::Game> game =
                    manifest ? SteamManifest::gameIn(fileName, *manifest) : std::nullopt;
                if (game && !seen.contains(game->appId)) {
                    seen.insert(game->appId);
                    games.append(*game);
                }
            }
        }
    }
    std::ranges::stable_sort(games, [](const SteamManifest::Game& a, const SteamManifest::Game& b) {
        return QString::localeAwareCompare(a.name, b.name) < 0;
    });
    return games;
}

QStringList OwnSteamGames::names(const QList<SteamManifest::Game>& games) {
    QStringList found;
    for (const SteamManifest::Game& game : games) {
        found.append(game.name);
    }
    return found;
}
