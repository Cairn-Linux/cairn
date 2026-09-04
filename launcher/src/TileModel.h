// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QAbstractListModel>
#include <QQmlEngine>

#include <cstdint>

class TileModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT

public:
    enum class Kind : std::uint8_t { Make, Practice, Machine };
    Q_ENUM(Kind)

    enum Role : std::uint16_t { TitleRole = Qt::UserRole + 1, KindRole, AccessibleNameRole };

    explicit TileModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    struct Tile {
        QString title;
        Kind kind;
    };

    const QList<Tile> m_tiles;
};
