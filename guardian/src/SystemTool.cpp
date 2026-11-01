// SPDX-License-Identifier: Apache-2.0
#include "SystemTool.h"

#include <QProcess>
#include <QProcessEnvironment>

namespace {

// Long enough for useradd to copy the skeleton into a new home.
constexpr int toolMilliseconds = 120000;

// save-games ignores SIGPIPE, and a program inherits a signal it ignores,
// so each tool starts with the usual handlers instead.
void prepare(QProcess& process) {
    QProcessEnvironment environment;
    environment.insert(QStringLiteral("PATH"), QStringLiteral("/usr/sbin:/usr/bin"));
    environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    process.setProcessEnvironment(environment);
    process.setWorkingDirectory(QStringLiteral("/"));
    process.setUnixProcessParameters(QProcess::UnixProcessFlag::ResetSignalHandlers);
}

bool finishes(QProcess& process) {
    if (!process.waitForFinished(toolMilliseconds)) {
        process.kill();
        process.waitForFinished();
        return false;
    }
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

} // namespace

bool SystemTool::run(const QString& program, const QStringList& arguments) {
    QProcess process;
    prepare(process);
    process.setStandardInputFile(QProcess::nullDevice());
    process.setStandardOutputFile(QProcess::nullDevice());
    process.setProcessChannelMode(QProcess::ForwardedErrorChannel);
    process.start(program, arguments);
    return finishes(process);
}

// The input is handed to the tool as it starts and the channel closed after
// it, so a tool that reads to the end of its input sees all of it, then
// the end.
bool SystemTool::runWithInput(const QString& program, const QStringList& arguments,
                              const QByteArray& input) {
    QProcess process;
    prepare(process);
    process.setStandardOutputFile(QProcess::nullDevice());
    process.setProcessChannelMode(QProcess::ForwardedErrorChannel);
    process.start(program, arguments);
    process.write(input);
    process.closeWriteChannel();
    return finishes(process);
}

std::optional<QByteArray> SystemTool::output(const QString& program, const QStringList& arguments,
                                             qint64 maxBytes) {
    QProcess process;
    prepare(process);
    process.setStandardInputFile(QProcess::nullDevice());
    process.setStandardErrorFile(QProcess::nullDevice());
    process.start(program, arguments);
    if (!finishes(process)) {
        return std::nullopt;
    }
    QByteArray printed = process.readAllStandardOutput();
    if (printed.size() > maxBytes) {
        return std::nullopt;
    }
    return printed;
}

// The tool's own lines say whether it went to its end, so its exit status
// is not needed. What follows the last line end was cut off, by the end of
// the output or by the two-minute limit, so it is not a line.
QStringList SystemTool::lines(const QString& program, const QStringList& arguments,
                              qint64 maxBytes) {
    QProcess process;
    prepare(process);
    process.setStandardInputFile(QProcess::nullDevice());
    process.setStandardErrorFile(QProcess::nullDevice());
    process.start(program, arguments);
    finishes(process);
    const QByteArray printed = process.readAllStandardOutput();
    if (printed.size() > maxBytes) {
        return {};
    }
    QStringList whole = QString::fromUtf8(printed).split(QLatin1Char('\n'));
    whole.removeLast();
    return whole;
}
