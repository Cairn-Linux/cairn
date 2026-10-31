// SPDX-License-Identifier: Apache-2.0
#include "SaveReport.h"

#include <QLatin1StringView>
#include <QRegularExpression>

#include <algorithm>

namespace {

// The words of ADR-0041's table, with cairn-family's own rules for a login,
// an appid and a word.
constexpr QLatin1StringView loginWord("([a-z_][a-z0-9_-]{0,31})");
constexpr QLatin1StringView appIdWord("([1-9][0-9]{0,9})");
constexpr QLatin1StringView reasonWord("([a-z]+(?:-[a-z]+)*)");

QRegularExpression pattern(const QString& text) {
    return QRegularExpression(QRegularExpression::anchoredPattern(text));
}

} // namespace

SaveReport::SaveReport(const QStringList& logins) : m_logins(logins.begin(), logins.end()) {}

void SaveReport::take(const QString& line) {
    static const QRegularExpression made(
        pattern(QStringLiteral("(pictures|catalogue|tools) (made|kept)")));
    static const QRegularExpression picture(
        pattern(QStringLiteral("picture-(missing|refused) ") + appIdWord));
    static const QRegularExpression step(pattern(QStringLiteral("(copying|waiting) ") + loginWord));
    static const QRegularExpression game(
        pattern(QStringLiteral("(copied|have) ") + loginWord + ' ' + appIdWord));
    static const QRegularExpression skipped(
        pattern(QStringLiteral("skipped ") + loginWord + ' ' + appIdWord + ' ' + reasonWord));
    static const QRegularExpression counted(
        pattern(QStringLiteral("child ") + loginWord +
                QStringLiteral(" added ([0-9]{1,4}) removed ([0-9]{1,4})(?: held ") + reasonWord +
                QStringLiteral(")?")));

    if (takeTool(line)) {
        return;
    }
    if (line == QLatin1String("looking")) {
        m_stage = Stage::Looking;
    } else if (const QRegularExpressionMatch match = made.match(line); match.hasMatch()) {
        takeMade(match.captured(1), match.captured(2) == QLatin1String("kept"));
    } else if (const QRegularExpressionMatch match = picture.match(line); match.hasMatch()) {
        // Said after the full list, while the stage is the tools'.
        if (match.captured(1) == QLatin1String("missing")) {
            m_picturesMissing.append(match.captured(2));
        } else {
            m_picturesRefused.append(match.captured(2));
        }
    } else if (const QRegularExpressionMatch match = step.match(line);
               match.hasMatch() && m_logins.contains(match.captured(2))) {
        m_stage = match.captured(1) == QLatin1String("copying") ? Stage::Copying : Stage::Waiting;
        m_current = match.captured(2);
    } else if (const QRegularExpressionMatch match = game.match(line);
               match.hasMatch() && m_logins.contains(match.captured(2))) {
        m_copied.append({.login = match.captured(2),
                         .appId = match.captured(3),
                         .had = match.captured(1) == QLatin1String("have")});
        m_copiedForSomeone = true;
    } else if (const QRegularExpressionMatch match = skipped.match(line);
               match.hasMatch() && m_logins.contains(match.captured(1))) {
        child(match.captured(1))
            .skipped.append({.appId = match.captured(2), .reason = match.captured(3)});
    } else if (const QRegularExpressionMatch match = counted.match(line);
               match.hasMatch() && m_logins.contains(match.captured(1))) {
        Child& counts = child(match.captured(1));
        counts.added = match.captured(2).toInt();
        counts.removed = match.captured(3).toInt();
        if (match.hasCaptured(4)) {
            counts.held = match.captured(4);
        }
        m_stage = Stage::Writing;
        m_current.clear();
    }
}

// The pictures, the full games list and the tools folder are made in turn,
// so each one's line says the next has started.
void SaveReport::takeMade(const QString& what, bool kept) {
    if (what == QLatin1String("pictures")) {
        m_picturesKept = kept;
        m_stage = Stage::Looking;
    } else if (what == QLatin1String("catalogue")) {
        m_catalogueKept = kept;
        m_stage = Stage::SharingTools;
    } else {
        m_toolsKept = kept;
        m_stage = Stage::Writing;
    }
}

// The lines about Steam's tools (ADR-0046): the tools step's left out ones,
// which name no child, and each child's copy of the tools.
bool SaveReport::takeTool(const QString& line) {
    static const QRegularExpression notShared(
        pattern(QStringLiteral("tool-not-shared ") + appIdWord + ' ' + reasonWord));
    static const QRegularExpression step(
        pattern(QStringLiteral("(copying-tools|no-tools|tools-unreadable) ") + loginWord));
    static const QRegularExpression tool(
        pattern(QStringLiteral("tool-(copied|have|missing) ") + loginWord + ' ' + appIdWord));
    static const QRegularExpression skipped(
        pattern(QStringLiteral("tool-skipped ") + loginWord + ' ' + appIdWord + ' ' + reasonWord));
    if (notShared.match(line).hasMatch()) {
        m_toolsNotShared = true;
        return true;
    }
    QRegularExpressionMatch match = step.match(line);
    if (match.hasMatch() && m_logins.contains(match.captured(2))) {
        m_copiedForSomeone = true;
        if (match.captured(1) == QLatin1String("copying-tools")) {
            m_stage = Stage::CopyingTools;
            m_current = match.captured(2);
        } else if (match.captured(1) == QLatin1String("no-tools")) {
            child(match.captured(2)).noTools = true;
        } else {
            child(match.captured(2)).toolsMissing = true;
        }
        return true;
    }
    match = tool.match(line);
    if (match.hasMatch() && m_logins.contains(match.captured(2))) {
        m_copiedForSomeone = true;
        Child& given = child(match.captured(2));
        if (match.captured(1) == QLatin1String("copied")) {
            ++given.toolsCopied;
        } else if (match.captured(1) == QLatin1String("missing")) {
            given.toolsMissing = true;
        }
        return true;
    }
    match = skipped.match(line);
    if (match.hasMatch() && m_logins.contains(match.captured(1))) {
        m_copiedForSomeone = true;
        child(match.captured(1)).toolsMissing = true;
        return true;
    }
    return false;
}

SaveReport::Stage SaveReport::stage() const {
    return m_stage;
}

QString SaveReport::current() const {
    return m_current;
}

std::optional<bool> SaveReport::picturesKept() const {
    return m_picturesKept;
}

std::optional<bool> SaveReport::catalogueKept() const {
    return m_catalogueKept;
}

QStringList SaveReport::picturesMissing() const {
    return m_picturesMissing;
}

QStringList SaveReport::picturesRefused() const {
    return m_picturesRefused;
}

bool SaveReport::toolsLeftOut() const {
    return m_toolsKept || m_toolsNotShared;
}

bool SaveReport::copiedForSomeone() const {
    return m_copiedForSomeone;
}

QList<SaveReport::Child> SaveReport::children() const {
    return m_children;
}

QList<SaveReport::Copied> SaveReport::copied() const {
    return m_copied;
}

SaveReport::Child& SaveReport::child(const QString& login) {
    const auto found = std::ranges::find(m_children, login, &Child::login);
    if (found != m_children.end()) {
        return *found;
    }
    m_children.append(Child{.login = login,
                            .added = {},
                            .removed = {},
                            .held = {},
                            .skipped = {},
                            .toolsCopied = 0,
                            .toolsMissing = false,
                            .noTools = false});
    return m_children.last();
}
