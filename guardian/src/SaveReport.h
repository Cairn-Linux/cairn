// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

#include <cstdint>
#include <optional>

// What cairn-family save-games has said so far in one Save (ADR-0041),
// read from its lines as they arrive: how far it has got, and each child's
// result. Only the fixed words, and the logins the Save named, are taken;
// any other line is left out, so nothing else the command printed reaches
// the page.
class SaveReport {
public:
    enum class Stage : std::uint8_t {
        // Nothing said yet: the password box, or the command's first step.
        Starting,
        // The password was accepted, and the pictures and the full games
        // list are being made again.
        Looking,
        // Writing the children's lists.
        Writing,
        // Copying one child's new games.
        Copying,
        // Waiting for one child's last login to end.
        Waiting,
    };

    struct Skip {
        QString appId;
        // The reason word, such as not-shared or downloading.
        QString reason;
    };

    struct Child {
        QString login;
        // Set once the child's line came: their list is written.
        std::optional<int> added;
        std::optional<int> removed;
        // Why their new games are not all given, such as logged-in.
        std::optional<QString> held;
        QList<Skip> skipped;
    };

    // One game the copy gave a child, or found they had already.
    struct Copied {
        QString login;
        QString appId;
        // The child's Steam had it already: `have`, not `copied`.
        bool had = false;
    };

    // The logins the Save names; lines about anyone else are left out.
    explicit SaveReport(const QStringList& logins = {});

    void take(const QString& line);

    Stage stage() const;
    // The child being copied or waited for, or "".
    QString current() const;
    // Whether the command said the pictures or the full games list stay as
    // they were. Nothing until it said either way.
    std::optional<bool> picturesKept() const;
    std::optional<bool> catalogueKept() const;
    // Each child the command said anything about, in the order it did.
    QList<Child> children() const;
    // Each game copied, or already there, in the order the command said so.
    QList<Copied> copied() const;

private:
    Child& child(const QString& login);

    QSet<QString> m_logins;
    Stage m_stage = Stage::Starting;
    QString m_current;
    std::optional<bool> m_picturesKept;
    std::optional<bool> m_catalogueKept;
    QList<Child> m_children;
    QList<Copied> m_copied;
};
