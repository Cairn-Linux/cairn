// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QJsonObject>
#include <QString>
#include <QUrl>

#include <optional>

// The picture on a tile, so a child who cannot read yet can still tell one
// game from another (#138, ADR-0035). It is a file on this computer, never a
// URL: the launcher fetches nothing. A game from the games file shows the
// picture step's copy for its app id (ADR-0047); a manifest entry may name
// the full path of a picture itself.
namespace TilePicture {

struct Result {
    // The picture as a file:// URL; empty when there is none to show.
    QUrl url;
    // Set when the entry names a picture the launcher cannot use: what the
    // entry says, for the journal. The tile stays and shows its name.
    std::optional<QString> refused;
};

// Reads a manifest entry's "picture" key. No key means no picture and
// nothing to say.
Result fromEntry(const QJsonObject& entry);

// The picture step's copy of a Steam game's picture, <folder>/<appId>.jpg,
// as a file:// URL when it is a plain file, not a link. Empty otherwise,
// and for an empty or relative folder or anything but an app id, so no
// name can lead out of the folder. A game without one shows its name.
QUrl forAppId(const QString& folder, const QString& appId);

} // namespace TilePicture
