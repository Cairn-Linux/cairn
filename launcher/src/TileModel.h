// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QStringList>

#include <cstdint>

// The list of tiles the launcher draws. C++ decides what the tiles are and
// what each one runs; QML only draws them.
class TileModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    // A kidscan manifest to read tiles from. Empty means the built-in six.
    Q_PROPERTY(
        QString manifestPath READ manifestPath WRITE setManifestPath NOTIFY manifestPathChanged)
    // A plain sentence when the manifest could not be used; empty otherwise.
    Q_PROPERTY(QString loadError READ loadError NOTIFY loadErrorChanged)

public:
    enum class Kind : std::uint8_t { Make, Practice, Machine };
    Q_ENUM(Kind)

    enum Role : std::uint16_t {
        TitleRole = Qt::UserRole + 1,
        KindRole,
        AccessibleNameRole,
        ExecRole
    };

    struct Tile {
        QString title;
        Kind kind;
        QStringList exec;
    };

    explicit TileModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString manifestPath() const;
    void setManifestPath(const QString& path);
    QString loadError() const;

    static QList<Tile> defaultTiles();

signals:
    void manifestPathChanged();
    void loadErrorChanged();

private:
    void replaceTiles(const QList<Tile>& tiles);

    QList<Tile> m_tiles;
    QString m_manifestPath;
    QString m_loadError;
};
