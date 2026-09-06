// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "HomeLayout.h"
#include "World.h"

// The dev-PC doors and the built-in home for a child called Sam.
inline QList<World::Door> fixtureDoors() {
    return {
        {.title = QStringLiteral("Draw"),
         .kind = World::Kind::Make,
         .exec = {QStringLiteral("tuxpaint")}},
        {.title = QStringLiteral("Music"), .kind = World::Kind::Make, .exec = {}},
        {.title = QStringLiteral("Tux Paint"),
         .kind = World::Kind::Make,
         .exec = {QStringLiteral("tuxpaint")}},
        {.title = QStringLiteral("Practice"),
         .kind = World::Kind::Practice,
         .exec = {QStringLiteral("gcompris-qt")}},
    };
}

inline World fixtureWorld() {
    return {fixtureDoors(), HomeLayout::builtIn(QStringLiteral("Sam"))};
}
