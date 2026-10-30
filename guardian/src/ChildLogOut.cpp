// SPDX-License-Identifier: Apache-2.0
#include "ChildLogOut.h"

#include "SaveRun.h"

#include <QHash>

namespace {

ChildLogOut::Reason reasonFor(const QString& word) {
    using Reason = ChildLogOut::Reason;
    static const QHash<QString, Reason> reasons = {
        {QStringLiteral("stuck"), Reason::Stuck},
        {QStringLiteral("logged-in"), Reason::StillClosing},
        {QStringLiteral("stopped"), Reason::StoppedEarly},
        {QStringLiteral("busy"), Reason::VisitOpen},
        {QStringLiteral("copying"), Reason::CopyRunning},
        {QStringLiteral("logging-out"), Reason::AnotherLogOut},
        {QStringLiteral("not-set-up"), Reason::NotSetUp},
        {QStringLiteral("not-started"), Reason::NotStarted},
        {QStringLiteral("dismissed"), Reason::Dismissed},
        {QStringLiteral("not-authorized"), Reason::NotAuthorized},
        {QStringLiteral("not-a-grown-up"), Reason::NotAGrownUp},
        {QStringLiteral("not-a-child"), Reason::NotAChild},
    };
    return reasons.value(word, Reason::Unknown);
}

} // namespace

ChildLogOut::ChildLogOut(const QString& pkexec, const QString& command, QObject* parent)
    : QObject(parent), m_run(new SaveRun(pkexec, command, this)) {
    connect(m_run, &SaveRun::lineRead, this, &ChildLogOut::lineRead);
    connect(m_run, &SaveRun::finished, this, &ChildLogOut::finished);
}

ChildLogOut::Step ChildLogOut::step() const {
    return m_step;
}

ChildLogOut::Reason ChildLogOut::reason() const {
    return m_reason;
}

QString ChildLogOut::login() const {
    return m_login;
}

QString ChildLogOut::name() const {
    return m_name;
}

bool ChildLogOut::busy() const {
    return m_step == Step::Asking || m_step == Step::LoggingOut;
}

bool ChildLogOut::isAbout(const QString& login) const {
    return m_step != Step::NotAsking && m_login == login;
}

void ChildLogOut::ask(const QString& login, const QString& name) {
    if (busy() || login.isEmpty()) {
        return;
    }
    m_step = Step::Warning;
    m_reason = Reason::NoReason;
    m_login = login;
    m_name = name;
    emit changed();
}

// Asking comes first: when the process cannot start at all, its answer
// arrives inside start(), and must not be overwritten.
void ChildLogOut::confirm() {
    if (m_step != Step::Warning) {
        return;
    }
    m_step = Step::Asking;
    m_held.clear();
    emit changed();
    if (!m_run->start({QStringLiteral("log-out"), m_login})) {
        clear();
    }
}

void ChildLogOut::notNow() {
    if (m_step == Step::Warning) {
        clear();
    }
}

void ChildLogOut::close() {
    if (m_step == Step::NotLoggedOut) {
        clear();
    }
}

// Only the words about the child asked for count.
void ChildLogOut::lineRead(const QString& line) {
    if (line == QStringLiteral("logging-out ") + m_login) {
        m_step = Step::LoggingOut;
        emit changed();
    } else if (const QString held = QStringLiteral("held %1 ").arg(m_login);
               line.startsWith(held)) {
        m_held = line.mid(held.size());
    }
}

// A logged-out child needs no sentence: their games simply show again.
void ChildLogOut::finished(const QString& answer) {
    if (answer == QStringLiteral("done") && m_held.isEmpty()) {
        clear();
    } else {
        m_step = Step::NotLoggedOut;
        if (answer == QStringLiteral("done")) {
            m_reason = reasonFor(m_held);
        } else if (answer == QStringLiteral("not-started")) {
            // SaveRun's own word: the command itself never ran.
            m_reason = Reason::CommandNotStarted;
        } else {
            m_reason = reasonFor(answer);
        }
        emit changed();
    }
    emit ended();
}

void ChildLogOut::clear() {
    m_step = Step::NotAsking;
    m_reason = Reason::NoReason;
    m_login.clear();
    m_name.clear();
    emit changed();
}
