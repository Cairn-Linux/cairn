// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "AppTree.h"

// The six dev-PC tiles, as the tests see them.
inline AppTree fixtureTree() {
    return AppTree({
        {.title = QStringLiteral("Draw"),
         .kind = AppTree::Kind::Make,
         .exec = {QStringLiteral("tuxpaint")}},
        {.title = QStringLiteral("Music"), .kind = AppTree::Kind::Make, .exec = {}},
        {.title = QStringLiteral("Tux Paint"),
         .kind = AppTree::Kind::Make,
         .exec = {QStringLiteral("tuxpaint")}},
        {.title = QStringLiteral("Practice"),
         .kind = AppTree::Kind::Practice,
         .exec = {QStringLiteral("gcompris-qt")}},
        {.title = QStringLiteral("Terminal"), .kind = AppTree::Kind::Machine, .exec = {}},
    });
}
