// SPDX-License-Identifier: Apache-2.0
#include "KeyValues.h"

#include <QFile>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

namespace {

// Steam nests its blocks three or four deep. A limit keeps a file of
// nothing but opening braces from using up the stack when it is freed.
constexpr qsizetype kDeepest = 32;

// Where the line that `at` is on ends.
qsizetype lineEnd(QStringView text, qsizetype at) {
    const qsizetype end = text.indexOf(u'\n', at);
    return end < 0 ? text.size() : end;
}

// The string whose opening quote is at `at`, which then moves past its
// closing quote, or nothing when it never closes. A backslash keeps the
// character after it, whatever it is.
std::optional<QString> quoted(QStringView text, qsizetype& at) {
    QString word;
    ++at;
    while (at < text.size() && text.at(at) != u'"') {
        if (text.at(at) == u'\\' && at + 1 < text.size()) {
            ++at;
        }
        word.append(text.at(at));
        ++at;
    }
    if (at >= text.size()) {
        return std::nullopt;
    }
    ++at;
    return word;
}

} // namespace

std::optional<KeyValues> KeyValues::parse(QStringView text) {
    // The blocks still open, the outermost first. Each is moved into the
    // one around it when its brace closes.
    std::vector<Node> open(1);
    std::optional<QString> pendingKey;
    qsizetype at = 0;
    while (at < text.size()) {
        const QChar c = text.at(at);
        if (text.sliced(at).startsWith(u"//")) {
            at = lineEnd(text, at);
        } else if (c == u'"') {
            std::optional<QString> word = quoted(text, at);
            if (!word) {
                return std::nullopt;
            }
            if (!pendingKey) {
                pendingKey = std::move(word);
            } else {
                open.back().children.push_back(
                    {.key = *pendingKey, .value = std::move(word), .children = {}});
                pendingKey.reset();
            }
        } else if (c == u'{' || c == u'}') {
            if (!brace(c, open, pendingKey)) {
                return std::nullopt;
            }
            ++at;
        } else {
            ++at;
        }
    }
    if (open.size() != 1) {
        return std::nullopt;
    }
    KeyValues parsed;
    parsed.m_root = std::move(open.front());
    return parsed;
}

bool KeyValues::brace(QChar brace, std::vector<Node>& open, std::optional<QString>& pendingKey) {
    if (brace == u'{') {
        if (!pendingKey || std::ssize(open) > kDeepest) {
            return false;
        }
        open.push_back({.key = *pendingKey, .value = std::nullopt, .children = {}});
        pendingKey.reset();
        return true;
    }
    if (open.size() == 1) {
        return false;
    }
    Node closed = std::move(open.back());
    open.pop_back();
    open.back().children.push_back(std::move(closed));
    return true;
}

std::optional<KeyValues> KeyValues::readFile(const QString& path) {
    const int descriptor =
        ::open(QFile::encodeName(path).constData(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    if (descriptor < 0) {
        return std::nullopt;
    }
    struct stat details{};
    if (::fstat(descriptor, &details) != 0 || !S_ISREG(details.st_mode) ||
        details.st_size > kFileLimit) {
        ::close(descriptor);
        return std::nullopt;
    }
    // The QFile closes the descriptor from here on.
    QFile file;
    if (!file.open(descriptor, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle)) {
        ::close(descriptor);
        return std::nullopt;
    }
    // The file may have grown since it was measured.
    const QByteArray bytes = file.read(kFileLimit + 1);
    if (bytes.size() > kFileLimit) {
        return std::nullopt;
    }
    return parse(QString::fromUtf8(bytes));
}

std::optional<QString> KeyValues::value(const QStringList& path) const {
    const Node* node = find(path);
    return node != nullptr ? node->value : std::nullopt;
}

QStringList KeyValues::keys(const QStringList& path) const {
    const Node* node = find(path);
    QStringList found;
    if (node == nullptr || node->value) {
        return found;
    }
    for (const Node& child : node->children) {
        if (!found.contains(child.key)) {
            found.append(child.key);
        }
    }
    return found;
}

// The first key that matches without case names the entry, and the last
// entry with exactly that key holds its value: kidscan's lookup in
// Python's dictionary, where a key given again replaces the first.
const KeyValues::Node* KeyValues::childOf(const Node& block, const QString& key) {
    const Node* named = nullptr;
    for (const Node& child : block.children) {
        const bool first = named == nullptr && child.key.compare(key, Qt::CaseInsensitive) == 0;
        const bool again = named != nullptr && child.key == named->key;
        if (first || again) {
            named = &child;
        }
    }
    return named;
}

const KeyValues::Node* KeyValues::find(const QStringList& path) const {
    const Node* node = &m_root;
    for (const QString& key : path) {
        if (node->value) {
            return nullptr;
        }
        node = childOf(*node, key);
        if (node == nullptr) {
            return nullptr;
        }
    }
    return node;
}
