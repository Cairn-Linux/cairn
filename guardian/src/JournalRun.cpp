// SPDX-License-Identifier: Apache-2.0
#include "JournalRun.h"

#include <QProcessEnvironment>

#include <utility>

JournalRun::JournalRun(QString program, QStringList arguments, QObject* parent)
    : QObject(parent), m_program(std::move(program)), m_arguments(std::move(arguments)) {
    m_limit.setSingleShot(true);
    m_limit.setInterval(maxMilliseconds);
    connect(&m_limit, &QTimer::timeout, this, [this] {
        if (m_process != nullptr) {
            m_timedOut = true;
            m_process->kill();
        }
    });
}

JournalRun::~JournalRun() {
    if (m_process != nullptr) {
        m_process->disconnect(this);
        m_process->kill();
        m_process->waitForFinished();
    }
}

void JournalRun::start() {
    m_process = new QProcess(this);
    QProcessEnvironment environment;
    environment.insert(QStringLiteral("PATH"), QStringLiteral("/usr/sbin:/usr/bin"));
    environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C.UTF-8"));
    environment.insert(QStringLiteral("SYSTEMD_PAGER"), QString());
    m_process->setProcessEnvironment(environment);
    m_process->setWorkingDirectory(QStringLiteral("/"));
    m_process->setStandardInputFile(QProcess::nullDevice());
    // What journalctl says as an error, such as its hint about groups, can
    // name files; it is dropped rather than put in the window's notes.
    m_process->setStandardErrorFile(QProcess::nullDevice());
    connect(m_process, &QProcess::readyReadStandardOutput, this, &JournalRun::readLines);
    connect(m_process, &QProcess::finished, this, &JournalRun::ended);
    connect(m_process, &QProcess::errorOccurred, this, &JournalRun::notStarted);
    m_limit.start();
    m_process->start(m_program, m_arguments);
}

void JournalRun::readLines() {
    if (m_process == nullptr) {
        return;
    }
    m_partial += m_process->readAllStandardOutput();
    qsizetype from = 0;
    while (true) {
        const qsizetype end = m_partial.indexOf('\n', from);
        if (end < 0) {
            break;
        }
        if (!m_skipping) {
            emit line(m_partial.sliced(from, end - from));
        }
        m_skipping = false;
        from = end + 1;
    }
    m_partial.remove(0, from);
    if (m_partial.size() > maxLine) {
        m_partial.clear();
        m_skipping = true;
    }
}

void JournalRun::ended(int exitCode, QProcess::ExitStatus status) {
    readLines();
    if (!m_skipping && !m_partial.isEmpty()) {
        emit line(m_partial);
    }
    m_partial.clear();
    finish(!m_timedOut && status == QProcess::NormalExit && exitCode == 0);
}

void JournalRun::notStarted(QProcess::ProcessError error) {
    if (error == QProcess::FailedToStart) {
        finish(false);
    }
}

// The process is let go before the signal, so its owner may delete this
// run from a slot.
void JournalRun::finish(bool succeeded) {
    if (m_process == nullptr) {
        return;
    }
    m_limit.stop();
    m_process->disconnect(this);
    m_process->deleteLater();
    m_process = nullptr;
    emit finished(succeeded);
}
