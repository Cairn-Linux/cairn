// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include <cstdint>
#include <optional>

// Everything the restricted shell can see, and nothing else (ADR-0013,
// TERMINAL-DESIGN §3): doors to the programs the host allows, one folder per
// kind, and "home", a small read-only layout of folders and notes. Every
// node has a path like "make/draw" or "home/sam/notes"; the root is "".
class World {
public:
    enum class Kind : std::uint8_t { Folder, Make, Practice, Machine, Note };

    // A program the host will start. `kind` is Make, Practice or Machine.
    struct Door {
        QString title;
        Kind kind;
        QStringList exec;
    };

    struct Node {
        QString path;
        QString name;
        QString title;
        Kind kind = Kind::Folder;
        QStringList exec;
        QString text;
    };

    World(const QList<Door>& doors, const QList<Node>& home);

    // "Tux Paint" is tux-paint; accents become plain letters.
    static QString slug(const QString& title);
    static QString parentOf(const QString& path);
    static QString join(const QString& folder, const QString& name);
    static QString folderName(Kind kind);
    static bool isDoor(const Node& node);

    QList<Node> children(const QString& folder) const;
    std::optional<Node> find(const QString& path) const;
    // The first node with this name anywhere, for "draw is in make".
    std::optional<Node> findByName(const QString& name) const;

private:
    void add(const Node& node);

    QList<Node> m_nodes;
};
