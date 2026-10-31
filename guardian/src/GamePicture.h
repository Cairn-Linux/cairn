// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QString>
#include <QUrl>

// A game's picture as the child's tile shows it, with the launcher's own
// rule (launcher/src/TilePicture.cpp, ADR-0035, ADR-0047): the picture
// step's copy for the game's app id, /var/lib/cairn/game-pictures/<appid>.jpg,
// a file on this computer, never a URL, since nothing here fetches from the
// network.
namespace GamePicture {

// The picture as a file:// URL when <folder>/<appId>.jpg is a plain file,
// not a link. An empty URL otherwise, and for an empty or relative folder
// or anything but an app id, so no name can lead out of the folder: the
// game is then shown by its title.
QUrl forAppId(const QString& folder, const QString& appId);

} // namespace GamePicture
