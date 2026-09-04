// SPDX-License-Identifier: Apache-2.0
#include "AppLauncher.h"

#include <QDebug>

namespace {
constexpr int defaultSettleMilliseconds = 5000;
}

AppLauncher::AppLauncher(QObject* parent) : QObject(parent), m_process(this), m_settleTimer(this) {
    m_settleTimer.setSingleShot(true);
    m_settleTimer.setInterval(defaultSettleMilliseconds);
    connect(&m_settleTimer, &QTimer::timeout, this, [this] { m_settled = true; });
    connect(&m_process, &QProcess::started, this, &AppLauncher::onStarted);
    connect(&m_process, &QProcess::errorOccurred, this, &AppLauncher::onErrorOccurred);
    connect(&m_process, &QProcess::finished, this, &AppLauncher::onFinished);
    // The child's app draws its own window; its terminal output is not for the child.
    m_process.setProcessChannelMode(QProcess::ForwardedChannels);
}

AppLauncher::State AppLauncher::state() const {
    return m_state;
}

QString AppLauncher::title() const {
    return m_title;
}

int AppLauncher::settleMilliseconds() const {
    return m_settleTimer.interval();
}

void AppLauncher::setSettleMilliseconds(int milliseconds) {
    if (milliseconds == m_settleTimer.interval()) {
        return;
    }
    m_settleTimer.setInterval(milliseconds);
    emit settleMillisecondsChanged();
}

void AppLauncher::launch(const QString& title, const QStringList& exec) {
    if (m_state == State::Starting || m_state == State::Running) {
        return;
    }
    if (m_title != title) {
        m_title = title;
        emit titleChanged();
    }
    if (exec.isEmpty()) {
        qWarning().noquote() << QStringLiteral("No program is set up for the tile %1.").arg(title);
        setState(State::Failed);
        return;
    }

    m_settled = false;
    setState(State::Starting);
    m_settleTimer.start();
    m_process.start(exec.first(), exec.mid(1));
}

void AppLauncher::dismiss() {
    if (m_state == State::Failed) {
        setState(State::Idle);
    }
}

void AppLauncher::setState(State state) {
    if (state == m_state) {
        return;
    }
    m_state = state;
    emit stateChanged();
}

void AppLauncher::onStarted() {
    setState(State::Running);
}

void AppLauncher::onErrorOccurred(QProcess::ProcessError error) {
    if (error == QProcess::FailedToStart) {
        m_settleTimer.stop();
        qWarning().noquote() << QStringLiteral("The program for %1 did not start: %2")
                                    .arg(m_title, m_process.program());
        setState(State::Failed);
    }
}

void AppLauncher::onFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    m_settleTimer.stop();
    const bool quitBadly = exitStatus == QProcess::CrashExit || exitCode != 0;
    if (quitBadly && !m_settled) {
        qWarning().noquote() << QStringLiteral("The program for %1 quit right away with code %2.")
                                    .arg(m_title)
                                    .arg(exitCode);
        setState(State::Failed);
        return;
    }
    setState(State::Idle);
}
