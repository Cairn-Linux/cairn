// SPDX-License-Identifier: Apache-2.0
#include "SteamForChild.h"

#include "Account.h"
#include "ScreenLockWatch.h"
#include "SteamVisit.h"

#include <QHash>

namespace {

// The child's full name, or the login name when there is none, as on the
// login screen.
QString nameOf(const QString& child) {
    const std::optional<Account> account = lookUpAccount(child);
    return account ? account->displayName() : child;
}

} // namespace

SteamForChild::SteamForChild(const Setup& setup, QObject* parent)
    : QObject(parent), m_child(setup.child), m_childName(nameOf(setup.child)),
      m_visit(new SteamVisit(setup.pkexec, setup.helper, this)),
      m_ownSteam(new OwnSteam(setup.uid, setup.ownSteam, this)),
      m_lock(new ScreenLockWatch(QDBusConnection::sessionBus(), this)),
      m_ownSteamOpen(m_ownSteam->running()) {
    connect(m_visit, &SteamVisit::stateChanged, this, &SteamForChild::visitChanged);
    connect(m_ownSteam, &OwnSteam::paused, this, &SteamForChild::ownSteamPaused);
    connect(m_lock, &ScreenLockWatch::locked, this, &SteamForChild::screenLocked);
}

QString SteamForChild::childName() const {
    return m_childName;
}

SteamForChild::Step SteamForChild::step() const {
    return m_step;
}

SteamForChild::Reason SteamForChild::reason() const {
    return m_reason;
}

bool SteamForChild::ownSteamOpen() const {
    return m_ownSteamOpen;
}

bool SteamForChild::ownSteamComingBack() const {
    return m_ownSteamComingBack;
}

void SteamForChild::open() {
    if (m_step != Step::Ready && m_step != Step::NotOpened) {
        return;
    }
    m_stopBeforeOpening = false;
    m_ownSteamComingBack = false;
    m_ownSteamOpen = m_ownSteam->running();
    emit ownSteamChanged();
    setStep(Step::ClosingOwnSteam);
    m_ownSteam->pause();
}

void SteamForChild::done() {
    if (m_step == Step::Waiting || m_step == Step::Open) {
        m_visit->close();
    }
}

bool SteamForChild::leave() {
    switch (m_step) {
    case Step::Ready:
    case Step::Closed:
    case Step::Stuck:
    case Step::NotOpened:
        return true;
    case Step::ClosingOwnSteam:
        m_stopBeforeOpening = true;
        break;
    case Step::Asking:
        m_stopBeforeOpening = true;
        m_visit->close();
        break;
    case Step::Waiting:
    case Step::Open:
    case Step::Closing:
        m_visit->close();
        break;
    }
    m_quitWhenFinished = true;
    return false;
}

void SteamForChild::screenLocked() {
    switch (m_step) {
    case Step::ClosingOwnSteam:
        m_stopBeforeOpening = true;
        break;
    case Step::Asking:
        m_stopBeforeOpening = true;
        m_visit->close();
        break;
    case Step::Waiting:
    case Step::Open:
        m_visit->close();
        break;
    case Step::Ready:
    case Step::Closing:
    case Step::Closed:
    case Step::Stuck:
    case Step::NotOpened:
        break;
    }
}

SteamForChild::Reason SteamForChild::reasonFor(const QString& refusal) {
    static const QHash<QString, Reason> reasons = {
        {QStringLiteral("busy"), Reason::Busy},
        // Provisioning is copying games into the child's library (ADR-0033).
        {QStringLiteral("copying"), Reason::Copying},
        // A grown-up is logging a child out from the Games page (ADR-0041).
        {QStringLiteral("logging-out"), Reason::LoggingOut},
        {QStringLiteral("logged-in"), Reason::LoggedIn},
        {QStringLiteral("still-running"), Reason::StillEnding},
        {QStringLiteral("not-set-up"), Reason::NotSetUp},
        {QStringLiteral("not-a-child"), Reason::NotAChild},
        {QStringLiteral("no-desktop"), Reason::NotAtTheComputer},
        // pkexec cannot tell a closed box from a wrong password.
        {QStringLiteral("dismissed"), Reason::NoPassword},
        {QStringLiteral("not-authorized"), Reason::NoPassword},
        {QStringLiteral("not-a-grown-up"), Reason::NotAllowed},
    };
    return reasons.value(refusal, Reason::Other);
}

void SteamForChild::ownSteamPaused(bool closed) {
    if (m_step != Step::ClosingOwnSteam) {
        return;
    }
    if (m_stopBeforeOpening) {
        setStep(Step::Ready);
        finish();
    } else if (!closed) {
        setStep(Step::NotOpened, Reason::OwnSteamStillOpen);
        finish();
    } else {
        m_visit->open(m_child);
    }
}

void SteamForChild::visitChanged() {
    switch (m_visit->state()) {
    case SteamVisit::State::NotStarted:
        break;
    case SteamVisit::State::Asking:
        setStep(Step::Asking);
        break;
    case SteamVisit::State::Waiting:
        setStep(Step::Waiting);
        break;
    case SteamVisit::State::Open:
        setStep(Step::Open);
        break;
    case SteamVisit::State::Closing:
        setStep(Step::Closing);
        break;
    case SteamVisit::State::Closed:
        setStep(Step::Closed);
        finish();
        break;
    case SteamVisit::State::Stuck:
        setStep(Step::Stuck);
        finish();
        break;
    case SteamVisit::State::NotOpened:
        if (m_stopBeforeOpening) {
            setStep(Step::Ready);
        } else {
            setStep(Step::NotOpened, reasonFor(m_visit->refusal()));
        }
        finish();
        break;
    }
}

// The child's Steam is closed, or never opened: the Guardian's own comes
// back. A window that was asked to close now does, unless a program of the
// child's would not end, which the Guardian needs to read about first.
void SteamForChild::finish() {
    m_ownSteamComingBack = m_ownSteam->resume();
    emit ownSteamChanged();
    if (m_quitWhenFinished && m_step != Step::Stuck) {
        emit quitRequested();
    }
}

void SteamForChild::setStep(Step step, Reason reason) {
    if (step == m_step && reason == m_reason) {
        return;
    }
    m_step = step;
    m_reason = reason;
    emit stepChanged();
}
