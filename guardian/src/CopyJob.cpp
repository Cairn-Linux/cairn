// SPDX-License-Identifier: Apache-2.0
#include "CopyJob.h"

#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>

#include <utility>

namespace {

// The helper's lines are a few words; a longer one is not the helper's.
constexpr qint64 longestLine = 256;

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
// is held as `failed`. `stuck` comes only from a log-out.
bool isHoldingRefusal(const QString& word) {
    static const QStringList words{
        QStringLiteral("logged-in"),     QStringLiteral("busy"),    QStringLiteral("copying"),
        QStringLiteral("still-running"), QStringLiteral("stopped"), QStringLiteral("not-set-up"),
        QStringLiteral("stuck"),
    };
    return words.contains(word);
}

// The helper's own trace goes to its error output, which goes nowhere.
// systemd-run gets nothing of root's environment, and the service gets the
// service manager's, so PKEXEC_UID never reaches the helper.
void prepare(QProcess& process) {
    QProcessEnvironment environment;
    environment.insert(QStringLiteral("PATH"), QStringLiteral("/usr/sbin:/usr/bin"));
    environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    process.setProcessEnvironment(environment);
    process.setWorkingDirectory(QStringLiteral("/"));
    process.setStandardInputFile(QProcess::nullDevice());
    process.setStandardErrorFile(QProcess::nullDevice());
    process.setUnixProcessParameters(QProcess::UnixProcessFlag::ResetSignalHandlers);
}

} // namespace

CopyJob::CopyJob(QString login, QStringList appIds, Say say)
    : m_login(std::move(login)), m_appIds(std::move(appIds)), m_say(std::move(say)) {}

// --pipe hands the service this program's pipe, so the helper's lines come
// straight back, and --wait waits for the service to end. --collect lets a
// failed job's name be used again.
// The helper takes --log-out straight after --copy-games.
CopyJob::Result CopyJob::run(const Setup& setup, const QString& login, unsigned int uid,
                             const QStringList& appIds, IfLoggedIn ifLoggedIn, const Say& say) {
    if (appIds.isEmpty()) {
        return {};
    }
    QStringList helper{setup.visitHelper, QStringLiteral("--copy-games")};
    if (ifLoggedIn == IfLoggedIn::LogOut) {
        helper.append(QStringLiteral("--log-out"));
    }
    helper += QStringList{login} + appIds;
    QProcess process;
    prepare(process);
    if (setup.runs == Runs::InThisUnit) {
        process.start(helper.takeFirst(), helper);
    } else {
        process.start(setup.systemdRun,
                      QStringList{QStringLiteral("--quiet"), QStringLiteral("--collect"),
                                  QStringLiteral("--wait"), QStringLiteral("--pipe"),
                                  QStringLiteral("--no-ask-password"),
                                  QStringLiteral("--service-type=exec"),
                                  QStringLiteral("--expand-environment=no"),
                                  QStringLiteral("--unit=cairn-copy-games-%1").arg(uid),
                                  QStringLiteral("--description=Cairn copies games to a child"),
                                  QStringLiteral("--")} +
                          helper);
    }

    CopyJob job(login, appIds, say);
    job.read(process);
    process.waitForFinished(-1);

    Result result = job.m_result;
    if (job.m_ending == QStringLiteral("finished")) {
        return result;
    }
    if (job.m_ending && job.m_ending->startsWith(QStringLiteral("refused "))) {
        const QString word = job.m_ending->mid(QStringLiteral("refused ").size());
        result.held = isHoldingRefusal(word) ? word : QStringLiteral("failed");
    } else if (!job.m_ending && job.m_lines == 0) {
        // Nothing of the helper's came: systemd-run's name is still taken by
        // an earlier copy, the helper is missing, or systemd is not there.
        result.held = QStringLiteral("not-started");
    } else {
        result.held = result.held.value_or(QStringLiteral("failed"));
    }
    return result;
}

// Waits for each line with no time limit. A line too long to be the helper's
// is dropped to its end, so no part of it is read as a line of its own.
void CopyJob::read(QProcess& process) {
    bool inLongLine = false;
    const auto takeLines = [&] {
        while (process.canReadLine() || process.bytesAvailable() >= longestLine) {
            const QByteArray piece = process.readLine(longestLine + 1);
            const bool whole = piece.endsWith('\n');
            if (whole && !inLongLine) {
                take(QString::fromUtf8(piece.chopped(1)));
            }
            inLongLine = !whole;
        }
    };
    do {
        takeLines();
    } while (process.waitForReadyRead(-1));
    takeLines();
}

void CopyJob::take(const QString& line) {
    if (++m_lines > mostLines) {
        return;
    }
    static const QRegularExpression game(QStringLiteral("^(copied|have) ([1-9][0-9]{0,9})$"));
    static const QRegularExpression skipped(
        QStringLiteral("^skipped ([1-9][0-9]{0,9}) ([a-z-]{1,32})$"));
    static const QRegularExpression ending(
        QStringLiteral("^(finished|failed|refused [a-z-]{1,32})$"));
    if (line == QStringLiteral("waiting") || line == QStringLiteral("logging-out") ||
        line == QStringLiteral("logged-out")) {
        m_say(QStringLiteral("%1 %2").arg(line, m_login));
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
