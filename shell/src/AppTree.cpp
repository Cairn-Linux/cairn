// SPDX-License-Identifier: Apache-2.0
#include "AppTree.h"

#include <QChar>

#include <algorithm>
#include <array>

namespace {

constexpr std::array<AppTree::Kind, 3> kindOrder = {AppTree::Kind::Make, AppTree::Kind::Practice,
                                                    AppTree::Kind::Machine};

bool anyNamed(const QList<AppTree::Entry>& entries, const QString& name) {
    return std::ranges::any_of(entries,
                               [&name](const AppTree::Entry& entry) { return entry.name == name; });
}

} // namespace

AppTree::AppTree(const QList<App>& apps) {
    for (const App& app : apps) {
        QString name = slug(app.title);
        if (name.isEmpty()) {
            name = QStringLiteral("thing");
        }
        // draw, draw-2, draw-3: the same word twice would make ls ambiguous.
        const QString base = name;
        for (int suffix = 2; anyNamed(m_entries, name); ++suffix) {
            name = QStringLiteral("%1-%2").arg(base).arg(suffix);
        }
        m_entries.append({.name = name, .title = app.title, .kind = app.kind, .exec = app.exec});
    }
}

QString AppTree::slug(const QString& title) {
    QString out;
    bool pendingDash = false;
    // Decomposed, an accented letter is the plain letter plus a mark, so
    // "Café" keeps its e and only the mark is dropped.
    const QString plain = title.toLower().normalized(QString::NormalizationForm_D);
    for (const QChar character : plain) {
        if (character.isLetterOrNumber() && character.unicode() < 128) {
            if (pendingDash && !out.isEmpty()) {
                out.append(QLatin1Char('-'));
            }
            pendingDash = false;
            out.append(character);
        } else if (!character.isMark()) {
            pendingDash = true;
        }
    }
    return out;
}

QString AppTree::folderName(Kind kind) {
    switch (kind) {
    case Kind::Make:
        return QStringLiteral("make");
    case Kind::Practice:
        return QStringLiteral("practice");
    case Kind::Machine:
        return QStringLiteral("machine");
    }
    return {};
}

std::optional<AppTree::Kind> AppTree::folderKind(const QString& name) {
    for (const Kind kind : kindOrder) {
        if (folderName(kind) == name) {
            return kind;
        }
    }
    return std::nullopt;
}

QStringList AppTree::folders() const {
    QStringList names;
    for (const Kind kind : kindOrder) {
        if (!entries(kind).isEmpty()) {
            names.append(folderName(kind));
        }
    }
    return names;
}

QList<AppTree::Entry> AppTree::entries(Kind kind) const {
    QList<Entry> inFolder;
    for (const Entry& entry : m_entries) {
        if (entry.kind == kind) {
            inFolder.append(entry);
        }
    }
    return inFolder;
}

std::optional<AppTree::Entry> AppTree::find(Kind kind, const QString& name) const {
    for (const Entry& entry : m_entries) {
        if (entry.kind == kind && entry.name == name) {
            return entry;
        }
    }
    return std::nullopt;
}

std::optional<AppTree::Entry> AppTree::findAnywhere(const QString& name) const {
    for (const Entry& entry : m_entries) {
        if (entry.name == name) {
            return entry;
        }
    }
    return std::nullopt;
}
