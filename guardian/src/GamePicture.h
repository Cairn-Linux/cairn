// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QJsonObject>
#include <QUrl>

// The picture a games file entry names, read with the launcher's own rule
// (launcher/src/TilePicture.cpp, ADR-0035), so the Games page shows a game
// as the child's tile does: the full path of a file on this computer, never
// a URL, since nothing here fetches from the network.
namespace GamePicture {

// The picture as a file:// URL, or an empty URL when the entry names none,
// or names one that is not a full path to a file here. The game is then
// shown by its title.
QUrl fromEntry(const QJsonObject& entry);

} // namespace GamePicture
