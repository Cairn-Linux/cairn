// SPDX-License-Identifier: Apache-2.0
#include "SteamVisit.h"

#include <utility>

namespace {

// pkexec's own exit codes, when it never ran the helper. Plasma's polkit
// agent gives the second for its Cancel button too (VM, 2026-10-12).
constexpr int kDismissed = 126;
constexpr int kNotAuthorized = 127;

} // namespace

SteamVisit::SteamVisit(QString pkexec, QString helper, QObject* parent)
    : QObject(parent), m_pkexec(std::move(pkexec)), m_helper(std::move(helper)) {}

// The helper runs as root, so the Guardian's process cannot end it. Ending
// its input closes the visit, and QProcess then waits for it to finish.
SteamVisit::~SteamVisit() {
    if (m_process != nullptr) {
        m_process->closeWriteChannel();
    }
}

SteamVisit::State SteamVisit::state() const {
    return m_state;
}

QString SteamVisit::refusal() const {
    return m_refusal;
}

void SteamVisit::open(const QString& child) {
    if (m_process != nullptr && m_process->state() != QProcess::NotRunning) {
        return;
    }
    if (m_process != nullptr) {
        m_process->deleteLater();
    }
    m_process = new QProcess(this);
    // The helper traces what it does on its error output, which goes on to
    // this program's, and from there to the journal.
    m_process->setProcessChannelMode(QProcess::ForwardedErrorChannel);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &SteamVisit::readLines);
    connect(m_process, &QProcess::finished, this, &SteamVisit::ended);
    connect(m_process, &QProcess::errorOccurred, this, &SteamVisit::notStarted);
    setState(State::Asking);
    m_process->start(m_pkexec, {m_helper, child});
}

void SteamVisit::close() {
    if (m_process == nullptr || m_process->state() == QProcess::NotRunning) {
        return;
    }
    if (m_state == State::Asking) {
        // pkexec still runs as the Guardian until the password is given, so
        // it can be stopped. Once it runs the helper as root it cannot, and
        // the end of the helper's input below closes the visit as it opens.
        m_process->terminate();
    } else {
        m_process->write("done\n");
    }
    m_process->closeWriteChannel();
}

void SteamVisit::readLines() {
    while (m_process->canReadLine()) {
        takeLine(QString::fromUtf8(m_process->readLine()).trimmed());
    }
}

void SteamVisit::takeLine(const QString& line) {
    static const QString refused = QStringLiteral("refused ");
    if (line == QLatin1String("waiting")) {
        setState(State::Waiting);
    } else if (line == QLatin1String("open")) {
        setState(State::Open);
    } else if (line == QLatin1String("closing")) {
        setState(State::Closing);
    } else if (line == QLatin1String("closed")) {
        setState(State::Closed);
    } else if (line == QLatin1String("stuck")) {
        setState(State::Stuck);
    } else if (line.startsWith(refused)) {
        setState(State::NotOpened, line.mid(refused.size()));
    }
}

void SteamVisit::ended(int exitCode, QProcess::ExitStatus status) {
    readLines();
    switch (m_state) {
    case State::Asking:
        if (status == QProcess::NormalExit && exitCode == kDismissed) {
            setState(State::NotOpened, QStringLiteral("dismissed"));
        } else if (status == QProcess::NormalExit && exitCode == kNotAuthorized) {
            setState(State::NotOpened, QStringLiteral("not-authorized"));
        } else {
            setState(State::NotOpened, QStringLiteral("failed"));
        }
        break;
    case State::Waiting:
        setState(State::NotOpened, QStringLiteral("failed"));
        break;
    case State::Open:
    case State::Closing:
        // The helper ended without saying the visit closed, so the child
        // may still be refused at login until a restart.
        setState(State::Stuck);
        break;
    case State::NotStarted:
    case State::Closed:
    case State::Stuck:
    case State::NotOpened:
        break;
    }
}

void SteamVisit::notStarted(QProcess::ProcessError error) {
    if (error == QProcess::FailedToStart) {
        setState(State::NotOpened, QStringLiteral("failed"));
    }
}

void SteamVisit::setState(State state, const QString& refusal) {
    if (state == m_state && refusal == m_refusal) {
        return;
    }
    m_state = state;
    m_refusal = refusal;
    emit stateChanged();
}
