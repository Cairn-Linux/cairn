// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QJsonObject>
#include <QString>
#include <QUrl>

#include <optional>

// The picture an entry of the manifest or the games file may name, so a
// child who cannot read yet can still tell one game from another (#138,
// ADR-0035). It is the full path of a file on this computer, such as the
// copy provisioning makes of a game's picture from Steam. Never a URL: the
// launcher fetches nothing.
namespace TilePicture {

struct Result {
    // The picture as a file:// URL; empty when there is none to show.
    QUrl url;
    // Set when the entry names a picture the launcher cannot use: what the
    // entry says, for the journal. The tile stays and shows its name.
    std::optional<QString> refused;
};

// Reads the entry's "picture" key. No key means no picture and nothing to say.
Result fromEntry(const QJsonObject& entry);

} // namespace TilePicture
