// SPDX-License-Identifier: Apache-2.0
#include "LogOut.h"

#include <QProcess>

LogOut::LogOut(QObject* parent) : QObject(parent) {}

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

bool LogOut::start() {
    if (!available()) {
        return false;
    }
    return QProcess::startDetached(m_program, {});
}
