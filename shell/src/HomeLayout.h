// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "World.h"

#include <QCoreApplication>
#include <QList>
#include <QString>

// The small fictional computer under home/<name> (TERMINAL-DESIGN §3): a
// few folders and notes laid out like a real Linux home, authored, never
// changed by the child. Built in, or read from a version-1 JSON file:
// {"version": 1, "entries": [{"path": "notes"}, {"path": "notes/hello",
// "text": "..."}]}. An entry with text is a note; without, a folder.
class HomeLayout {
    Q_DECLARE_TR_FUNCTIONS(HomeLayout)

public:
    struct Result {
        QList<World::Node> nodes;
        QString error;
    };

    static QList<World::Node> builtIn(const QString& childName);
    // Errors come back as a plain sentence, never an exception; the caller
    // falls back to the built-in layout.
    static Result read(const QString& path, const QString& childName);

private:
    static QString childFolder(const QString& childName);
    static World::Node folder(const QString& path);
    static World::Node note(const QString& path, const QString& text);
};
