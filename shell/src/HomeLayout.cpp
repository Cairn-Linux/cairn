// SPDX-License-Identifier: Apache-2.0
#include "HomeLayout.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

namespace {
constexpr int supportedVersion = 1;
}

QList<World::Node> HomeLayout::builtIn(const QString& childName) {
    const QString me = childFolder(childName);
    return {
        folder(me),
        note(World::join(me, QStringLiteral("hello")),
             tr("Hello, %1. This is your computer. Type ls to see what is here.").arg(childName)),
        note(World::join(me, QStringLiteral("words")),
             tr("ls shows what is here. cd goes into a place. cd .. comes back. open starts "
                "a thing. cat reads a note. help shows the words. exit goes back to the "
                "tiles.")),
        folder(World::join(me, QStringLiteral("notes"))),
        note(World::join(me, QStringLiteral("notes/cairn")),
             tr("A cairn is a small pile of stones that shows the way.")),
        note(World::join(me, QStringLiteral("notes/trail")),
             tr("Someone walked this trail before you and left a marker so you could find "
                "the way.")),
        folder(World::join(me, QStringLiteral("pictures"))),
    };
}

HomeLayout::Result HomeLayout::read(const QString& path, const QString& childName) {
    const QUrl url(path);
    QFile file(url.isLocalFile() ? url.toLocalFile() : path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {.nodes = {}, .error = tr("The home layout %1 could not be read.").arg(path)};
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return {.nodes = {}, .error = tr("The home layout %1 is not valid JSON.").arg(path)};
    }
    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("version")).toInt() != supportedVersion) {
        return {.nodes = {},
                .error =
                    tr("The home layout %1 is not version %2.").arg(path).arg(supportedVersion)};
    }
    const QString me = childFolder(childName);
    Result result;
    result.nodes.append(folder(me));
    for (const QJsonValue value : root.value(QStringLiteral("entries")).toArray()) {
        const QJsonObject entry = value.toObject();
        const QString relative = entry.value(QStringLiteral("path")).toString();
        if (relative.isEmpty() || relative.startsWith(QLatin1Char('/')) ||
            relative.contains(QStringLiteral(".."))) {
            return {.nodes = {},
                    .error = tr("The home layout %1 has an entry with a bad path.").arg(path)};
        }
        const QString full = World::join(me, relative);
        if (entry.contains(QStringLiteral("text"))) {
            result.nodes.append(note(full, entry.value(QStringLiteral("text")).toString()));
        } else {
            result.nodes.append(folder(full));
        }
    }
    return result;
}

QString HomeLayout::childFolder(const QString& childName) {
    QString name = World::slug(childName);
    if (name.isEmpty()) {
        name = QStringLiteral("me");
    }
    return World::join(QStringLiteral("home"), name);
}

World::Node HomeLayout::folder(const QString& path) {
    const QString parent = World::parentOf(path);
    const QString name = path.mid(parent.isEmpty() ? 0 : parent.size() + 1);
    return {.path = path,
            .name = name,
            .title = name,
            .kind = World::Kind::Folder,
            .exec = {},
            .text = {}};
}

World::Node HomeLayout::note(const QString& path, const QString& text) {
    World::Node node = folder(path);
    node.kind = World::Kind::Note;
    node.text = text;
    return node;
}
