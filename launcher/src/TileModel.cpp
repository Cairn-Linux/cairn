// SPDX-License-Identifier: Apache-2.0
#include "TileModel.h"

#include "TileFiles.h"

#include <QDebug>

TileModel::TileModel(QObject* parent) : QAbstractListModel(parent), m_tiles(builtInTiles()) {
    m_tiles.append(terminalTile());
}

QList<TileModel::Tile> TileModel::tiles() const {
    return m_tiles;
}

QList<TileModel::Tile> TileModel::builtInTiles() {
    // None launches anything until a manifest names a program for it.
    return {{.title = tr("Draw"), .kind = Kind::Make, .exec = {}, .opensTerminal = false},
            {.title = tr("Music"), .kind = Kind::Make, .exec = {}, .opensTerminal = false},
            {.title = tr("Build"), .kind = Kind::Make, .exec = {}, .opensTerminal = false},
            {.title = tr("Story"), .kind = Kind::Make, .exec = {}, .opensTerminal = false},
            {.title = tr("Practice"), .kind = Kind::Practice, .exec = {}, .opensTerminal = false}};
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
    case PictureRole:
        return tile.picture;
    default:
        return {};
    }
}

QHash<int, QByteArray> TileModel::roleNames() const {
    return {{TitleRole, "title"},
            {KindRole, "kind"},
            {AccessibleNameRole, "accessibleName"},
            {ExecRole, "exec"},
            {OpensTerminalRole, "opensTerminal"},
            {PictureRole, "picture"}};
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
    if (m_loadsOnChange) {
        load();
    }
}

QString TileModel::gamesPath() const {
    return m_gamesPath;
}

void TileModel::setGamesPath(const QString& path) {
    if (path == m_gamesPath) {
        return;
    }
    m_gamesPath = path;
    emit gamesPathChanged();
    if (m_loadsOnChange) {
        load();
    }
}

QString TileModel::picturesFolder() const {
    return m_picturesFolder;
}

void TileModel::setPicturesFolder(const QString& folder) {
    if (folder == m_picturesFolder) {
        return;
    }
    m_picturesFolder = folder;
    emit picturesFolderChanged();
    if (m_loadsOnChange) {
        load();
    }
}

QString TileModel::loadError() const {
    return m_loadError;
}

void TileModel::classBegin() {
    m_loadsOnChange = false;
}

void TileModel::componentComplete() {
    m_loadsOnChange = true;
    load();
}

void TileModel::load() {
    const TileFiles::Result result = TileFiles::read(m_manifestPath, m_gamesPath, m_picturesFolder);
    // A parent reads these in the journal; the child sees the tiles that did
    // load, or the built-in ones.
    for (const QString& sentence : result.sentences) {
        qWarning().noquote() << sentence;
    }
    QList<Tile> tiles = result.tiles;
    tiles.append(terminalTile());
    replaceTiles(tiles);
    if (result.manifestError != m_loadError) {
        m_loadError = result.manifestError;
        emit loadErrorChanged();
    }
}

void TileModel::replaceTiles(const QList<Tile>& tiles) {
    beginResetModel();
    m_tiles = tiles;
    endResetModel();
}
