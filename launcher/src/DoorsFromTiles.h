// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "TerminalSession.h"
#include "TileModel.h"

#include <QObject>
#include <QQmlEngine>

// Hands the launcher's tiles to Footpath as its doors, every time the tiles
// change. The Terminal tile itself is not a door. This is the whole of what
// Cairn tells the terminal about programs; Footpath knows nothing of tiles.
class DoorsFromTiles : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(TileModel* tiles READ tiles WRITE setTiles NOTIFY tilesChanged)
    Q_PROPERTY(TerminalSession* session READ session WRITE setSession NOTIFY sessionChanged)

public:
    explicit DoorsFromTiles(QObject* parent = nullptr);

    TileModel* tiles() const;
    void setTiles(TileModel* tiles);
    TerminalSession* session() const;
    void setSession(TerminalSession* session);

signals:
    void tilesChanged();
    void sessionChanged();

private:
    void handOver();

    TileModel* m_tiles = nullptr;
    TerminalSession* m_session = nullptr;
};
