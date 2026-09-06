// SPDX-License-Identifier: Apache-2.0
#include "Interpreter.h"

#include "Completion.h"
#include "Hints.h"

#include <utility>

namespace {

Reply::Icon iconFor(World::Kind kind) {
    switch (kind) {
    case World::Kind::Folder:
        return Reply::Icon::Folder;
    case World::Kind::Make:
        return Reply::Icon::Make;
    case World::Kind::Practice:
        return Reply::Icon::Practice;
    case World::Kind::Machine:
        return Reply::Icon::Machine;
    case World::Kind::Note:
        return Reply::Icon::Note;
    }
    return Reply::Icon::None;
}

} // namespace

Interpreter::Interpreter(World world) : m_world(std::move(world)) {}

QStringList Interpreter::commands() {
    return {QStringLiteral("ls"),  QStringLiteral("cd"),   QStringLiteral("open"),
            QStringLiteral("cat"), QStringLiteral("help"), QStringLiteral("exit")};
}

QString Interpreter::location() const {
    return QStringLiteral("/") + m_location;
}

Reply Interpreter::run(const QString& line) {
    QStringList words = line.toLower().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (words.isEmpty()) {
        return {};
    }
    const QString command = words.takeFirst();
    if (words.size() > 1) {
        return Hints::oneNameAtATime(command);
    }
    if (command == QStringLiteral("ls")) {
        return ls(words);
    }
    if (command == QStringLiteral("cd")) {
        return cd(words);
    }
    if (command == QStringLiteral("open")) {
        return open(words);
    }
    if (command == QStringLiteral("cat")) {
        return cat(words);
    }
    if (command == QStringLiteral("help")) {
        return help(words);
    }
    if (command == QStringLiteral("exit")) {
        return Reply{.lines = {}, .launch = std::nullopt, .leave = true};
    }
    return Hints::unknownCommand(command, commands());
}

QStringList Interpreter::complete(const QString& line) const {
    return Completion::candidates(line, commands(), namesHere());
}

Reply Interpreter::ls(const QStringList& args) {
    if (args.isEmpty()) {
        return listing(m_location);
    }
    const std::optional<World::Node> node = here(args.first());
    if (!node) {
        return notFound(args.first());
    }
    if (node->kind == World::Kind::Folder) {
        return listing(node->path);
    }
    if (node->kind == World::Kind::Note) {
        return Hints::isANote(node->name);
    }
    return Hints::isAThingToOpen(node->name);
}

Reply Interpreter::cd(const QStringList& args) {
    if (args.isEmpty() || args.first() == QStringLiteral("/")) {
        m_location.clear();
        return {};
    }
    if (args.first() == QStringLiteral("..")) {
        m_location = World::parentOf(m_location);
        return {};
    }
    const std::optional<World::Node> node = here(args.first());
    if (!node) {
        return notFound(args.first());
    }
    if (node->kind == World::Kind::Folder) {
        m_location = node->path;
        return {};
    }
    if (node->kind == World::Kind::Note) {
        return Hints::isANote(node->name);
    }
    return Hints::isAThingToOpen(node->name);
}

Reply Interpreter::open(const QStringList& args) {
    if (args.isEmpty()) {
        return Hints::needsAName(QStringLiteral("open"));
    }
    const std::optional<World::Node> node = here(args.first());
    if (!node) {
        return notFound(args.first());
    }
    if (node->kind == World::Kind::Folder) {
        return Hints::isAPlace(node->name);
    }
    if (node->kind == World::Kind::Note) {
        return Hints::say(node->text);
    }
    Reply reply = Hints::say(tr("Opening %1.").arg(node->title));
    reply.launch = Reply::Launch{.title = node->title, .exec = node->exec};
    return reply;
}

Reply Interpreter::cat(const QStringList& args) {
    if (args.isEmpty()) {
        return Hints::needsAName(QStringLiteral("cat"));
    }
    const std::optional<World::Node> node = here(args.first());
    if (!node) {
        return notFound(args.first());
    }
    if (node->kind == World::Kind::Folder) {
        return Hints::isAPlace(node->name);
    }
    if (World::isDoor(*node)) {
        return Hints::isNotForReading(node->name);
    }
    return Hints::say(node->text);
}

Reply Interpreter::help(const QStringList& args) {
    const QList<std::pair<QString, QString>> lines = {
        {QStringLiteral("ls"), tr("ls shows what is here.")},
        {QStringLiteral("cd"), tr("cd goes into a place. cd .. comes back.")},
        {QStringLiteral("open"), tr("open starts a thing.")},
        {QStringLiteral("cat"), tr("cat reads a note.")},
        {QStringLiteral("help"), tr("help shows this.")},
        {QStringLiteral("exit"), tr("exit goes back to the tiles.")},
    };
    Reply reply;
    for (const auto& [command, text] : lines) {
        if (args.isEmpty() || args.first() == command) {
            reply.lines.append({.text = text, .icon = Reply::Icon::None});
        }
    }
    if (reply.lines.isEmpty()) {
        return Hints::unknownCommand(args.first(), commands());
    }
    return reply;
}

Reply Interpreter::listing(const QString& folder) const {
    Reply reply;
    for (const World::Node& node : m_world.children(folder)) {
        reply.lines.append({.text = node.name, .icon = iconFor(node.kind)});
    }
    if (reply.lines.isEmpty()) {
        return Hints::nothingHere();
    }
    return reply;
}

Reply Interpreter::notFound(const QString& name) const {
    return Hints::notFound(name, m_world, m_location, namesHere());
}

QStringList Interpreter::namesHere() const {
    QStringList names;
    for (const World::Node& node : m_world.children(m_location)) {
        names.append(node.name);
    }
    return names;
}

std::optional<World::Node> Interpreter::here(const QString& name) const {
    if (name.contains(QLatin1Char('/'))) {
        return std::nullopt;
    }
    return m_world.find(World::join(m_location, name));
}
