// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QChar>
#include <QString>
#include <QStringList>
#include <QStringView>

#include <optional>
#include <vector>

// Valve's KeyValues text, which Steam writes in each library's
// appmanifest_*.acf and libraryfolders.vdf, read only as far as kidscan
// reads it (tools/kidscan): quoted keys and values, blocks in braces and
// // comments. Anything else outside quotes is passed over, as kidscan
// does. A string that never closes, a brace that does not match, or
// blocks nested deeper than Steam ever writes them make the whole text
// unreadable, never a crash.
class KeyValues {
public:
    // Steam's own files are a few kilobytes; kidscan stops at the same size.
    static constexpr qint64 kFileLimit = 1 << 20;

    // The text, or nothing when it is not KeyValues as above.
    static std::optional<KeyValues> parse(QStringView text);
    // The file's text parsed, or nothing when it is not a plain file of at
    // most kFileLimit bytes, cannot be read, or does not parse. A link or
    // a named pipe is never followed or waited on.
    static std::optional<KeyValues> readFile(const QString& path);

    // The value at this path of keys, such as {"AppState", "name"}. Keys
    // match without regard to case, because Steam is not consistent about
    // it, and a key given twice keeps its last value, as in kidscan.
    // Nothing when there is no such key or it holds a block.
    std::optional<QString> value(const QStringList& path) const;
    // The keys in the block at this path, in the file's order, each once;
    // none when it is not a block.
    QStringList keys(const QStringList& path) const;

private:
    // A block holds blocks, so Node's copies and moves call themselves;
    // parse() keeps the depth within kDeepest.
    struct Node { // NOLINT(misc-no-recursion)
        QString key;
        // A value, or nothing for a block.
        std::optional<QString> value;
        std::vector<Node> children;
    };

    // Opens or closes a block, or says the brace is out of place.
    static bool brace(QChar brace, std::vector<Node>& open, std::optional<QString>& pendingKey);
    static const Node* childOf(const Node& block, const QString& key);
    const Node* find(const QStringList& path) const;

    Node m_root;
};
