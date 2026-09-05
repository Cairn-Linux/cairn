// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "AppTree.h"
#include "Reply.h"

#include <QCoreApplication>
#include <QString>
#include <QStringList>

#include <optional>

// The sentences the shell says after a mistake. Each suggests the next thing
// to type and never assigns blame; the wording is written up in
// shell/README.md for child testing (DESIGN §14 Q2).
class Hints {
    Q_DECLARE_TR_FUNCTIONS(Hints)

public:
    static Reply say(const QString& text);

    static Reply unknownCommand(const QString& word, const QStringList& commands);
    // `folder` is where the child is; `here` are the names in it.
    static Reply notFound(const QString& name, const AppTree& tree,
                          std::optional<AppTree::Kind> folder, const QStringList& here);
    static Reply isAPlace(const QString& name);
    static Reply isNextDoor(const QString& name);
    static Reply isAThingToOpen(const QString& name);
    static Reply isNotForReading(const QString& name);
    static Reply needsAName(const QString& command);
    static Reply oneNameAtATime(const QString& command);
    static Reply nothingHere();
};
