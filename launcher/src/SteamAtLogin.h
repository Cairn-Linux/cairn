// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "TileModel.h"

#include <QList>
#include <QStringList>

// When a child who has Steam games logs in, the Steam client starts in the
// background, so their first game opens without waiting for it and Steam's
// own windows open before play, where labwc hides them (ADR-0027, #119). Only
// for a child with a Steam tile: the client takes about a gigabyte of memory.
namespace SteamAtLogin {

// True when any tile runs a Steam game, `steam -applaunch <appid>`.
bool wanted(const QList<TileModel::Tile>& tiles);

// Starts the client as a service of the child's own systemd, cairn-steam,
// with the display it needs, never inside a game's scope (ADR-0025). A second
// start in the same session finds the service there and does nothing.
QStringList command();

// Runs command() and leaves it running; nothing waits for it.
void start();

} // namespace SteamAtLogin
