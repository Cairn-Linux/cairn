// SPDX-License-Identifier: Apache-2.0
#include "SaveRun.h"

#include "PkexecAnswer.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <QTimer>

#include <optional>
#include <utility>

namespace {

// cairn-family's lines are a few words; a longer one is not the command's.
constexpr qint64 longestLine = 256;

// cairn-family's exit codes with its last line.
constexpr int kDone = 0;
constexpr int kRefused = 1;

QString unknown() {
    return QStringLiteral("unknown");
}

// Nothing ran, so nothing can have been saved.
QString notStartedWord() {
    return QStringLiteral("not-started");
}

// The last line, read only as far as the words it can say.
QString answerFrom(const QString& lastLine, int exitCode) {
    static const QRegularExpression refused(QStringLiteral("^refused ([a-z]+(-[a-z]+)*)$"));
    if (exitCode == kDone && lastLine == QLatin1String("done")) {
        return QStringLiteral("done");
    }
    const QRegularExpressionMatch match = refused.match(lastLine);
    if (exitCode == kRefused && match.hasMatch()) {
        return match.captured(1);
    }
    return unknown();
}

} // namespace

SaveRun::SaveRun(QString pkexec, QString command, QObject* parent)
    : QObject(parent), m_pkexec(std::move(pkexec)), m_command(std::move(command)) {}

bool SaveRun::running() const {
    return m_process != nullptr || m_commandMissing;
}

bool SaveRun::start(const QStringList& arguments) {
    if (running()) {
        return false;
    }
    m_lastLine.clear();
    m_lines = 0;
    m_inLongLine = false;
    // As for FamilyChange: pkexec gives the same 127 for a missing command
    // as for a refused password, so the window checks first.
    if (!QFileInfo(m_command).isExecutable()) {
        qWarning("cairn-guardian: %s is not there to run, so nothing was done.",
                 qUtf8Printable(m_command));
        m_commandMissing = true;
        QTimer::singleShot(0, this, [this] {
            m_commandMissing = false;
            emit finished(notStartedWord());
        });
        return true;
    }
    m_process = new QProcess(this);
    // What the command and the tools it runs say on their error output goes
    // on to this program's, and from there to the journal.
    m_process->setProcessChannelMode(QProcess::ForwardedErrorChannel);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &SaveRun::readLines);
    connect(m_process, &QProcess::finished, this, &SaveRun::ended);
    connect(m_process, &QProcess::errorOccurred, this, &SaveRun::notStarted);
    m_process->start(m_pkexec, QStringList{m_command} + arguments);
    return true;
}

// Only whole lines are taken; the rest waits for more output. A piece as
// long as the longest line with no end is dropped to its end, so no part of
// it is read as a line of its own.
void SaveRun::readLines() {
    while (m_process->canReadLine() || m_process->bytesAvailable() >= longestLine) {
        const QByteArray piece = m_process->readLine(longestLine + 1);
        const bool whole = piece.endsWith('\n');
        if (whole && !m_inLongLine) {
            take(piece.chopped(1));
        }
        m_inLongLine = !whole;
    }
}

void SaveRun::take(const QByteArray& line) {
    ++m_lines;
    m_lastLine = QString::fromUtf8(line).trimmed();
    emit lineRead(m_lastLine);
}

void SaveRun::ended(int exitCode, QProcess::ExitStatus status) {
    if (m_process == nullptr) {
        return;
    }
    readLines();
    // The command ends every line, so a last piece with no end is a line
    // cut off.
    const QByteArray rest = m_process->readAllStandardOutput();
    if (!rest.isEmpty() && !m_inLongLine && rest.size() <= longestLine) {
        take(rest);
    }
    if (status != QProcess::NormalExit) {
        finish(unknown());
        return;
    }
    const std::optional<QString> pkexecOwn = PkexecAnswer::fromExitCode(exitCode);
    finish(m_lines == 0 && rest.isEmpty() && pkexecOwn ? *pkexecOwn
                                                       : answerFrom(m_lastLine, exitCode));
}

void SaveRun::notStarted(QProcess::ProcessError error) {
    if (error == QProcess::FailedToStart) {
        // The page sends the parent to this line in the system log.
        qWarning("cairn-guardian: %s could not be started, so nothing was done.",
                 qUtf8Printable(m_pkexec));
        finish(notStartedWord());
    }
}

// The process is let go before the signal, so a Save started from a slot
// connected to it can run.
void SaveRun::finish(const QString& answer) {
    if (m_process == nullptr) {
        return;
    }
    m_process->deleteLater();
    m_process = nullptr;
    emit finished(answer);
}
