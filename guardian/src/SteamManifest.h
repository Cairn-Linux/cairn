// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "KeyValues.h"

#include <QString>
#include <QStringList>

#include <optional>

// Which of Steam's appmanifest_*.acf files describe a game, by kidscan's
// rule (tools/kidscan/kidscan.py, app_from_manifest): Steam's runtimes,
// redistributables and Proton are tools, not games, and are never named.
// The Guardian tool cannot run kidscan's Python, so the rule is here
// again, and a test compares the list of tools with kidscan's.
namespace SteamManifest {

struct Game {
    QString appId;
    // Steam's own name for it, or its folder's name when it has none.
    QString name;
};

// The game the manifest in the file `fileName` describes, or nothing when
// it is a tool or not a manifest kidscan would read: its appid must be a
// plain number that names the file, and its name and folder plain text.
// Unlike kidscan, a game still downloading, or on a beta branch, is a game
// here too: what matters is where Steam is putting it.
std::optional<Game> gameIn(const QString& fileName, const KeyValues& manifest);

// Whether Steam ships this as a tool, by its appid or by its name.
bool isTool(const QString& appId, const QString& name);

// The appids of the tools kidscan's APPID_DENYLIST names, for the test.
QStringList toolAppIds();

} // namespace SteamManifest
