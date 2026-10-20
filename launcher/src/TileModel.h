// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QQmlParserStatus>
#include <QStringList>
#include <QUrl>

#include <cstdint>

// The list of tiles the launcher draws. C++ decides what the tiles are and
// what each one runs; QML only draws them.
// A list model for the view, and QQmlParserStatus to hear when QML has set
// all its properties: this is how Qt's QML types are meant to learn that, so
// the lint is off.
class TileModel : public QAbstractListModel, // NOLINT(misc-multiple-inheritance)
                  public QQmlParserStatus {
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    QML_ELEMENT
    // A kidscan manifest to read tiles from. Empty means the built-in six.
    Q_PROPERTY(
        QString manifestPath READ manifestPath WRITE setManifestPath NOTIFY manifestPathChanged)
    // The games file provisioning makes from the shared Steam library
    // (ADR-0033). Its tiles follow the manifest's. Empty means none.
    Q_PROPERTY(QString gamesPath READ gamesPath WRITE setGamesPath NOTIFY gamesPathChanged)
    // A plain sentence when the manifest could not be used; empty otherwise.
    Q_PROPERTY(QString loadError READ loadError NOTIFY loadErrorChanged)

public:
    enum class Kind : std::uint8_t { Make, Practice, Games, Machine };
    Q_ENUM(Kind)

    enum Role : std::uint16_t {
        TitleRole = Qt::UserRole + 1,
        KindRole,
        AccessibleNameRole,
        ExecRole,
        OpensTerminalRole,
        PictureRole
    };

    struct Tile {
        QString title;
        Kind kind;
        QStringList exec;
        // The launcher's own Terminal tile: opens the restricted shell inside
        // this window instead of starting a program.
        bool opensTerminal = false;
        // A picture of what the tile opens, as a file:// URL; empty shows the
        // title instead (ADR-0035).
        QUrl picture = {};
    };

    explicit TileModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString manifestPath() const;
    void setManifestPath(const QString& path);
    QString gamesPath() const;
    void setGamesPath(const QString& path);
    QString loadError() const;

    // A TileModel written in QML gets all its paths before it reads the
    // files, so it reads them once. One made in C++ reads them on each change.
    void classBegin() override;
    void componentComplete() override;

    // The tiles as shown, from the files or built in, always ending with the
    // Terminal tile, which is the launcher's and never a file's.
    QList<Tile> tiles() const;
    // The L1 front page (DESIGN §5.1) without its Terminal: shown when there
    // is no manifest, or it cannot be used.
    static QList<Tile> builtInTiles();
    static Tile terminalTile();

signals:
    void manifestPathChanged();
    void gamesPathChanged();
    void loadErrorChanged();

private:
    void load();
    void replaceTiles(const QList<Tile>& tiles);

    QList<Tile> m_tiles;
    QString m_manifestPath;
    QString m_gamesPath;
    QString m_loadError;
    bool m_loadsOnChange = true;
};
