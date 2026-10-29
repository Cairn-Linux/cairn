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
        pattern(QStringLiteral("(pictures|catalogue) (made|kept)")));
    static const QRegularExpression step(pattern(QStringLiteral("(copying|waiting) ") + loginWord));
    static const QRegularExpression skipped(
        pattern(QStringLiteral("skipped ") + loginWord + ' ' + appIdWord + ' ' + reasonWord));
    static const QRegularExpression counted(
        pattern(QStringLiteral("child ") + loginWord +
                QStringLiteral(" added ([0-9]{1,4}) removed ([0-9]{1,4})(?: held ") + reasonWord +
                QStringLiteral(")?")));

    if (line == QLatin1String("looking")) {
        m_stage = Stage::Looking;
    } else if (const QRegularExpressionMatch match = made.match(line); match.hasMatch()) {
        const bool kept = match.captured(2) == QLatin1String("kept");
        if (match.captured(1) == QLatin1String("pictures")) {
            m_picturesKept = kept;
            m_stage = Stage::Looking;
        } else {
            m_catalogueKept = kept;
            m_stage = Stage::Writing;
        }
    } else if (const QRegularExpressionMatch match = step.match(line);
               match.hasMatch() && m_logins.contains(match.captured(2))) {
        m_stage = match.captured(1) == QLatin1String("copying") ? Stage::Copying : Stage::Waiting;
        m_current = match.captured(2);
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

QList<SaveReport::Child> SaveReport::children() const {
    return m_children;
}

SaveReport::Child& SaveReport::child(const QString& login) {
    const auto found = std::ranges::find(m_children, login, &Child::login);
    if (found != m_children.end()) {
        return *found;
    }
    m_children.append(Child{.login = login, .added = {}, .removed = {}, .held = {}, .skipped = {}});
    return m_children.last();
}
