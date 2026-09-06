// SPDX-License-Identifier: Apache-2.0
#include "World.h"

#include <QChar>

#include <algorithm>
#include <array>

namespace {

constexpr std::array<World::Kind, 3> doorKinds = {World::Kind::Make, World::Kind::Practice,
                                                  World::Kind::Machine};

} // namespace

World::World(const QList<Door>& doors, const QList<Node>& home) {
    for (const Kind kind : doorKinds) {
        const QString folder = folderName(kind);
        bool any = false;
        for (const Door& door : doors) {
            if (door.kind != kind) {
                continue;
            }
            if (!any) {
                add({.path = folder,
                     .name = folder,
                     .title = folder,
                     .kind = Kind::Folder,
                     .exec = {},
                     .text = {}});
                any = true;
            }
            QString name = slug(door.title);
            if (name.isEmpty()) {
                name = QStringLiteral("thing");
            }
            // draw, draw-2, draw-3: the same word twice would make ls ambiguous.
            const QString base = name;
            for (int suffix = 2; find(join(folder, name)); ++suffix) {
                name = QStringLiteral("%1-%2").arg(base).arg(suffix);
            }
            add({.path = join(folder, name),
                 .name = name,
                 .title = door.title,
                 .kind = door.kind,
                 .exec = door.exec,
                 .text = {}});
        }
    }
    if (!home.isEmpty()) {
        add({.path = QStringLiteral("home"),
             .name = QStringLiteral("home"),
             .title = QStringLiteral("home"),
             .kind = Kind::Folder,
             .exec = {},
             .text = {}});
        for (const Node& node : home) {
            add(node);
        }
    }
}

QString World::slug(const QString& title) {
    QString out;
    bool pendingDash = false;
    // Decomposed, an accented letter is the plain letter plus a mark, so
    // "Café" keeps its e and only the mark is dropped.
    const QString plain = title.toLower().normalized(QString::NormalizationForm_D);
    for (const QChar character : plain) {
        if (character.isLetterOrNumber() && character.unicode() < 128) {
            if (pendingDash && !out.isEmpty()) {
                out.append(QLatin1Char('-'));
            }
            pendingDash = false;
            out.append(character);
        } else if (!character.isMark()) {
            pendingDash = true;
        }
    }
    return out;
}

QString World::parentOf(const QString& path) {
    const qsizetype slash = path.lastIndexOf(QLatin1Char('/'));
    return slash < 0 ? QString() : path.left(slash);
}

QString World::join(const QString& folder, const QString& name) {
    return folder.isEmpty() ? name : folder + QLatin1Char('/') + name;
}

QString World::folderName(Kind kind) {
    switch (kind) {
    case Kind::Make:
        return QStringLiteral("make");
    case Kind::Practice:
        return QStringLiteral("practice");
    case Kind::Machine:
        return QStringLiteral("machine");
    case Kind::Folder:
    case Kind::Note:
        break;
    }
    return {};
}

bool World::isDoor(const Node& node) {
    return node.kind == Kind::Make || node.kind == Kind::Practice || node.kind == Kind::Machine;
}

QList<World::Node> World::children(const QString& folder) const {
    QList<Node> inFolder;
    for (const Node& node : m_nodes) {
        if (parentOf(node.path) == folder && (!folder.isEmpty() || !node.path.isEmpty())) {
            inFolder.append(node);
        }
    }
    return inFolder;
}

std::optional<World::Node> World::find(const QString& path) const {
    for (const Node& node : m_nodes) {
        if (node.path == path) {
            return node;
        }
    }
    return std::nullopt;
}

std::optional<World::Node> World::findByName(const QString& name) const {
    for (const Node& node : m_nodes) {
        if (node.name == name) {
            return node;
        }
    }
    return std::nullopt;
}

void World::add(const Node& node) {
    if (node.path.isEmpty() || find(node.path)) {
        return;
    }
    m_nodes.append(node);
}
