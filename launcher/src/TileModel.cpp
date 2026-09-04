// SPDX-License-Identifier: Apache-2.0
#include "TileModel.h"

TileModel::TileModel(QObject* parent)
    : QAbstractListModel(parent), m_tiles{{.title = tr("Draw"), .kind = Kind::Make},
                                          {.title = tr("Music"), .kind = Kind::Make},
                                          {.title = tr("Build"), .kind = Kind::Make},
                                          {.title = tr("Story"), .kind = Kind::Make},
                                          {.title = tr("Practice"), .kind = Kind::Practice},
                                          {.title = tr("Terminal"), .kind = Kind::Machine}} {}

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
    default:
        return {};
    }
}

QHash<int, QByteArray> TileModel::roleNames() const {
    return {{TitleRole, "title"}, {KindRole, "kind"}, {AccessibleNameRole, "accessibleName"}};
}
