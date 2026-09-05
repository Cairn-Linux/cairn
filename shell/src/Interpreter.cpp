// SPDX-License-Identifier: Apache-2.0
#include "Interpreter.h"

#include "Completion.h"
#include "Hints.h"

#include <utility>

namespace {

Reply::Icon iconFor(AppTree::Kind kind) {
    switch (kind) {
    case AppTree::Kind::Make:
        return Reply::Icon::Make;
    case AppTree::Kind::Practice:
        return Reply::Icon::Practice;
    case AppTree::Kind::Machine:
        return Reply::Icon::Machine;
    }
    return Reply::Icon::None;
}

} // namespace

Interpreter::Interpreter(AppTree tree, QObject* parent)
    : QObject(parent), m_tree(std::move(tree)) {}

QStringList Interpreter::commands() {
    return {QStringLiteral("ls"), QStringLiteral("cd"), QStringLiteral("open"),
            QStringLiteral("cat"), QStringLiteral("help")};
}

QString Interpreter::location() const {
    if (!m_folder) {
        return QStringLiteral("/");
    }
    return QStringLiteral("/") + AppTree::folderName(*m_folder);
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
    return Hints::unknownCommand(command, commands());
}

QStringList Interpreter::complete(const QString& line) const {
    return Completion::candidates(line, commands(), namesHere());
}

Reply Interpreter::ls(const QStringList& args) {
    if (args.isEmpty()) {
        return listing(m_folder);
    }
    const std::optional<AppTree::Kind> kind = AppTree::folderKind(args.first());
    if (!m_folder && kind && !m_tree.entries(*kind).isEmpty()) {
        return listing(kind);
    }
    if (entryHere(args.first())) {
        return Hints::isAThingToOpen(args.first());
    }
    return notFound(args.first());
}

Reply Interpreter::cd(const QStringList& args) {
    if (args.isEmpty() || args.first() == QStringLiteral("/") ||
        args.first() == QStringLiteral("..")) {
        m_folder = std::nullopt;
        return {};
    }
    if (m_folder) {
        if (entryHere(args.first())) {
            return Hints::isAThingToOpen(args.first());
        }
        if (AppTree::folderKind(args.first())) {
            return Hints::isNextDoor(args.first());
        }
        return notFound(args.first());
    }
    const std::optional<AppTree::Kind> kind = AppTree::folderKind(args.first());
    if (kind && !m_tree.entries(*kind).isEmpty()) {
        m_folder = kind;
        return {};
    }
    return notFound(args.first());
}

Reply Interpreter::open(const QStringList& args) {
    if (args.isEmpty()) {
        return Hints::needsAName(QStringLiteral("open"));
    }
    if (!m_folder && AppTree::folderKind(args.first())) {
        return Hints::isAPlace(args.first());
    }
    const std::optional<AppTree::Entry> entry = entryHere(args.first());
    if (!entry) {
        return notFound(args.first());
    }
    Reply reply = Hints::say(tr("Opening %1.").arg(entry->title));
    reply.launch = Reply::Launch{.title = entry->title, .exec = entry->exec};
    return reply;
}

Reply Interpreter::cat(const QStringList& args) {
    if (args.isEmpty()) {
        return Hints::needsAName(QStringLiteral("cat"));
    }
    if (!m_folder && AppTree::folderKind(args.first())) {
        return Hints::isAPlace(args.first());
    }
    if (entryHere(args.first())) {
        return Hints::isNotForReading(args.first());
    }
    return notFound(args.first());
}

Reply Interpreter::help(const QStringList& args) {
    const QList<std::pair<QString, QString>> lines = {
        {QStringLiteral("ls"), tr("ls shows what is here.")},
        {QStringLiteral("cd"), tr("cd goes into a place. cd .. comes back.")},
        {QStringLiteral("open"), tr("open starts a thing.")},
        {QStringLiteral("cat"), tr("cat reads a thing.")},
        {QStringLiteral("help"), tr("help shows this.")},
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

Reply Interpreter::listing(std::optional<AppTree::Kind> folder) const {
    Reply reply;
    if (!folder) {
        for (const QString& name : m_tree.folders()) {
            reply.lines.append({.text = name, .icon = Reply::Icon::Folder});
        }
        if (reply.lines.isEmpty()) {
            return Hints::nothingHere();
        }
        return reply;
    }
    for (const AppTree::Entry& entry : m_tree.entries(*folder)) {
        reply.lines.append({.text = entry.name, .icon = iconFor(entry.kind)});
    }
    return reply;
}

Reply Interpreter::notFound(const QString& name) const {
    return Hints::notFound(name, m_tree, m_folder, namesHere());
}

QStringList Interpreter::namesHere() const {
    if (!m_folder) {
        return m_tree.folders();
    }
    QStringList names;
    for (const AppTree::Entry& entry : m_tree.entries(*m_folder)) {
        names.append(entry.name);
    }
    return names;
}

std::optional<AppTree::Entry> Interpreter::entryHere(const QString& name) const {
    if (!m_folder) {
        return std::nullopt;
    }
    return m_tree.find(*m_folder, name);
}
