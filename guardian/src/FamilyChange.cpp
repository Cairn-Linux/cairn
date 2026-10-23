// SPDX-License-Identifier: Apache-2.0
#include "FamilyChange.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <QTimer>

#include <utility>

namespace {

// pkexec's own exit codes, when it never ran the command. Plasma's polkit
// agent gives the second for its Cancel button too (VM, 2026-10-12).
constexpr int kDismissed = 126;
constexpr int kNotAuthorized = 127;

// cairn-family's exit codes with its answer.
constexpr int kDone = 0;
constexpr int kRefused = 1;

QString unknown() {
    return QStringLiteral("unknown");
}

// The command's one line, read only as far as the words it can say.
QString answerFrom(const QByteArray& output, int exitCode) {
    static const QRegularExpression refused(QStringLiteral("^refused ([a-z]+(-[a-z]+)*)$"));
    const QString line = QString::fromUtf8(output).trimmed();
    if (line.contains(QLatin1Char('\n'))) {
        return unknown();
    }
    if (exitCode == kDone && line == QLatin1String("done")) {
        return QStringLiteral("done");
    }
    const QRegularExpressionMatch match = refused.match(line);
    if (exitCode == kRefused && match.hasMatch()) {
        return match.captured(1);
    }
    return unknown();
}

} // namespace

FamilyChange::FamilyChange(QString pkexec, QString command, QObject* parent)
    : QObject(parent), m_pkexec(std::move(pkexec)), m_command(std::move(command)) {}

bool FamilyChange::running() const {
    return m_process != nullptr || m_commandMissing;
}

bool FamilyChange::start(const QStringList& arguments) {
    if (running()) {
        return false;
    }
    // pkexec checks the command's path before it asks for a password, and
    // gives the same 127 as a refused password when it is not there. The
    // window checks first, so a missing command is never told to the
    // parent as their password. The answer still comes after start()
    // returns, as every other answer does.
    if (!QFileInfo(m_command).isExecutable()) {
        qWarning("cairn-guardian: %s is not there to run, so nothing changed.",
                 qUtf8Printable(m_command));
        m_commandMissing = true;
        QTimer::singleShot(0, this, [this] {
            m_commandMissing = false;
            emit finished(unknown());
        });
        return true;
    }
    m_process = new QProcess(this);
    // What the command and the tools it runs say on their error output goes
    // on to this program's, and from there to the journal.
    m_process->setProcessChannelMode(QProcess::ForwardedErrorChannel);
    connect(m_process, &QProcess::finished, this, &FamilyChange::ended);
    connect(m_process, &QProcess::errorOccurred, this, &FamilyChange::notStarted);
    m_process->start(m_pkexec, QStringList{m_command} + arguments);
    return true;
}

void FamilyChange::ended(int exitCode, QProcess::ExitStatus status) {
    if (m_process == nullptr) {
        return;
    }
    if (status != QProcess::NormalExit) {
        finish(unknown());
        return;
    }
    const QByteArray output = m_process->readAllStandardOutput();
    if (output.isEmpty() && exitCode == kDismissed) {
        finish(QStringLiteral("dismissed"));
    } else if (output.isEmpty() && exitCode == kNotAuthorized) {
        finish(QStringLiteral("not-authorized"));
    } else {
        finish(answerFrom(output, exitCode));
    }
}

void FamilyChange::notStarted(QProcess::ProcessError error) {
    if (error == QProcess::FailedToStart) {
        finish(unknown());
    }
}

// The process is let go before the signal, so a change started from a slot
// connected to it can run.
void FamilyChange::finish(const QString& answer) {
    if (m_process == nullptr) {
        return;
    }
    m_process->deleteLater();
    m_process = nullptr;
    emit finished(answer);
}
