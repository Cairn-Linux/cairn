// SPDX-License-Identifier: Apache-2.0
#include "SystemJob.h"

#include <QProcess>
#include <QProcessEnvironment>

namespace {

// A helper's lines are a few words; a longer one is not the helper's.
constexpr qint64 longestLine = 256;

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

// --pipe hands the service this program's pipe, so the helper's lines come
// straight back, and --wait waits for the service to end. --collect lets a
// failed job's name be used again. For --uid, systemd sets the account's own
// groups as a login would, and RuntimeMaxSec stops the whole service, cp
// and all.
int SystemJob::run(const Job& job, const Take& take) {
    QStringList arguments{
        QStringLiteral("--quiet"),
        QStringLiteral("--collect"),
        QStringLiteral("--wait"),
        QStringLiteral("--pipe"),
        QStringLiteral("--no-ask-password"),
        QStringLiteral("--service-type=exec"),
        QStringLiteral("--expand-environment=no"),
        QStringLiteral("--unit=") + job.unit,
        QStringLiteral("--description=") + job.description,
    };
    if (!job.user.isEmpty()) {
        arguments.append(QStringLiteral("--uid=") + job.user);
    }
    if (job.timeLimitSeconds > 0) {
        arguments.append(QStringLiteral("--property=RuntimeMaxSec=%1").arg(job.timeLimitSeconds));
    }
    arguments.append(QStringLiteral("--"));
    QProcess process;
    prepare(process);
    process.start(job.systemdRun, arguments + job.command);
    const int lines = read(process, take);
    process.waitForFinished(-1);
    return lines;
}

// Waits for each line with no time limit. A line too long to be the helper's
// is dropped to its end, so no part of it is read as a line of its own.
int SystemJob::read(QProcess& process, const Take& take) {
    int lines = 0;
    bool inLongLine = false;
    const auto takeLines = [&] {
        while (process.canReadLine() || process.bytesAvailable() >= longestLine) {
            const QByteArray piece = process.readLine(longestLine + 1);
            const bool whole = piece.endsWith('\n');
            if (whole && !inLongLine && ++lines <= mostLines) {
                take(QString::fromUtf8(piece.chopped(1)));
            }
            inLongLine = !whole;
        }
    };
    do {
        takeLines();
    } while (process.waitForReadyRead(-1));
    takeLines();
    return lines;
}
