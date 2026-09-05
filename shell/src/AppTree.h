// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include <cstdint>
#include <optional>

// What the restricted shell can see at L1: not the filesystem, but the same
// programs the launcher's tiles come from, in one folder per kind
// (ADR-0013). Names are lowercase slugs of the titles, so "Tux Paint" is
// tux-paint, and a repeated name gets a number: draw, draw-2.
class AppTree {
public:
    enum class Kind : std::uint8_t { Make, Practice, Machine };

    struct App {
        QString title;
        Kind kind;
        QStringList exec;
    };

    struct Entry {
        QString name;
        QString title;
        Kind kind;
        QStringList exec;
    };

    explicit AppTree(const QList<App>& apps);

    static QString slug(const QString& title);
    static QString folderName(Kind kind);
    static std::optional<Kind> folderKind(const QString& name);

    // Folder names at the root, in kind order, only for kinds that have apps.
    QStringList folders() const;
    QList<Entry> entries(Kind kind) const;
    std::optional<Entry> find(Kind kind, const QString& name) const;
    // The entry with this name in any folder, for "draw is in make".
    std::optional<Entry> findAnywhere(const QString& name) const;

private:
    QList<Entry> m_entries;
};
