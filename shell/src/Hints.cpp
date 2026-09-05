// SPDX-License-Identifier: Apache-2.0
#include "Hints.h"

#include "DidYouMean.h"

Reply Hints::say(const QString& text) {
    Reply reply;
    reply.lines.append({.text = text, .icon = Reply::Icon::None});
    return reply;
}

Reply Hints::unknownCommand(const QString& word, const QStringList& commands) {
    if (const std::optional<QString> meant = DidYouMean::closest(word, commands)) {
        return say(tr("I don't know \"%1\". Did you mean %2?").arg(word, *meant));
    }
    return say(tr("I don't know \"%1\". Type help to see what I know.").arg(word));
}

Reply Hints::notFound(const QString& name, const AppTree& tree, std::optional<AppTree::Kind> folder,
                      const QStringList& here) {
    if (const std::optional<AppTree::Entry> elsewhere = tree.findAnywhere(name)) {
        const QString target = AppTree::folderName(elsewhere->kind);
        if (folder) {
            return say(tr("%1 is in %2. Type cd .. and then cd %2.").arg(name, target));
        }
        return say(tr("%1 is in %2. Type cd %2 first.").arg(name, target));
    }
    if (const std::optional<QString> meant = DidYouMean::closest(name, here)) {
        return say(tr("I can't find \"%1\". Did you mean %2?").arg(name, *meant));
    }
    return say(tr("I can't find \"%1\" here. Type ls to see what is here.").arg(name));
}

Reply Hints::isAPlace(const QString& name) {
    return say(tr("%1 is a place. Type cd %1 to go there.").arg(name));
}

Reply Hints::isNextDoor(const QString& name) {
    return say(tr("%1 is next door. Type cd .. first.").arg(name));
}

Reply Hints::isAThingToOpen(const QString& name) {
    return say(tr("%1 is a thing to open, not a place. Type open %1.").arg(name));
}

Reply Hints::isNotForReading(const QString& name) {
    return say(tr("%1 is a thing to open, not to read. Type open %1.").arg(name));
}

Reply Hints::needsAName(const QString& command) {
    return say(tr("%1 needs a name. Type ls to see them.").arg(command));
}

Reply Hints::oneNameAtATime(const QString& command) {
    return say(tr("%1 takes one name at a time.").arg(command));
}

Reply Hints::nothingHere() {
    return say(tr("Nothing here yet."));
}
