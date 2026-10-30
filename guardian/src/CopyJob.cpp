// SPDX-License-Identifier: Apache-2.0
#include "CopyJob.h"

#include <QRegularExpression>

#include <utility>

namespace {

// What the copy program says when it copies nothing for the child at all
// (provision/cairn-copy-games.py).
bool isCondition(const QString& line) {
    static const QStringList conditions{
        QStringLiteral("no-library"),   QStringLiteral("no-shared"),
        QStringLiteral("lists-shared"), QStringLiteral("unreadable-config"),
        QStringLiteral("no-reflink"),
    };
    return conditions.contains(line);
}

// The helper's refusals a parent can act on. Any other means a mistake, and
// is held as `failed`.
bool isHoldingRefusal(const QString& word) {
    static const QStringList words{
        QStringLiteral("logged-in"),   QStringLiteral("busy"),          QStringLiteral("copying"),
        QStringLiteral("logging-out"), QStringLiteral("still-running"), QStringLiteral("stopped"),
        QStringLiteral("not-set-up"),
    };
    return words.contains(word);
}

} // namespace

CopyJob::CopyJob(QString login, QStringList appIds, Say say)
    : m_login(std::move(login)), m_appIds(std::move(appIds)), m_say(std::move(say)) {}

// The inhibitor lock lasts exactly as long as the helper: systemd-inhibit
// takes it, runs the helper, and lets it go when the helper ends.
CopyJob::Result CopyJob::run(const Setup& setup, const QString& login, unsigned int uid,
                             const QStringList& appIds, const Say& say) {
    if (appIds.isEmpty()) {
        return {};
    }
    CopyJob job(login, appIds, say);
    const int lines = SystemJob::run(
        {.systemdRun = setup.systemdRun,
         .unit = QStringLiteral("cairn-copy-games-%1").arg(uid),
         .description = QStringLiteral("Cairn copies games to a child"),
         .command =
             QStringList{setup.systemdInhibit, QStringLiteral("--what=sleep:handle-lid-switch"),
                         QStringLiteral("--mode=block"), QStringLiteral("--who=Cairn"),
                         QStringLiteral("--why=Copying games to a child"), setup.visitHelper,
                         QStringLiteral("--copy-games"), login} +
             appIds},
        [&job](const QString& line) { job.take(line); });

    Result result = job.m_result;
    if (job.m_ending == QStringLiteral("finished")) {
        return result;
    }
    if (job.m_ending && job.m_ending->startsWith(QStringLiteral("refused "))) {
        const QString word = job.m_ending->mid(QStringLiteral("refused ").size());
        result.held = isHoldingRefusal(word) ? word : QStringLiteral("failed");
    } else if (!job.m_ending && lines == 0) {
        // Nothing of the helper's came back: the name is still taken by an
        // earlier copy, the helper is missing, the inhibitor lock could not
        // be taken, or systemd is not there.
        result.held = QStringLiteral("not-started");
    } else {
        result.held = result.held.value_or(QStringLiteral("failed"));
    }
    return result;
}

void CopyJob::take(const QString& line) {
    static const QRegularExpression game(QStringLiteral("^(copied|have) ([1-9][0-9]{0,9})$"));
    static const QRegularExpression skipped(
        QStringLiteral("^skipped ([1-9][0-9]{0,9}) ([a-z-]{1,32})$"));
    static const QRegularExpression ending(
        QStringLiteral("^(finished|failed|refused [a-z-]{1,32})$"));
    if (line == QStringLiteral("waiting")) {
        m_say(QStringLiteral("waiting %1").arg(m_login));
    } else if (const QRegularExpressionMatch match = game.match(line);
               match.hasMatch() && m_appIds.contains(match.captured(2))) {
        m_say(QStringLiteral("%1 %2 %3").arg(match.captured(1), m_login, match.captured(2)));
        if (!m_result.copied.contains(match.captured(2))) {
            m_result.copied.append(match.captured(2));
        }
    } else if (const QRegularExpressionMatch match = skipped.match(line);
               match.hasMatch() && m_appIds.contains(match.captured(1))) {
        m_say(
            QStringLiteral("skipped %1 %2 %3").arg(m_login, match.captured(1), match.captured(2)));
    } else if (isCondition(line)) {
        m_result.held = m_result.held.value_or(line);
    } else if (ending.match(line).hasMatch()) {
        m_ending = line;
    }
}
