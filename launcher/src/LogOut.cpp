// SPDX-License-Identifier: Apache-2.0
#include "LogOut.h"

#include <QDebug>
#include <QProcess>

namespace {
// cairn-log-out gives Steam up to ten seconds to quit and then ends the
// session at once, so a session still here after three times that is one
// the program did not end.
constexpr int defaultWaitMilliseconds = 30000;
} // namespace

LogOut::LogOut(QObject* parent) : QObject(parent), m_waitTimer(this) {
    m_waitTimer.setSingleShot(true);
    m_waitTimer.setInterval(defaultWaitMilliseconds);
    connect(&m_waitTimer, &QTimer::timeout, this, [this] {
        qWarning().noquote() << QStringLiteral("%1 did not end the session.").arg(m_program);
        setState(State::DidNotFinish);
    });
}

QString LogOut::program() const {
    return m_program;
}

void LogOut::setProgram(const QString& program) {
    if (program == m_program) {
        return;
    }
    m_program = program;
    emit programChanged();
}

bool LogOut::available() const {
    return !m_program.isEmpty();
}

LogOut::State LogOut::state() const {
    return m_state;
}

int LogOut::waitMilliseconds() const {
    return m_waitTimer.interval();
}

void LogOut::setWaitMilliseconds(int milliseconds) {
    if (milliseconds == m_waitTimer.interval()) {
        return;
    }
    m_waitTimer.setInterval(milliseconds);
    emit waitMillisecondsChanged();
}

bool LogOut::start() {
    if (!available()) {
        return false;
    }
    if (m_state == State::Leaving) {
        return true;
    }
    if (!QProcess::startDetached(m_program, {})) {
        qWarning().noquote() << QStringLiteral("%1 could not be started.").arg(m_program);
        setState(State::Idle);
        return false;
    }
    m_waitTimer.start();
    setState(State::Leaving);
    return true;
}

void LogOut::dismiss() {
    if (m_state == State::DidNotFinish) {
        setState(State::Idle);
    }
}

void LogOut::setState(State state) {
    if (state == m_state) {
        return;
    }
    m_state = state;
    emit stateChanged();
}
