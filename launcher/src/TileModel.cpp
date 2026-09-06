// SPDX-License-Identifier: Apache-2.0
#include "TileModel.h"

#include "Manifest.h"

#include <QDebug>

TileModel::TileModel(QObject* parent) : QAbstractListModel(parent), m_tiles(defaultTiles()) {}

QList<TileModel::Tile> TileModel::tiles() const {
    return m_tiles;
}

QList<TileModel::Tile> TileModel::defaultTiles() {
    // The six tiles of the L1 front page (DESIGN §5.1). None launches anything
    // until a manifest names a program for it; the last is the Terminal.
    return {{.title = tr("Draw"), .kind = Kind::Make, .exec = {}, .opensTerminal = false},
            {.title = tr("Music"), .kind = Kind::Make, .exec = {}, .opensTerminal = false},
            {.title = tr("Build"), .kind = Kind::Make, .exec = {}, .opensTerminal = false},
            {.title = tr("Story"), .kind = Kind::Make, .exec = {}, .opensTerminal = false},
            {.title = tr("Practice"), .kind = Kind::Practice, .exec = {}, .opensTerminal = false},
            terminalTile()};
}

TileModel::Tile TileModel::terminalTile() {
    return {.title = tr("Terminal"), .kind = Kind::Machine, .exec = {}, .opensTerminal = true};
}

int TileModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(m_tiles.size());
}

QVariant TileModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.model() != this || index.column() != 0 || index.row() < 0 ||
        index.row() >= rowCount()) {
        return {};
    }

    const Tile& tile = m_tiles.at(index.row());
    switch (role) {
    case TitleRole:
    case AccessibleNameRole:
        return tile.title;
    case KindRole:
        return QVariant::fromValue(tile.kind);
    case ExecRole:
        return tile.exec;
    case OpensTerminalRole:
        return tile.opensTerminal;
    default:
        return {};
    }
}

QHash<int, QByteArray> TileModel::roleNames() const {
    return {{TitleRole, "title"},
            {KindRole, "kind"},
            {AccessibleNameRole, "accessibleName"},
            {ExecRole, "exec"},
            {OpensTerminalRole, "opensTerminal"}};
}

QString TileModel::manifestPath() const {
    return m_manifestPath;
}

void TileModel::setManifestPath(const QString& path) {
    if (path == m_manifestPath) {
        return;
    }
    m_manifestPath = path;
    emit manifestPathChanged();

    const QString previousError = m_loadError;
    if (path.isEmpty()) {
        m_loadError.clear();
        replaceTiles(defaultTiles());
    } else {
        const Manifest::Result result = Manifest::read(path);
        m_loadError = result.error;
        if (result.error.isEmpty()) {
            QList<Tile> tiles = result.tiles;
            tiles.append(terminalTile());
            replaceTiles(tiles);
        } else {
            // A parent reads this in the terminal; the child keeps the built-in tiles.
            qWarning().noquote() << result.error;
            replaceTiles(defaultTiles());
        }
    }
    if (m_loadError != previousError) {
        emit loadErrorChanged();
    }
}

QString TileModel::loadError() const {
    return m_loadError;
}

void TileModel::replaceTiles(const QList<Tile>& tiles) {
    beginResetModel();
    m_tiles = tiles;
    endResetModel();
}
