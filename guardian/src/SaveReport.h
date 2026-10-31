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
        // Steam's tools are being taken from the Guardian's own Steam into
        // the tools folder (ADR-0046).
        SharingTools,
        // Writing the children's lists.
        Writing,
        // Copying one child's new games.
        Copying,
        // Copying Steam's tools into one child's Steam, after their games.
        CopyingTools,
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
        // How many of Steam's tools the copy gave them.
        int toolsCopied = 0;
        // They may lack a tool: one was left out, another tool runs in one
        // that neither they nor the tools folder had, or the folder could
        // not be read.
        bool toolsMissing = false;
        // The tools folder held no tool to copy.
        bool noTools = false;
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
    // Whether some tool of the Guardian's Steam stayed out of the tools
    // folder: the tools step did not finish, or left a tool out.
    bool toolsLeftOut() const;
    // Whether any child's copy got as far as a game or a tool, so a Save's
    // word about the tools is about something a child was given.
    bool copiedForSomeone() const;
    // Each child the command said anything about, in the order it did.
    QList<Child> children() const;
    // Each game copied, or already there, in the order the command said so.
    QList<Copied> copied() const;

private:
    Child& child(const QString& login);
    void takeMade(const QString& what, bool kept);
    bool takeTool(const QString& line);

    QSet<QString> m_logins;
    Stage m_stage = Stage::Starting;
    QString m_current;
    std::optional<bool> m_picturesKept;
    std::optional<bool> m_catalogueKept;
    bool m_toolsKept = false;
    bool m_toolsNotShared = false;
    bool m_copiedForSomeone = false;
    QList<Child> m_children;
    QList<Copied> m_copied;
};
