// SPDX-License-Identifier: Apache-2.0
#include "DoorsFromTiles.h"

namespace {

World::Kind worldKind(TileModel::Kind kind) {
    switch (kind) {
    case TileModel::Kind::Make:
        return World::Kind::Make;
    case TileModel::Kind::Practice:
        return World::Kind::Practice;
    case TileModel::Kind::Machine:
        return World::Kind::Machine;
    }
    return World::Kind::Machine;
}

} // namespace

DoorsFromTiles::DoorsFromTiles(QObject* parent) : QObject(parent) {}

TileModel* DoorsFromTiles::tiles() const {
    return m_tiles;
}

void DoorsFromTiles::setTiles(TileModel* tiles) {
    if (tiles == m_tiles) {
        return;
    }
    if (m_tiles != nullptr) {
        disconnect(m_tiles, nullptr, this, nullptr);
    }
    m_tiles = tiles;
    if (m_tiles != nullptr) {
        connect(m_tiles, &TileModel::modelReset, this, &DoorsFromTiles::handOver);
    }
    emit tilesChanged();
    handOver();
}

TerminalSession* DoorsFromTiles::session() const {
    return m_session;
}

void DoorsFromTiles::setSession(TerminalSession* session) {
    if (session == m_session) {
        return;
    }
    m_session = session;
    emit sessionChanged();
    handOver();
}

void DoorsFromTiles::handOver() {
    if (m_tiles == nullptr || m_session == nullptr) {
        return;
    }
    QList<World::Door> doors;
    for (const TileModel::Tile& tile : m_tiles->tiles()) {
        if (!tile.opensTerminal) {
            doors.append({.title = tile.title, .kind = worldKind(tile.kind), .exec = tile.exec});
        }
    }
    m_session->setDoors(doors);
    m_session->reset();
}
