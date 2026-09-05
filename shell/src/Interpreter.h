// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "AppTree.h"
#include "Reply.h"

#include <QObject>
#include <QString>
#include <QStringList>

#include <optional>

// The restricted shell's five commands over the app tree: ls, cd, open, cat,
// help. Every answer is a Reply; a mistake gets a suggestion, never a
// scolding, an errno or an exit code. Nothing here touches the filesystem or
// runs anything: open only names a program for the host to start.
class Interpreter : public QObject {
    Q_OBJECT

public:
    explicit Interpreter(AppTree tree, QObject* parent = nullptr);

    static QStringList commands();

    // "/" at the root, "/make" inside a folder. For the prompt.
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

    Reply listing(std::optional<AppTree::Kind> folder) const;
    Reply notFound(const QString& name) const;
    QStringList namesHere() const;
    std::optional<AppTree::Entry> entryHere(const QString& name) const;

    AppTree m_tree;
    std::optional<AppTree::Kind> m_folder;
};
