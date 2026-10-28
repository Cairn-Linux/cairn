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
    process.setStandardInputFile(QProcess::nullDevice());
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
    process.setStandardOutputFile(QProcess::nullDevice());
    process.setProcessChannelMode(QProcess::ForwardedErrorChannel);
    process.start(program, arguments);
    return finishes(process);
}

std::optional<QByteArray> SystemTool::output(const QString& program, const QStringList& arguments,
                                             qint64 maxBytes) {
    QProcess process;
    prepare(process);
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
