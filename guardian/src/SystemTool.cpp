// SPDX-License-Identifier: Apache-2.0
#include "SystemTool.h"

#include <QProcess>
#include <QProcessEnvironment>

namespace {

// Long enough for useradd to copy the skeleton into a new home.
constexpr int toolMilliseconds = 120000;

} // namespace

bool SystemTool::run(const QString& program, const QStringList& arguments) {
    QProcessEnvironment environment;
    environment.insert(QStringLiteral("PATH"), QStringLiteral("/usr/sbin:/usr/bin"));
    environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));

    QProcess process;
    process.setProcessEnvironment(environment);
    process.setWorkingDirectory(QStringLiteral("/"));
    process.setStandardInputFile(QProcess::nullDevice());
    process.setStandardOutputFile(QProcess::nullDevice());
    process.setProcessChannelMode(QProcess::ForwardedErrorChannel);
    process.start(program, arguments);
    if (!process.waitForFinished(toolMilliseconds)) {
        process.kill();
        process.waitForFinished();
        return false;
    }
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}
