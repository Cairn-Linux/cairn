// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "Reply.h"
#include "World.h"

#include <QCoreApplication>
#include <QString>
#include <QStringList>

#include <optional>

// The restricted shell's six words over the world: ls, cd, open, cat, help,
// exit. Every answer is a Reply; a mistake gets a suggestion, never a
// scolding, an errno or an exit code. Nothing here touches the filesystem or
// runs anything: open only names a program for the host to start.
class Interpreter {
    Q_DECLARE_TR_FUNCTIONS(Interpreter)

public:
    explicit Interpreter(World world);

    static QStringList commands();

    // "/" at the root, "/home/sam/notes" inside. For the prompt.
    QString location() const;

    Reply run(const QString& line);

    // What the line could become: command names before the first space,
    // then names in the current folder. Sorted, for the surface's hinting.
    QStringList complete(const QString& line) const;

private:
    Reply ls(const QStringList& args);
    Reply cd(const QStringList& args);
    Reply open(const QStringList& args);
    Reply cat(const QStringList& args);
    static Reply help(const QStringList& args);

    Reply listing(const QString& folder) const;
    Reply notFound(const QString& name) const;
    QStringList namesHere() const;
    std::optional<World::Node> here(const QString& name) const;

    World m_world;
    QString m_location;
};
