// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include <cstdint>
#include <optional>

// What the interpreter answers to one line. Lines are drawn by the surface,
// each with an icon slot so a pre-reader can tell a folder from a thing to
// open; a launch is carried out by whoever hosts the shell, never here.
struct Reply {
    enum class Icon : std::uint8_t { None, Folder, Make, Practice, Machine };

    struct Line {
        QString text;
        Icon icon = Icon::None;
    };

    struct Launch {
        QString title;
        QStringList exec;
    };

    QList<Line> lines;
    std::optional<Launch> launch;
};
