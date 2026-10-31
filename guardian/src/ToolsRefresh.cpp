// SPDX-License-Identifier: Apache-2.0
#include "ToolsRefresh.h"

#include "SystemJob.h"

#include <QRegularExpression>

// The step runs as the Guardian, so it reads only what the Guardian could
// read anyway, follows their library list into no folder of anyone else's,
// and writes builds the Guardian owns, as Steam's own installs in the shared
// folder are. A fixed unit name means one at a time.
ToolsRefresh::Result ToolsRefresh::run(const Setup& setup, const std::optional<QString>& guardian) {
    Result result;
    if (!guardian) {
        return result;
    }
    SystemJob::run(
        {.systemdRun = setup.systemdRun,
         .unit = QStringLiteral("cairn-share-tools"),
         .description = QStringLiteral("Cairn takes Steam's tools from a grown-up's Steam"),
         .command = {setup.python, QStringLiteral("-I"), QStringLiteral("-B"), setup.shareStep},
         .user = *guardian,
         .timeLimitSeconds = timeLimitSeconds},
        [&result](const QString& line) { take(result, line); });
    return result;
}

// The step's own words (provision/cairn-share-tools.py), with cairn-family's
// rules for an appid and a reason word.
void ToolsRefresh::take(Result& result, const QString& line) {
    static const QRegularExpression inFolder(QStringLiteral("^tool-(shared|had) [1-9][0-9]{0,9}$"));
    static const QRegularExpression leftOut(
        QStringLiteral("^tool-(kept|not-shared) [1-9][0-9]{0,9} [a-z]+(?:-[a-z]+)*$"));
    static const QRegularExpression ending(
        QStringLiteral("^(failed|no-steam|refused root|refused folder)$"));
    if (line == QLatin1String("finished")) {
        result.made = true;
    } else if (const QRegularExpressionMatch match = inFolder.match(line); match.hasMatch()) {
        if (match.captured(1) == QLatin1String("shared")) {
            result.notes.append(line);
        }
    } else if (const QRegularExpressionMatch match = leftOut.match(line); match.hasMatch()) {
        result.notes.append(line);
        if (match.captured(1) == QLatin1String("not-shared")) {
            result.leftOut.append(line);
        }
    } else if (ending.match(line).hasMatch()) {
        result.notes.append(line);
    }
}
